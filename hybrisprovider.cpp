/*
    Copyright (C) 2013 Jolla Ltd.
    Contact: Aaron McCarthy <aaron.mccarthy@jollamobile.com>
*/

#include "hybrisprovider.h"
#include "geoclue_adaptor.h"
#include "position_adaptor.h"
#include "velocity_adaptor.h"
#include "satellite_adaptor.h"

#include <QtNetwork/QNetworkAccessManager>
#include <QtNetwork/QNetworkReply>
#include <QtNetwork/QHostAddress>

#include <networkmanager.h>
#include <networkservice.h>

#include <qofonomanager.h>
#include <qofonoconnectionmanager.h>
#include <qofonoconnectioncontext.h>

#include <mlite5/MGConfItem>
#include <contextproperty.h>

Q_DECLARE_METATYPE(QHostAddress)

namespace
{

HybrisProvider *staticProvider = 0;

const int QuitIdleTime = 30000;
const quint32 MinimumInterval = 1000;
const quint32 PreferredAccuracy = 0;
const quint32 PreferredInitialFixTime = 0;

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
        loc.setSpeed(location->speed);

    if (location->flags & GPS_LOCATION_HAS_BEARING)
        loc.setDirection(location->bearing);

    if (location->flags & GPS_LOCATION_HAS_ACCURACY) {
        Accuracy accuracy;
        accuracy.setHorizontal(location->accuracy);
        accuracy.setVertical(location->accuracy);
        loc.setAccuracy(accuracy);
    }

    QMetaObject::invokeMethod(staticProvider, "setLocation", Q_ARG(Location, loc));
}

void statusCallback(GpsStatus *status)
{
    Q_UNUSED(status)
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

    QMetaObject::invokeMethod(staticProvider, "setSatellite",
                              Q_ARG(QList<SatelliteInfo>, satellites),
                              Q_ARG(QList<int>, usedPrns));
}

void nmeaCallback(GpsUtcTime timestamp, const char *nmea, int length)
{
    Q_UNUSED(timestamp)
    Q_UNUSED(nmea)
    Q_UNUSED(length)
}

void setCapabilitiesCallback(uint32_t capabilities)
{
    Q_UNUSED(capabilities)
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
    pthread_attr_t attr;

    int error = pthread_attr_init(&attr);
    error = pthread_create(&threadId, &attr, (void*(*)(void*))start, arg);

    if (error)
      return 0;

    return threadId;
}

void requestUtcTimeCallback()
{
    QMetaObject::invokeMethod(staticProvider, "injectUtcTime");
}

void agpsStatusCallback(AGpsStatus *status)
{
    QHostAddress ipv4(status->ipv4_addr);
    QHostAddress ipv6;
    QByteArray ssid(status->ssid, SSID_BUF_SIZE);
    QByteArray password(status->password, SSID_BUF_SIZE);

    QMetaObject::invokeMethod(staticProvider, "agpsStatus", Q_ARG(qint16, status->type),
                              Q_ARG(quint16, status->status), Q_ARG(QHostAddress, ipv4),
                              Q_ARG(QHostAddress, ipv6), Q_ARG(QByteArray, ssid),
                              Q_ARG(QByteArray, password));
}

void gpsNiNotifyCallback(GpsNiNotification *notification)
{
    Q_UNUSED(notification)
}

void agpsRilRequestSetId(uint32_t flags)
{
    Q_UNUSED(flags)
}

void agpsRilRequestRefLoc(uint32_t flags)
{
    Q_UNUSED(flags)
}

void gpsXtraDownloadRequest()
{
    QMetaObject::invokeMethod(staticProvider, "xtraDownloadRequest");
}

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

GpsXtraCallbacks gpsXtraCallbacks = {
    gpsXtraDownloadRequest,
    createThreadCallback
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
    m_networkManager(new NetworkManager(this)), m_networkService(0), m_requestedConnect(false)
{
    if (staticProvider)
        qFatal("Only a single instance of HybrisProvider is supported.");

    qRegisterMetaType<Location>();
    qRegisterMetaType<QHostAddress>();
    qDBusRegisterMetaType<Accuracy>();
    qDBusRegisterMetaType<SatelliteInfo>();
    qDBusRegisterMetaType<QList<SatelliteInfo> >();

    staticProvider = this;

    m_locationEnabled = new MGConfItem(QStringLiteral("/jolla/location/enabled"), this);
    connect(m_locationEnabled, SIGNAL(valueChanged()), this, SLOT(locationEnabledChanged()));

    m_flightMode = new ContextProperty(QStringLiteral("System.OfflineMode"), this);
    m_flightMode->subscribe();
    connect(m_flightMode, SIGNAL(valueChanged()), this, SLOT(locationEnabledChanged()));

    new GeoclueAdaptor(this);
    new PositionAdaptor(this);
    new VelocityAdaptor(this);
    new SatelliteAdaptor(this);

    m_manager = new QNetworkAccessManager(this);

    QDBusConnection connection = QDBusConnection::sessionBus();

    m_watcher = new QDBusServiceWatcher(this);
    m_watcher->setConnection(connection);
    m_watcher->setWatchMode(QDBusServiceWatcher::WatchForUnregistration);
    connect(m_watcher, SIGNAL(serviceUnregistered(QString)),
            this, SLOT(serviceUnregistered(QString)));

    m_idleTimer = startTimer(QuitIdleTime);

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
        error = m_xtra->init(&gpsXtraCallbacks);
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

void HybrisProvider::AddReference()
{
    if (!calledFromDBus())
        qFatal("AddReference must only be called from DBus");

    const QString service = message().service();
    if (!m_watchedServices.contains(service))
        m_watcher->addWatchedService(service);

    m_watchedServices.append(service);

    startPositioningIfNeeded();
}

void HybrisProvider::RemoveReference()
{
    if (!calledFromDBus())
        qFatal("RemoveReference must only be called from DBus");

    const QString service = message().service();
    m_watchedServices.removeOne(service);
    if (!m_watchedServices.contains(service))
        m_watcher->removeWatchedService(service);

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
    Q_UNUSED(options)
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
    if (event->timerId() == m_idleTimer) {
        killTimer(m_idleTimer);
        m_idleTimer = -1;
        qApp->quit();
    } else {
        QObject::timerEvent(event);
    }
}

void HybrisProvider::setLocation(const Location &location)
{
    // Stop listening to all PositionChanged signals from org.freedesktop.Geoclue.Position
    // interfaces.
    if (m_positionInjectionConnected) {
        QDBusConnection conn = QDBusConnection::sessionBus();
        conn.disconnect(QString(), QString(), QStringLiteral("org.freedesktop.Geoclue.Position"),
                        QStringLiteral("PositionChanged"),
                        this, SLOT(injectPosition(int,int,double,double,double,Accuracy)));
        m_positionInjectionConnected = false;
    }

    setStatus(StatusAvailable);
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
    m_watchedServices.removeAll(service);
    m_watcher->removeWatchedService(service);
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

    m_gps->inject_location(latitude, longitude, accuracy.horizontal());
}

void HybrisProvider::injectUtcTime()
{
    qint64 currentTime = QDateTime::currentMSecsSinceEpoch();
    m_gps->inject_time(currentTime, currentTime, 0);
}

void HybrisProvider::xtraDownloadRequest()
{
    if (m_xtraDownloadReply)
        return;

    QNetworkRequest request(QUrl(QStringLiteral("http://xtra1.gpsonextra.net/xtra.bin")));
    m_xtraDownloadReply = m_manager->get(request);
    connect(m_xtraDownloadReply, SIGNAL(finished()), this, SLOT(xtraDownloadFinished()));
    connect(m_xtraDownloadReply, SIGNAL(error(QNetworkReply::NetworkError)),
            this, SLOT(xtraDownloadFailed(QNetworkReply::NetworkError)));
}

void HybrisProvider::xtraDownloadFailed(QNetworkReply::NetworkError error)
{
    Q_UNUSED(error)

    m_xtraDownloadReply->deleteLater();
    m_xtraDownloadReply = 0;
}

void HybrisProvider::xtraDownloadFinished()
{
    QByteArray xtraData = m_xtraDownloadReply->readAll();
    m_xtra->inject_xtra_data(xtraData.data(), xtraData.length());
}

void HybrisProvider::agpsStatus(qint16 type, quint16 status, const QHostAddress &ipv4,
                                const QHostAddress &ipv6, const QByteArray &ssid,
                                const QByteArray &password)
{
    Q_UNUSED(ipv4)
    Q_UNUSED(ipv6)
    Q_UNUSED(ssid)
    Q_UNUSED(password)

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
        m_agps->data_conn_closed(AGPS_TYPE_SUPL);
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

void HybrisProvider::dataServiceConnectedChanged(bool connected)
{
    if (!connected)
        return;

    const QString interface =
        m_networkService->ethernet().value(QStringLiteral("Interface")).toString();
    if (interface.isEmpty()) {
        qWarning("Network service does not have a network interface associated with it.");
        return;
    }

    QOfonoManager ofonoManager;
    if (!ofonoManager.available()) {
        qWarning("Ofono not available.");
        return;
    }

    QStringList modems = ofonoManager.modems();
    if (modems.isEmpty()) {
        qWarning("No modems found.");
        return;
    }

    QOfonoConnectionManager ofonoConnectionManager;
    ofonoConnectionManager.setModemPath(modems.first());
    QStringList contexts = ofonoConnectionManager.contexts();
    QOfonoConnectionContext connectionContext;
    foreach (const QString &context, contexts) {
        // Find the APN of the connection context associated with the network session.
        connectionContext.setContextPath(context);
        if (connectionContext.settings().value(QStringLiteral("Interface")) == interface) {
            const QByteArray apn = connectionContext.accessPointName().toLocal8Bit();
            m_agps->data_conn_open(AGPS_TYPE_SUPL, apn.constData(), AGPS_APN_BEARER_IPV4);
            break;
        }
    }
}

void HybrisProvider::networkServiceDestroyed()
{
    m_networkService = 0;
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

    emit PositionChanged(positionFields, m_currentLocation.timestamp() / 1000,
                         m_currentLocation.latitude(), m_currentLocation.longitude(),
                         m_currentLocation.altitude(), m_currentLocation.accuracy());

    emit VelocityChanged(velocityFields, m_currentLocation.timestamp() / 1000,
                         m_currentLocation.speed(), m_currentLocation.direction(),
                         m_currentLocation.climb());
}

void HybrisProvider::emitSatelliteChanged()
{
    emit SatelliteChanged(m_satelliteTimestamp, m_usedPrns.length(), m_visibleSatellites.length(),
                          m_usedPrns, m_visibleSatellites);
}

void HybrisProvider::startPositioningIfNeeded()
{
    // Positioning is already started.
    if (m_status == StatusAcquiring || m_status == StatusAvailable)
        return;

    // Positioning is unused.
    if (m_watchedServices.isEmpty())
        return;

    // Positioning disabled externally
    if (!positioningEnabled())
        return;

    if (m_idleTimer != -1) {
        killTimer(m_idleTimer);
        m_idleTimer = -1;
    }

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

    int error = m_gps->set_position_mode(GPS_POSITION_MODE_MS_BASED,
                                         GPS_POSITION_RECURRENCE_PERIODIC, MinimumInterval,
                                         PreferredAccuracy, PreferredInitialFixTime);
    if (error) {
        qWarning("Failed to set position mode, error %d\n", error);
        setStatus(StatusError);
        return;
    }

    // Assist GPS by injecting current time.
    injectUtcTime();

    error = m_gps->start();
    if (error) {
        qWarning("Failed to start positioning, error %d\n", error);
        setStatus(StatusError);
        return;
    }

    setStatus(StatusAcquiring);
}

void HybrisProvider::stopPositioningIfNeeded()
{
    // Positioning is already stopped.
    if (m_status == StatusError || m_status == StatusUnavailable)
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
        int error = m_gps->stop();
        if (error)
            qWarning("Failed to stop positioning, error %d\n", error);

        setStatus(StatusUnavailable);
    }

    if (m_watchedServices.isEmpty())
        m_idleTimer = startTimer(QuitIdleTime);
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
    bool enabled = m_locationEnabled->value(false).toBool();
    bool flightMode = m_flightMode->value(false).toBool();

    return enabled && !flightMode;
}

void HybrisProvider::startDataConnection()
{
    if (!m_networkService) {
        QVector<NetworkService *> services = m_networkManager->getServices(QStringLiteral("cellular"));
        if (!services.isEmpty()) {
            m_networkService = services.first();
            connect(m_networkService, SIGNAL(connectedChanged(bool)),
                    this, SLOT(dataServiceConnectedChanged(bool)));
            connect(m_networkService, SIGNAL(destroyed()), this, SLOT(networkServiceDestroyed()));
        }
    }

    if (!m_networkService) {
        m_agps->data_conn_failed(AGPS_TYPE_SUPL);
        return;
    }

    if (!m_networkService->connected()) {
        m_requestedConnect = true;
        m_networkService->requestConnect();
        return;
    }

    // Data connection already available, tell GPS stack.
    dataServiceConnectedChanged(true);
}

void HybrisProvider::stopDataConnection()
{
    if (m_networkService && m_networkService->connected() && m_requestedConnect) {
        m_requestedConnect = false;
        m_networkService->requestDisconnect();
    }
}
