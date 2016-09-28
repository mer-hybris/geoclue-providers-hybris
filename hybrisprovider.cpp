/*
    Copyright (C) 2015 Jolla Ltd.
    Contact: Aaron McCarthy <aaron.mccarthy@jollamobile.com>

    This file is part of geoclue-hybris.

    Geoclue-hybris is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License.
*/

#include "hybrisprovider.h"
#include "devicecontrol.h"

#include "geoclue_adaptor.h"
#include "position_adaptor.h"
#include "velocity_adaptor.h"
#include "satellite_adaptor.h"

#include "connectiond_interface.h"
#include "connectionselector_interface.h"

#include <QtCore/QLoggingCategory>
#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QHostAddress>
#include <QtNetwork/QUdpSocket>
#include <QtNetwork/QHostInfo>
#include <QtDBus/QDBusConnection>
#include <QtDBus/QDBusMessage>

#include <networkmanager.h>
#include <networkservice.h>

#include <qofonomanager.h>
#include <qofonoconnectionmanager.h>
#include <qofonoconnectioncontext.h>

#include <qofonoextmodemmanager.h>

#include <strings.h>
#include <sys/time.h>

Q_DECLARE_METATYPE(QHostAddress)

Q_LOGGING_CATEGORY(lcGeoclueHybris, "geoclue.provider.hybris")
Q_LOGGING_CATEGORY(lcGeoclueHybrisNmea, "geoclue.provider.hybris.nmea")
Q_LOGGING_CATEGORY(lcGeoclueHybrisPosition, "geoclue.provider.hybris.position")

namespace
{

HybrisProvider *staticProvider = 0;

const int QuitIdleTime = 30000;
const int FixTimeout = 30000;
const quint32 MinimumInterval = 1000;
const quint32 PreferredAccuracy = 0;
const quint32 PreferredInitialFixTime = 0;
const double KnotsToMps = 0.514444;

const QString LocationSettingsFile = QStringLiteral("/etc/location/location.conf");
const QString LocationSettingsEnabledKey = QStringLiteral("location/enabled");
const QString LocationSettingsGpsEnabledKey = QStringLiteral("location/gps/enabled");
const QString LocationSettingsAgpsEnabledKey = QStringLiteral("location/%1/enabled");
const QString LocationSettingsAgpsOnlineEnabledKey = QStringLiteral("location/%1/online_enabled");
const QString LocationSettingsAgpsAgreementAcceptedKey = QStringLiteral("location/%1/agreement_accepted");
const QString LocationSettingsAgpsProvidersKey = QStringLiteral("location/agps_providers");
// deprecated keys
const QString LocationSettingsOldAgpsEnabledKey = QStringLiteral("location/agreement_accepted");
const QString LocationSettingsOldAgpsAgreementAcceptedKey = QStringLiteral("location/here_agreement_accepted");

void locationCallback(GpsLocation *location)
{
    Location loc;

    loc.setTimestamp(location->timestamp);

    if (location->flags & GPS_LOCATION_HAS_LAT_LONG) {
        loc.setLatitude(location->latitude);
        loc.setLongitude(location->longitude);
    }

    if (location->flags & GPS_LOCATION_HAS_ALTITUDE)
        loc.setAltitude(location->altitude);

    if (location->flags & GPS_LOCATION_HAS_SPEED)
        loc.setSpeed(location->speed / KnotsToMps);

    if (location->flags & GPS_LOCATION_HAS_BEARING)
        loc.setDirection(location->bearing);

    if (location->flags & GPS_LOCATION_HAS_ACCURACY) {
        Accuracy accuracy;
        accuracy.setHorizontal(location->accuracy);
        accuracy.setVertical(location->accuracy);
        loc.setAccuracy(accuracy);
    }

    QMetaObject::invokeMethod(staticProvider, "setLocation", Qt::QueuedConnection,
                              Q_ARG(Location, loc));
}

void statusCallback(GpsStatus *status)
{
    switch (status->status) {
    case GPS_STATUS_ENGINE_ON:
        QMetaObject::invokeMethod(staticProvider, "engineOn", Qt::QueuedConnection);
        break;
    case GPS_STATUS_ENGINE_OFF:
        QMetaObject::invokeMethod(staticProvider, "engineOff", Qt::QueuedConnection);
        break;
    default:
        ;
    }
}

void svStatusCallback(GpsSvStatus *svStatus)
{
    QList<SatelliteInfo> satellites;
    QList<int> usedPrns;

    for (int i = 0; i < svStatus->num_svs; ++i) {
        SatelliteInfo satInfo;
        GpsSvInfo &svInfo = svStatus->sv_list[i];
        satInfo.setPrn(svInfo.prn);
        satInfo.setSnr(svInfo.snr);
        satInfo.setElevation(svInfo.elevation);
        satInfo.setAzimuth(svInfo.azimuth);
        satellites.append(satInfo);

        if (svStatus->used_in_fix_mask & (1 << i))
            usedPrns.append(svInfo.prn);
    }

    QMetaObject::invokeMethod(staticProvider, "setSatellite", Qt::QueuedConnection,
                              Q_ARG(QList<SatelliteInfo>, satellites),
                              Q_ARG(QList<int>, usedPrns));
}

bool nmeaChecksumValid(const QByteArray &nmea)
{
    unsigned char checksum = 0;
    for (int i = 1; i < nmea.length(); ++i) {
        if (nmea.at(i) == '*') {
            if (nmea.length() < i+3)
                return false;

            checksum ^= nmea.mid(i+1, 2).toInt(0, 16);

            break;
        }

        checksum ^= nmea.at(i);
    }

    return checksum == 0;
}

void parseRmc(const QByteArray &nmea)
{
    QList<QByteArray> fields = nmea.split(',');
    if (fields.count() < 12)
        return;

    bool ok;
    double variation = fields.at(10).toDouble(&ok);
    if (ok) {
        if (fields.at(11) == "W")
            variation = -variation;

        QMetaObject::invokeMethod(staticProvider, "setMagneticVariation", Qt::QueuedConnection,
                                  Q_ARG(double, variation));
    }
}

void nmeaCallback(GpsUtcTime timestamp, const char *nmeaData, int length)
{
    // Trim trailing whitepsace
    while (length > 0 && isspace(nmeaData[length-1]))
        --length;

    if (length == 0)
        return;

    QByteArray nmea = QByteArray::fromRawData(nmeaData, length);

    qCDebug(lcGeoclueHybrisNmea) << timestamp << nmea;

    if (!nmeaChecksumValid(nmea))
        return;

    // truncate checksum and * from end of sentence
    nmea.truncate(nmea.length()-3);

    if (nmea.startsWith("$GPRMC"))
        parseRmc(nmea);
}

void setCapabilitiesCallback(uint32_t capabilities)
{
    qCDebug(lcGeoclueHybris) << "capabilities" << showbase << hex << capabilities;
}

void acquireWakelockCallback()
{
}

void releaseWakelockCallback()
{
}

pthread_t createThreadCallback(const char *name, void (*start)(void *), void *arg)
{
    Q_UNUSED(name)

    pthread_t threadId;

    int error = pthread_create(&threadId, 0, (void*(*)(void*))start, arg);

    return error ? 0 : threadId;
}

void requestUtcTimeCallback()
{
    qCDebug(lcGeoclueHybris);

    QMetaObject::invokeMethod(staticProvider, "injectUtcTime", Qt::QueuedConnection);
}

void agpsStatusCallback(AGpsStatus *status)
{
    QHostAddress ipv4;
    QHostAddress ipv6;
    QByteArray ssid;
    QByteArray password;

#if GEOCLUE_ANDROID_GPS_INTERFACE == 2
    if (status->addr.ss_family == AF_INET) {
        ipv4.setAddress(status->ipaddr);
    } else if (status->addr.ss_family == AF_INET6) {
        qDebug() << "IPv6 address extraction is untested";
        ipv6.setAddress(reinterpret_cast<sockaddr *>(&status->addr));
        qDebug() << "IPv6 address:" << ipv6;
    }
#elif GEOCLUE_ANDROID_GPS_INTERFACE == 1
    ipv4.setAddress(status->ipaddr);
#else
    ipv4.setAddress(status->ipv4_addr);
    ssid = QByteArray(status->ssid, SSID_BUF_SIZE);
    password = QByteArray(status->password, SSID_BUF_SIZE);
#endif

    QMetaObject::invokeMethod(staticProvider, "agpsStatus", Qt::QueuedConnection,
                              Q_ARG(qint16, status->type), Q_ARG(quint16, status->status),
                              Q_ARG(QHostAddress, ipv4), Q_ARG(QHostAddress, ipv6),
                              Q_ARG(QByteArray, ssid), Q_ARG(QByteArray, password));
}

void gpsNiNotifyCallback(GpsNiNotification *notification)
{
    Q_UNUSED(notification)
    qCDebug(lcGeoclueHybris);
}

void agpsRilRequestSetId(uint32_t flags)
{
    Q_UNUSED(flags)
    qCDebug(lcGeoclueHybris) << "flags" << showbase << hex << flags;
}

void agpsRilRequestRefLoc(uint32_t flags)
{
    Q_UNUSED(flags)
    qCDebug(lcGeoclueHybris) << "flags" << showbase << hex << flags;
}

void gpsXtraDownloadRequest()
{
    QMetaObject::invokeMethod(staticProvider, "xtraDownloadRequest", Qt::QueuedConnection);
}

#if GEOCLUE_ANDROID_GPS_INTERFACE == 2
ApnIpType fromContextProtocol(const QString &protocol)
{
    if (protocol == QLatin1String("ip"))
        return APN_IP_IPV4;
    else if (protocol == QLatin1String("ipv6"))
        return APN_IP_IPV6;
    else if (protocol == QLatin1String("dual"))
        return APN_IP_IPV4V6;
    else
        return APN_IP_INVALID;
}
#elif GEOCLUE_ANDROID_GPS_INTERFACE == 1
#else
AGpsBearerType fromContextProtocol(const QString &protocol)
{
    if (protocol == QLatin1String("ip"))
        return AGPS_APN_BEARER_IPV4;
    else if (protocol == QLatin1String("ipv6"))
        return AGPS_APN_BEARER_IPV6;
    else if (protocol == QLatin1String("dual"))
        return AGPS_APN_BEARER_IPV4V6;
    else
        return AGPS_APN_BEARER_INVALID;
}
#endif

}

GpsCallbacks gpsCallbacks = {
    sizeof(GpsCallbacks),
    locationCallback,
    statusCallback,
    svStatusCallback,
    nmeaCallback,
    setCapabilitiesCallback,
    acquireWakelockCallback,
    releaseWakelockCallback,
    createThreadCallback,
    requestUtcTimeCallback
};

AGpsCallbacks agpsCallbacks = {
    agpsStatusCallback,
    createThreadCallback
};

GpsNiCallbacks gpsNiCallbacks = {
    gpsNiNotifyCallback,
    createThreadCallback
};

AGpsRilCallbacks agpsRilCallbacks = {
    agpsRilRequestSetId,
    agpsRilRequestRefLoc,
    createThreadCallback
};

// Work-around for compatibility, the public definition of GpsXtraCallbacks has only two members,
// however, some hardware adaptation definitions contain an extra report_xtra_server_cb member.
// Add extra pointer length padding and initialise it to nullptr to prevent crashes.
struct GpsXtraCallbacksWrapper {
    GpsXtraCallbacks callbacks;
    void *padding;
} gpsXtraCallbacks = {
    { gpsXtraDownloadRequest, createThreadCallback },
    0
};


QDBusArgument &operator<<(QDBusArgument &argument, const Accuracy &accuracy)
{
    const qint32 GeoclueAccuracyLevelDetailed = 6;

    argument.beginStructure();
    argument << GeoclueAccuracyLevelDetailed << accuracy.horizontal() << accuracy.vertical();
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, Accuracy &accuracy)
{
    qint32 level;
    double a;

    argument.beginStructure();
    argument >> level;
    argument >> a;
    accuracy.setHorizontal(a);
    argument >> a;
    accuracy.setVertical(a);
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const SatelliteInfo &si)
{
    argument.beginStructure();
    argument << si.prn() << si.elevation() << si.azimuth() << si.snr();
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, SatelliteInfo &si)
{
    int a;

    argument.beginStructure();
    argument >> a;
    si.setPrn(a);
    argument >> a;
    si.setElevation(a);
    argument >> a;
    si.setAzimuth(a);
    argument >> a;
    si.setSnr(a);
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument, const QList<SatelliteInfo> &sis)
{
    argument.beginArray(qMetaTypeId<SatelliteInfo>());
    foreach (const SatelliteInfo &si, sis)
        argument << si;
    argument.endArray();

    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument, QList<SatelliteInfo> &sis)
{
    sis.clear();

    argument.beginArray();
    while (!argument.atEnd()) {
        SatelliteInfo si;
        argument >> si;
        sis.append(si);
    }
    argument.endArray();

    return argument;
}

HybrisProvider::HybrisProvider(QObject *parent)
:   QObject(parent), m_gps(0), m_agps(0), m_agpsril(0), m_gpsni(0), m_xtra(0),
    m_status(StatusUnavailable), m_positionInjectionConnected(false), m_xtraDownloadReply(0),
    m_requestedConnect(false), m_gpsStarted(false), m_deviceControl(0),
    m_networkManager(new NetworkManager(this)), m_cellularTechnology(0),
    m_ofonoExtModemManager(new QOfonoExtModemManager(this)),
    m_connectionManager(new QOfonoConnectionManager(this)), m_connectionContext(0), m_ntpSocket(0),
    m_agpsEnabled(false), m_agpsOnlineEnabled(false)
{
    if (staticProvider)
        qFatal("Only a single instance of HybrisProvider is supported.");

    qRegisterMetaType<Location>();
    qRegisterMetaType<QHostAddress>();
    qDBusRegisterMetaType<Accuracy>();
    qDBusRegisterMetaType<SatelliteInfo>();
    qDBusRegisterMetaType<QList<SatelliteInfo> >();

    staticProvider = this;

    m_locationSettings = new QFileSystemWatcher(this);
    connect(m_locationSettings, SIGNAL(fileChanged(QString)),
            this, SLOT(locationEnabledChanged()));
    m_locationSettings->addPath(LocationSettingsFile);

    new GeoclueAdaptor(this);
    new PositionAdaptor(this);
    new VelocityAdaptor(this);
    new SatelliteAdaptor(this);

    m_manager = new QNetworkAccessManager(this);

    connect(m_networkManager, SIGNAL(technologiesChanged()), this, SLOT(technologiesChanged()));

    technologiesChanged();

    connect(m_ofonoExtModemManager, SIGNAL(defaultDataModemChanged(QString)),
            this, SLOT(defaultDataModemChanged(QString)));

    connect(m_connectionManager, SIGNAL(validChanged(bool)),
            this, SLOT(connectionManagerValidChanged()));

    defaultDataModemChanged(m_ofonoExtModemManager->defaultDataModem());

    QDBusConnection connection = QDBusConnection::sessionBus();

    m_watcher = new QDBusServiceWatcher(this);
    m_watcher->setConnection(connection);
    m_watcher->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    connect(m_watcher, SIGNAL(serviceUnregistered(QString)),
            this, SLOT(serviceUnregistered(QString)));

    m_connectiond = new ComJollaConnectiondInterface(QStringLiteral("com.jolla.Connectiond"),
                                                     QStringLiteral("/Connectiond"), connection);

    m_connectionSelector = new ComJollaLipstickConnectionSelectorIfInterface(
        QStringLiteral("com.jolla.lipstick.ConnectionSelector"), QStringLiteral("/"), connection);

    if (m_watchedServices.isEmpty()) {
        m_idleTimer.start(QuitIdleTime, this);
    }

    const hw_module_t *hwModule;

    int error = hw_get_module(GPS_HARDWARE_MODULE_ID, &hwModule);
    if (error) {
        qWarning("Android GPS interface not found, error %d\n", error);
        return;
    }

    qWarning("Android GPS hardware module \"%s\" \"%s\" %u.%u\n", hwModule->id, hwModule->name,
             hwModule->module_api_version, hwModule->hal_api_version);

    error = hwModule->methods->open(hwModule, GPS_HARDWARE_MODULE_ID,
                                    reinterpret_cast<hw_device_t **>(&m_gpsDevice));
    if (error) {
        qWarning("Failed to open GPS device, error %d\n", error);
        return;
    }

    m_gps = m_gpsDevice->get_gps_interface(m_gpsDevice);
    if (!m_gps) {
        m_status = StatusError;
        return;
    }

    qWarning("Initialising GPS interface\n");
    error = m_gps->init(&gpsCallbacks);
    if (error) {
        qWarning("Failed to initialise GPS interface, error %d\n", error);
        m_status = StatusError;
        return;
    }

    m_agps = static_cast<const AGpsInterface *>(m_gps->get_extension(AGPS_INTERFACE));
    if (m_agps) {
        qWarning("Initialising AGPS Interface\n");
        m_agps->init(&agpsCallbacks);
    }

    m_gpsni = static_cast<const GpsNiInterface *>(m_gps->get_extension(GPS_NI_INTERFACE));
    if (m_gpsni) {
        qWarning("Initialising GPS NI Interface\n");
        m_gpsni->init(&gpsNiCallbacks);
    }

    m_agpsril = static_cast<const AGpsRilInterface *>(m_gps->get_extension(AGPS_RIL_INTERFACE));
    if (m_agpsril) {
        qWarning("Initialising AGPS RIL Interface\n");
        m_agpsril->init(&agpsRilCallbacks);
    }

    m_xtra = static_cast<const GpsXtraInterface *>(m_gps->get_extension(GPS_XTRA_INTERFACE));
    if (m_xtra) {
        qWarning("Initialising GPS Xtra Interface\n");
        error = m_xtra->init(&gpsXtraCallbacks.callbacks);
        if (error)
            qWarning("GPS Xtra Interface init failed, error %d\n", error);
    }

    m_debug = static_cast<const GpsDebugInterface *>(m_gps->get_extension(GPS_DEBUG_INTERFACE));
}

HybrisProvider::~HybrisProvider()
{
    if (m_gps)
        m_gps->cleanup();

    if (m_gpsDevice->common.close)
        m_gpsDevice->common.close(reinterpret_cast<hw_device_t *>(m_gpsDevice));

    if (staticProvider == this)
        staticProvider = 0;
}

void HybrisProvider::setDeviceController(DeviceControl *control)
{
    if (m_deviceControl == control)
        return;

    if (m_deviceControl) {
        disconnect(m_deviceControl, SIGNAL(poweredChanged()),
                   this, SLOT(locationEnabledChanged()));
    }

    m_deviceControl = control;

    if (m_deviceControl)
        connect(m_deviceControl, SIGNAL(poweredChanged()), this, SLOT(locationEnabledChanged()));
}

void HybrisProvider::AddReference()
{
    if (!calledFromDBus())
        qFatal("AddReference must only be called from DBus");

    bool wasInactive = m_watchedServices.isEmpty();
    const QString service = message().service();
    m_watcher->addWatchedService(service);
    m_watchedServices[service].referenceCount += 1;
    if (wasInactive) {
        qCDebug(lcGeoclueHybris) << "new watched service, stopping idle timer.";
        m_idleTimer.stop();
    }

    startPositioningIfNeeded();
}

void HybrisProvider::RemoveReference()
{
    if (!calledFromDBus())
        qFatal("RemoveReference must only be called from DBus");

    const QString service = message().service();

    if (m_watchedServices[service].referenceCount > 0)
        m_watchedServices[service].referenceCount -= 1;

    if (m_watchedServices[service].referenceCount == 0) {
        m_watcher->removeWatchedService(service);
        m_watchedServices.remove(service);
    }

    if (m_watchedServices.isEmpty()) {
        qCDebug(lcGeoclueHybris) << "no watched services, starting idle timer.";
        m_idleTimer.start(QuitIdleTime, this);
    }

    stopPositioningIfNeeded();
}

QString HybrisProvider::GetProviderInfo(QString &description)
{
    description = tr("Android GPS provider");
    return QLatin1String("Hybris");
}

int HybrisProvider::GetStatus()
{
    return m_status;
}

void HybrisProvider::SetOptions(const QVariantMap &options)
{
    if (!calledFromDBus())
        qFatal("SetOptions must only be called from DBus");

    const QString service = message().service();
    if (!m_watchedServices.contains(service)) {
        qWarning("Only active users can call SetOptions");
        return;
    }

    if (options.contains(QStringLiteral("UpdateInterval"))) {
        m_watchedServices[service].updateInterval =
            options.value(QStringLiteral("UpdateInterval")).toUInt();

        quint32 updateInterval = minimumRequestedUpdateInterval();

        int error = m_gps->set_position_mode(m_agpsEnabled ? GPS_POSITION_MODE_MS_BASED
                                                           : GPS_POSITION_MODE_STANDALONE,
                                             GPS_POSITION_RECURRENCE_PERIODIC, updateInterval,
                                             PreferredAccuracy, PreferredInitialFixTime);
        if (error) {
            qWarning("While updating the updateInterval, failed to set position mode, error %d\n", error);
        }
    }
}

int HybrisProvider::GetPosition(int &timestamp, double &latitude, double &longitude,
                                double &altitude, Accuracy &accuracy)
{
    PositionFields positionFields = NoPositionFields;

    timestamp = m_currentLocation.timestamp() / 1000;
    if (!qIsNaN(m_currentLocation.latitude()))
        positionFields |= LatitudePresent;
    latitude = m_currentLocation.latitude();
    if (!qIsNaN(m_currentLocation.longitude()))
        positionFields |= LongitudePresent;
    longitude = m_currentLocation.longitude();
    if (!qIsNaN(m_currentLocation.altitude()))
        positionFields |= AltitudePresent;
    altitude = m_currentLocation.altitude();
    accuracy = m_currentLocation.accuracy();

    return positionFields;
}

int HybrisProvider::GetVelocity(int &timestamp, double &speed, double &direction, double &climb)
{
    VelocityFields velocityFields = NoVelocityFields;

    timestamp = m_currentLocation.timestamp() / 1000;
    if (!qIsNaN(m_currentLocation.speed()))
        velocityFields |= SpeedPresent;
    speed = m_currentLocation.speed();
    if (!qIsNaN(m_currentLocation.direction()))
        velocityFields |= DirectionPresent;
    direction = m_currentLocation.direction();
    if (!qIsNaN(m_currentLocation.climb()))
        velocityFields |= ClimbPresent;
    climb = m_currentLocation.climb();

    return velocityFields;
}

int HybrisProvider::GetLastSatellite(int &satelliteUsed, int &satelliteVisible,
                                     QList<int> &usedPrn, QList<SatelliteInfo> &satInfo)
{
    satelliteUsed = m_previousUsedPrns.length();
    satelliteVisible = m_previousVisibleSatellites.length();
    usedPrn = m_previousUsedPrns;
    satInfo = m_previousVisibleSatellites;

    return m_previousSatelliteTimestamp;
}

int HybrisProvider::GetSatellite(int &satelliteUsed, int &satelliteVisible, QList<int> &usedPrn,
                                 QList<SatelliteInfo> &satInfo)
{
    satelliteUsed = m_usedPrns.length();
    satelliteVisible = m_visibleSatellites.length();
    usedPrn = m_usedPrns;
    satInfo = m_visibleSatellites;

    return m_satelliteTimestamp;
}

void HybrisProvider::timerEvent(QTimerEvent *event)
{
    if (event->timerId() == m_idleTimer.timerId()) {
        m_idleTimer.stop();
        qCDebug(lcGeoclueHybris) << "have been idle for too long, quitting";
        qApp->quit();
    } else if (event->timerId() == m_fixLostTimer.timerId()) {
        m_fixLostTimer.stop();
        setStatus(StatusAcquiring);
    } else if (event->timerId() == m_ntpRetryTimer.timerId()) {
        sendNtpRequest();
    } else {
        QObject::timerEvent(event);
    }
}

void HybrisProvider::setLocation(const Location &location)
{
    qCDebug(lcGeoclueHybrisPosition) << location.timestamp() << location.latitude()
                                     << location.longitude() << location.altitude();

    // Stop listening to all PositionChanged signals from org.freedesktop.Geoclue.Position
    // interfaces.
    if (m_positionInjectionConnected) {
        QDBusConnection conn = QDBusConnection::sessionBus();
        conn.disconnect(QString(), QString(), QStringLiteral("org.freedesktop.Geoclue.Position"),
                        QStringLiteral("PositionChanged"),
                        this, SLOT(injectPosition(int,int,double,double,double,Accuracy)));
        m_positionInjectionConnected = false;
    }

    if (location.timestamp() != 0) {
        setStatus(StatusAvailable);
        m_fixLostTimer.start(FixTimeout, this);
    }

    m_currentLocation = location;
    emitLocationChanged();
}

void HybrisProvider::setSatellite(const QList<SatelliteInfo> &satellites, const QList<int> &used)
{
    m_previousSatelliteTimestamp = m_satelliteTimestamp;
    m_previousVisibleSatellites = m_visibleSatellites;
    m_previousUsedPrns = m_usedPrns;

    m_satelliteTimestamp = QDateTime::currentMSecsSinceEpoch();
    m_visibleSatellites = satellites;
    m_usedPrns = used;
    emitSatelliteChanged();
}

void HybrisProvider::serviceUnregistered(const QString &service)
{
    m_watchedServices.remove(service);
    m_watcher->removeWatchedService(service);

    if (m_watchedServices.isEmpty()) {
        qCDebug(lcGeoclueHybris) << "no watched services, starting idle timer.";
        m_idleTimer.start(QuitIdleTime, this);
    }

    stopPositioningIfNeeded();
}

void HybrisProvider::locationEnabledChanged()
{
    if (positioningEnabled()) {
        startPositioningIfNeeded();
    } else {
        setLocation(Location());
        stopPositioningIfNeeded();
    }
}

void HybrisProvider::injectPosition(int fields, int timestamp, double latitude, double longitude,
                                    double altitude, const Accuracy &accuracy)
{
    Q_UNUSED(timestamp)
    Q_UNUSED(altitude)

    PositionFields positionFields = static_cast<PositionFields>(fields);
    if (!(positionFields & LatitudePresent && positionFields & LongitudePresent))
        return;

    qCDebug(lcGeoclueHybris) << fields << timestamp << latitude << longitude << altitude
                             << accuracy.horizontal() << accuracy.vertical();

    m_gps->inject_location(latitude, longitude, accuracy.horizontal());
}

void HybrisProvider::injectUtcTime()
{
    qCDebug(lcGeoclueHybris) << "Time injection requested";

    NetworkService *service = m_networkManager->defaultRoute();
    if (!service) {
        qCDebug(lcGeoclueHybris) << "No default network service";
        return;
    }

    m_ntpServers = service->timeservers();
    if (m_ntpServers.isEmpty()) {
        qCDebug(lcGeoclueHybris) << service->name() << "doesn't advertise time servers";
        return;
    } else {
        qCDebug(lcGeoclueHybris) << "Available time servers:" << m_ntpServers;
    }

    if (!m_ntpSocket) {
        m_ntpSocket = new QUdpSocket(this);
        connect(m_ntpSocket, SIGNAL(readyRead()), this, SLOT(handleNtpResponse()));
        m_ntpSocket->bind();
    }

    m_ntpRetryTimer.start(10000, this);

    sendNtpRequest();
}

struct NtpShort {
    quint16 seconds;
    quint16 fraction;
} __attribute__ ((packed));

#define SECONDS_FROM_1900_TO_1970 2208988800UL

struct NtpTime {
    quint32 seconds;
    quint32 fraction;

    void set(const timeval &time) {
        // seconds since 1900
        seconds = qToBigEndian<quint32>(time.tv_sec + SECONDS_FROM_1900_TO_1970);
        // fraction of a second
        fraction = qToBigEndian<quint32>(time.tv_usec * 1000);
    }

    qint64 toMSecsSinceEpoc() const {
        qint64 msec = qint64(qFromBigEndian<quint32>(seconds) - SECONDS_FROM_1900_TO_1970) * 1000 +
                      1000 * qFromBigEndian<quint32>(fraction) / std::numeric_limits<quint32>::max();
        return msec;
    }

} __attribute__ ((packed));

struct NtpMessage {
    quint8 flags;
    quint8 stratum;
    qint8 poll;
    qint8 precision;
    NtpShort rootDelay;
    NtpShort rootDispersion;
    quint32 referenceId;
    NtpTime referenceTimestamp;
    NtpTime originTimestamp;
    NtpTime receiveTimestamp;
    NtpTime transmitTimestamp;
} __attribute__ ((packed));

void HybrisProvider::sendNtpRequest(const QHostInfo &host)
{
    if (host.error() != QHostInfo::NoError)
        return;

    if (host.addresses().isEmpty())
        return;

    qCDebug(lcGeoclueHybris) << "Sending NTP request to" << host.addresses().first();

    QHostAddress address = host.addresses().first();

    NtpMessage request;
    bzero(&request, sizeof(NtpMessage));

    // client mode (3) and version (3)
    request.flags = 3 | (3 << 3);

    timeval ntpRequestTime;
    gettimeofday(&ntpRequestTime, 0);

    timespec ticks;
    clock_gettime(CLOCK_MONOTONIC, &ticks);
    m_ntpRequestTicks = 1000*ticks.tv_sec + ticks.tv_nsec/1000000;

    request.transmitTimestamp.set(ntpRequestTime);
    m_ntpRequestTime = request.transmitTimestamp.toMSecsSinceEpoc();

    m_ntpSocket->writeDatagram(reinterpret_cast<const char *>(&request), sizeof(NtpMessage),
                               address, 123);
}

void HybrisProvider::handleNtpResponse()
{
    while (m_ntpSocket->hasPendingDatagrams()) {
        NtpMessage response;

        timespec ticks;
        clock_gettime(CLOCK_MONOTONIC, &ticks);
        qint64 ntpResponseTicks = 1000*ticks.tv_sec + ticks.tv_nsec/1000000;

        QHostAddress host;
        quint16 port;

        m_ntpSocket->readDatagram(reinterpret_cast<char *>(&response), sizeof(NtpMessage), &host, &port);

        qCDebug(lcGeoclueHybris) << "Got NTP response from" << host << port;

        qint64 responseTime = m_ntpRequestTime + (ntpResponseTicks - m_ntpRequestTicks);
        qint64 transmitTime = response.transmitTimestamp.toMSecsSinceEpoc();
        qint64 receiveTime = response.receiveTimestamp.toMSecsSinceEpoc();
        qint64 originTime = response.originTimestamp.toMSecsSinceEpoc();
        qint64 clockOffset = ((receiveTime - originTime) + (transmitTime - responseTime))/2;

        qint64 time = responseTime + clockOffset;
        qint64 reference = ntpResponseTicks;
        int certainty = (ntpResponseTicks - m_ntpRequestTicks - (transmitTime - receiveTime)) / 2;

        qCDebug(lcGeoclueHybris) << "Injecting time" << time << reference << certainty;

        m_gps->inject_time(time, reference, certainty);

        m_ntpRetryTimer.stop();
    }
}

void HybrisProvider::xtraDownloadRequest()
{
    if (!m_agpsOnlineEnabled)
        return;

    if (m_xtraDownloadReply)
        return;

    qCDebug(lcGeoclueHybris) << "xtra download requested";

    QFile gpsConf(QStringLiteral("/system/etc/gps.conf"));
    if (!gpsConf.open(QIODevice::ReadOnly))
        return;

    while (!gpsConf.atEnd()) {
        const QByteArray line = gpsConf.readLine().trimmed();
        if (line.startsWith('#'))
            continue;

        const QList<QByteArray> split = line.split('=');
        if (split.length() != 2)
            continue;

        const QByteArray key = split.at(0).trimmed();
        if (key == "XTRA_SERVER_1" || key == "XTRA_SERVER_2" || key == "XTRA_SERVER_3")
            m_xtraServers.enqueue(QUrl::fromEncoded(split.at(1).trimmed()));
    }

    xtraDownloadRequestSendNext();
}

void HybrisProvider::xtraDownloadRequestSendNext()
{
    if (m_xtraServers.isEmpty())
        return;

    qCDebug(lcGeoclueHybris) << m_xtraServers;

    m_xtraDownloadReply = m_manager->get(QNetworkRequest(m_xtraServers.dequeue()));
    connect(m_xtraDownloadReply, SIGNAL(finished()), this, SLOT(xtraDownloadFinished()));
}

void HybrisProvider::xtraDownloadFinished()
{
    if (!m_xtraDownloadReply)
        return;

    qCDebug(lcGeoclueHybris);

    m_xtraDownloadReply->deleteLater();

    if (m_xtraDownloadReply->error() != QNetworkReply::NoError) {
        qCDebug(lcGeoclueHybris) << "Error:" << m_xtraDownloadReply->error()
                                 << m_xtraDownloadReply->errorString();

        m_xtraDownloadReply = 0;

        // Try next server
        xtraDownloadRequestSendNext();
    } else {
        QByteArray xtraData = m_xtraDownloadReply->readAll();
        m_xtra->inject_xtra_data(xtraData.data(), xtraData.length());

        m_xtraDownloadReply = 0;

        m_xtraServers.clear();
    }
}

void HybrisProvider::agpsStatus(qint16 type, quint16 status, const QHostAddress &ipv4,
                                const QHostAddress &ipv6, const QByteArray &ssid,
                                const QByteArray &password)
{
    // TODO: It these get in use later please check also agpsStatusCallback for giving
    // proper values.
    Q_UNUSED(ipv4)
    Q_UNUSED(ipv6)
    Q_UNUSED(ssid)
    Q_UNUSED(password)

    qCDebug(lcGeoclueHybris) << "type:" << type << "status:" << status;

    if (!m_agpsEnabled) {
#if GEOCLUE_ANDROID_GPS_INTERFACE == 2 || GEOCLUE_ANDROID_GPS_INTERFACE == 1
        m_agps->data_conn_failed();
#else
        m_agps->data_conn_failed(AGPS_TYPE_SUPL);
#endif
        return;
    }

    if (type != AGPS_TYPE_SUPL) {
        qWarning("Only SUPL AGPS is supported.");
        return;
    }

    switch (status) {
    case GPS_REQUEST_AGPS_DATA_CONN:
        startDataConnection();
        break;
    case GPS_RELEASE_AGPS_DATA_CONN:
        // Immediately inform that connection is closed.
#if GEOCLUE_ANDROID_GPS_INTERFACE == 2 || GEOCLUE_ANDROID_GPS_INTERFACE == 1
        m_agps->data_conn_closed();
#else
        m_agps->data_conn_closed(AGPS_TYPE_SUPL);
#endif
        stopDataConnection();
        break;
    case GPS_AGPS_DATA_CONNECTED:
        break;
    case GPS_AGPS_DATA_CONN_DONE:
        break;
    case GPS_AGPS_DATA_CONN_FAILED:
        break;
    default:
        qWarning("Unknown AGPS Status.");
    }
}

void HybrisProvider::dataServiceConnected()
{
    qCDebug(lcGeoclueHybris);

    if (!m_agpsOnlineEnabled)
        return;

    QVector<NetworkService*> services = m_networkManager->getServices(QStringLiteral("cellular"));
    Q_FOREACH (NetworkService *service, services) {
        if (!service->connected())
            continue;

        qCDebug(lcGeoclueHybris) << "Connected to" << service->name();
        m_agpsInterface = service->ethernet().value(QStringLiteral("Interface")).toString();
        if (!m_agpsInterface.isEmpty()) {
            m_networkServicePath = service->path();
            processConnectionContexts();
            return;
        }

        qWarning("Network service does not have a network interface associated with it.");
    }

    qWarning("No connected cellular network service found.");
}

void HybrisProvider::connectionErrorReported(const QString &path, const QString &error)
{
    qCDebug(lcGeoclueHybris) << path << error;

    if (path.contains(QStringLiteral("cellular")))
#if GEOCLUE_ANDROID_GPS_INTERFACE == 2 || GEOCLUE_ANDROID_GPS_INTERFACE == 1
        m_agps->data_conn_failed();
#else
        m_agps->data_conn_failed(AGPS_TYPE_SUPL);
#endif
}

void HybrisProvider::connectionSelected(bool selected)
{
    if (!selected) {
        qCDebug(lcGeoclueHybris) << "User aborted mobile data connection";

#if GEOCLUE_ANDROID_GPS_INTERFACE == 2 || GEOCLUE_ANDROID_GPS_INTERFACE == 1
        m_agps->data_conn_failed();
#else
        m_agps->data_conn_failed(AGPS_TYPE_SUPL);
#endif
    }
}

void HybrisProvider::setMagneticVariation(double variation)
{
    if (m_magneticVariation != variation) {
        qCDebug(lcGeoclueHybris) << "Updating magnetic variation to" << variation;

        QDBusMessage msg = QDBusMessage::createMethodCall(QStringLiteral("com.nokia.SensorService"),
                                                          QStringLiteral("/SensorManager"),
                                                          QStringLiteral("local.SensorManager"),
                                                          QStringLiteral("setMagneticDeviation"));
        msg.setArguments(QList<QVariant>() << variation);
        QDBusConnection::systemBus().call(msg, QDBus::NoBlock);
        m_magneticVariation = variation;
    }
}

void HybrisProvider::engineOn()
{
    qCDebug(lcGeoclueHybris);

    // The GPS is being turned back on because a position update is required soon.

    // I would like to set the status to StatusAcquiring here, but that would cause Geoclue Master
    // to switch to using another provider. Instead start the fix lost timer.
    if (m_status != StatusAvailable)
        setStatus(StatusAcquiring);

    m_fixLostTimer.start(FixTimeout, this);
}

void HybrisProvider::engineOff()
{
    qCDebug(lcGeoclueHybris);

    // The GPS is being turned off because a position update is not required for a while.

    // I would like to set the status to StatusUnavailable here, but that would cause Geoclue
    // Master to switch to using another provider.

    m_fixLostTimer.stop();
}

void HybrisProvider::technologiesChanged()
{
    if (m_cellularTechnology) {
        disconnect(m_cellularTechnology, SIGNAL(connectedChanged(bool)),
                   this, SLOT(cellularConnected(bool)));
    }

    m_cellularTechnology = m_networkManager->getTechnology(QStringLiteral("cellular"));

    if (m_cellularTechnology) {
        qCDebug(lcGeoclueHybris) << "Cellular technology available and"
                                 << (m_cellularTechnology->connected() ? "connected" : "not connected");
        connect(m_cellularTechnology, SIGNAL(connectedChanged(bool)),
                this, SLOT(cellularConnected(bool)));
    } else {
        qCDebug(lcGeoclueHybris) << "Cellular technology not available";
    }
}

void HybrisProvider::defaultDataModemChanged(const QString &modem)
{
    qCDebug(lcGeoclueHybris) << "Default data modem changed to" << modem;

    m_connectionManager->setModemPath(modem);
}

void HybrisProvider::connectionManagerValidChanged()
{
    qCDebug(lcGeoclueHybris);

    if (m_agpsOnlineEnabled && !m_agpsInterface.isEmpty())
        processConnectionContexts();
}

void HybrisProvider::connectionContextValidChanged()
{
    qCDebug(lcGeoclueHybris);

    if (!m_agpsOnlineEnabled)
        return;

    if (m_connectionContext->isValid() &&
        m_connectionContext->settings().value(QStringLiteral("Interface")) == m_agpsInterface) {
        const QByteArray apn = m_connectionContext->accessPointName().toLocal8Bit();
        const QString protocol = m_connectionContext->protocol();

        qCDebug(lcGeoclueHybris) << "Found connection context APN" << apn;

        m_agpsInterface.clear();
        m_connectionContext->deleteLater();
        m_connectionContext = 0;

#if GEOCLUE_ANDROID_GPS_INTERFACE == 2
        if (m_agps->data_conn_open_with_apn_ip_type)
            m_agps->data_conn_open_with_apn_ip_type(apn.constData(), fromContextProtocol(protocol));
        else
            m_agps->data_conn_open(apn.constData());
#elif GEOCLUE_ANDROID_GPS_INTERFACE == 1
        m_agps->data_conn_open(apn.constData());
#else
        m_agps->data_conn_open(AGPS_TYPE_SUPL, apn.constData(), fromContextProtocol(protocol));
#endif
    } else {
        processNextConnectionContext();
    }
}

void HybrisProvider::cellularConnected(bool connected)
{
    qCDebug(lcGeoclueHybris) << connected;
    if (connected)
        dataServiceConnected();
}

void HybrisProvider::emitLocationChanged()
{
    PositionFields positionFields = NoPositionFields;

    if (!qIsNaN(m_currentLocation.latitude()))
        positionFields |= LatitudePresent;
    if (!qIsNaN(m_currentLocation.longitude()))
        positionFields |= LongitudePresent;
    if (!qIsNaN(m_currentLocation.altitude()))
        positionFields |= AltitudePresent;

    VelocityFields velocityFields = NoVelocityFields;

    if (!qIsNaN(m_currentLocation.speed()))
        velocityFields |= SpeedPresent;
    if (!qIsNaN(m_currentLocation.direction()))
        velocityFields |= DirectionPresent;
    if (!qIsNaN(m_currentLocation.climb()))
        velocityFields |= ClimbPresent;

    emit VelocityChanged(velocityFields, m_currentLocation.timestamp() / 1000,
                         m_currentLocation.speed(), m_currentLocation.direction(),
                         m_currentLocation.climb());

    emit PositionChanged(positionFields, m_currentLocation.timestamp() / 1000,
                         m_currentLocation.latitude(), m_currentLocation.longitude(),
                         m_currentLocation.altitude(), m_currentLocation.accuracy());
}

void HybrisProvider::emitSatelliteChanged()
{
    emit SatelliteChanged(m_satelliteTimestamp, m_usedPrns.length(), m_visibleSatellites.length(),
                          m_usedPrns, m_visibleSatellites);
}

void HybrisProvider::startPositioningIfNeeded()
{
    // Positioning is already started.
    if (m_gpsStarted)
        return;

    // Positioning is unused.
    if (m_watchedServices.isEmpty())
        return;

    // Positioning disabled externally
    if (!positioningEnabled())
        return;

    m_idleTimer.stop();

    if (!m_gps)
        return;

    // Listen to all PositionChanged signals from org.freedesktop.Geoclue.Position interfaces. Used
    // to inject the current position to achieve a faster fix.
    if (!m_positionInjectionConnected) {
        QDBusConnection conn = QDBusConnection::sessionBus();
        m_positionInjectionConnected =
            conn.connect(QString(), QString(), QStringLiteral("org.freedesktop.Geoclue.Position"),
                         QStringLiteral("PositionChanged"),
                         this, SLOT(injectPosition(int,int,double,double,double,Accuracy)));
    }

    int error = m_gps->set_position_mode(m_agpsEnabled ? GPS_POSITION_MODE_MS_BASED
                                                       : GPS_POSITION_MODE_STANDALONE,
                                         GPS_POSITION_RECURRENCE_PERIODIC,
                                         minimumRequestedUpdateInterval(),
                                         PreferredAccuracy, PreferredInitialFixTime);
    if (error) {
        qWarning("Failed to set position mode, error %d\n", error);
        setStatus(StatusError);
        return;
    }

    qCDebug(lcGeoclueHybris) << "Starting positioning";

    error = m_gps->start();
    if (error) {
        qWarning("Failed to start positioning, error %d\n", error);
        setStatus(StatusError);
        return;
    }

    m_gpsStarted = true;
}

void HybrisProvider::stopPositioningIfNeeded()
{
    // Positioning is already stopped.
    if (!m_gpsStarted)
        return;

    // Positioning enabled externally and positioning is still being used.
    if (positioningEnabled() && !m_watchedServices.isEmpty())
        return;

    // Stop listening to all PositionChanged signals from org.freedesktop.Geoclue.Position
    // interfaces.
    if (m_positionInjectionConnected) {
        QDBusConnection conn = QDBusConnection::sessionBus();
        conn.disconnect(QString(), QString(), QStringLiteral("org.freedesktop.Geoclue.Position"),
                        QStringLiteral("PositionChanged"),
                        this, SLOT(injectPosition(int,int,double,double,double,Accuracy)));
        m_positionInjectionConnected = false;
    }

    if (m_gps) {
        qCDebug(lcGeoclueHybris) << "Stopping positioning";
        int error = m_gps->stop();
        if (error)
            qWarning("Failed to stop positioning, error %d\n", error);
        m_gpsStarted = false;
        setStatus(StatusUnavailable);
    }

    m_fixLostTimer.stop();
}

void HybrisProvider::setStatus(HybrisProvider::Status status)
{
    if (m_status == status)
        return;

    m_status = status;
    emit StatusChanged(m_status);
}

/*
    Returns true if positioning is enabled, otherwise returns false.

    Currently checks the state of the Location enabled setting and flight mode.
*/
bool HybrisProvider::positioningEnabled()
{
    QSettings settings(LocationSettingsFile, QSettings::IniFormat);

    // check the keys related to agps enablement.  We can have multiple agps providers.
    bool agpsAgreementAccepted = false;
    bool agpsEnabled = false;
    bool agpsOnlineEnabled = false;
    QString agpsProviders = settings.value(LocationSettingsAgpsProvidersKey, QStringLiteral("here")).toString();
    Q_FOREACH (const QString &agpsProvider, agpsProviders.split(',', QString::SkipEmptyParts)) {
        agpsAgreementAccepted = settings.value(LocationSettingsAgpsAgreementAcceptedKey.arg(agpsProvider), false).toBool();
        agpsEnabled = settings.value(LocationSettingsAgpsEnabledKey.arg(agpsProvider), false).toBool();
        agpsOnlineEnabled = settings.value(LocationSettingsAgpsOnlineEnabledKey.arg(agpsProvider), false).toBool();
        if (agpsAgreementAccepted && agpsEnabled && agpsOnlineEnabled) {
            break;
        }
    }
    // check the deprecated keys, also:
    bool oldAgpsAgreementAccepted = settings.value(LocationSettingsOldAgpsAgreementAcceptedKey, false).toBool();
    bool oldAgpsEnabled = settings.value(LocationSettingsOldAgpsEnabledKey, false).toBool();
    m_agpsEnabled = (agpsAgreementAccepted || oldAgpsAgreementAccepted) && (agpsEnabled || oldAgpsEnabled);
    m_agpsOnlineEnabled = agpsOnlineEnabled || (oldAgpsAgreementAccepted && oldAgpsEnabled);

    // check the keys related to the location and gps enablement, plus gps power state
    bool locationEnabled = settings.value(LocationSettingsEnabledKey, false).toBool();
    bool gpsEnabled = settings.value(LocationSettingsGpsEnabledKey, true).toBool(); // defaults to true if no key exists but location is enabled.
    bool powered = m_deviceControl->powered();
    return locationEnabled && gpsEnabled && powered;
}

quint32 HybrisProvider::minimumRequestedUpdateInterval() const
{
    quint32 updateInterval = UINT_MAX;

    foreach (const ServiceData &data, m_watchedServices) {
        // Old data, service not currently using positioning.
        if (data.referenceCount <= 0) {
            qWarning("Service data was not removed!");
            continue;
        }

        // Service hasn't requested a specific update interval.
        if (data.updateInterval == 0)
            continue;

        updateInterval = qMin(updateInterval, data.updateInterval);
    }

    if (updateInterval == UINT_MAX)
        return MinimumInterval;

    return qMax(updateInterval, MinimumInterval);
}

void HybrisProvider::startDataConnection()
{
    qCDebug(lcGeoclueHybris);

    if (!m_agpsOnlineEnabled) {
        qCDebug(lcGeoclueHybris) << "Online aGPS not enabled, not starting data connection.";
#if GEOCLUE_ANDROID_GPS_INTERFACE == 2 || GEOCLUE_ANDROID_GPS_INTERFACE == 1
        m_agps->data_conn_failed();
#else
        m_agps->data_conn_failed(AGPS_TYPE_SUPL);
#endif
        return;
    }

    // Check if existing cellular network service is connected
    NetworkTechnology *technology = m_networkManager->getTechnology(QStringLiteral("cellular"));
    if (technology && technology->connected()) {
        qCDebug(lcGeoclueHybris) << technology << technology->type()
                                 << (technology->connected() ? "connected" : "not connected")
                                 << (technology->powered() ? "powered" : "not powered");
        dataServiceConnected();
        return;
    }

    // No data connection, ask connection agent to connect to a cellular service
    connect(m_connectiond, SIGNAL(errorReported(QString,QString)),
            this, SLOT(connectionErrorReported(QString,QString)));
    connect(m_connectionSelector, SIGNAL(connectionSelectorClosed(bool)),
            this, SLOT(connectionSelected(bool)));
    m_connectiond->connectToType(QStringLiteral("cellular"));

    m_requestedConnect = true;
}

void HybrisProvider::stopDataConnection()
{
    qCDebug(lcGeoclueHybris);

    if (!m_requestedConnect)
        return;

    disconnect(m_connectiond, SIGNAL(errorReported(QString,QString)),
               this, SLOT(connectionErrorReported(QString,QString)));
    disconnect(m_connectionSelector, SIGNAL(connectionSelectorClosed(bool)),
               this, SLOT(connectionSelected(bool)));
    m_requestedConnect = false;

    if (!m_networkServicePath.isEmpty()) {
        NetworkService service;
        service.setPath(m_networkServicePath);
        service.requestDisconnect();
        m_networkServicePath.clear();
    }
}

void HybrisProvider::sendNtpRequest()
{
    qCDebug(lcGeoclueHybris) << m_ntpServers;

    if (!m_agpsOnlineEnabled) {
        qCDebug(lcGeoclueHybris) << "Online aGPS not enabled, not sending NTP request.";
        return;
    }

    if (m_ntpServers.isEmpty()) {
        qCDebug(lcGeoclueHybris) << "No NTP servers known, not sending NTP request.";
        return;
    }

    QString server = m_ntpServers.takeFirst();
    QHostInfo::lookupHost(server, this, SLOT(sendNtpRequest(QHostInfo)));

    if (m_ntpServers.isEmpty())
        m_ntpRetryTimer.stop();
}

void HybrisProvider::processConnectionContexts()
{
    if (!m_connectionManager->isValid()) {
        qCDebug(lcGeoclueHybris) << "Connection manager is not yet valid.";
        return;
    }

    m_connectionContexts = m_connectionManager->contexts();
    processNextConnectionContext();
}

void HybrisProvider::processNextConnectionContext()
{
    qCDebug(lcGeoclueHybris) << "Remaining connection contexts to check:" << m_connectionContexts;

    if (m_connectionContexts.isEmpty()) {
        qWarning("Could not determine APN for active cellular connection.");

        m_agpsInterface.clear();
        delete m_connectionContext;
        m_connectionContext = 0;

#if GEOCLUE_ANDROID_GPS_INTERFACE == 2 || GEOCLUE_ANDROID_GPS_INTERFACE == 1
        m_agps->data_conn_failed();
#else
        m_agps->data_conn_failed(AGPS_TYPE_SUPL);
#endif

        return;
    }

    if (!m_connectionContext) {
        m_connectionContext = new QOfonoConnectionContext(this);
        connect(m_connectionContext, SIGNAL(validChanged(bool)),
                this, SLOT(connectionContextValidChanged()));
    }

    const QString contextPath = m_connectionContexts.takeFirst();
    qCDebug(lcGeoclueHybris) << "Getting APN for" << contextPath;
    m_connectionContext->setContextPath(contextPath);
}
