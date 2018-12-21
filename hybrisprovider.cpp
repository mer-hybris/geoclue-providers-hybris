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

#include "geoclue_adaptor.h"
#include "position_adaptor.h"
#include "velocity_adaptor.h"
#include "satellite_adaptor.h"

#include "connectiond_interface.h"
#include "connectionselector_interface.h"

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

HybrisProvider *staticProvider = Q_NULLPTR;

namespace
{

const int QuitIdleTime = 30000;
const int FixTimeout = 30000;
const quint32 MinimumInterval = 1000;
const quint32 PreferredAccuracy = 0;
const quint32 PreferredInitialFixTime = 0;
const double KnotsToMps = 0.514444;

const QString LocationSettingsDir = QStringLiteral("/etc/location/");
const QString LocationSettingsFile = QStringLiteral("/etc/location/location.conf");
const QString LocationSettingsEnabledKey = QStringLiteral("location/enabled");
const QString LocationSettingsAllowedDataSourcesGpsKey = QStringLiteral("allowed_data_sources/gps");
const QString LocationSettingsGpsEnabledKey = QStringLiteral("location/gps/enabled");
const QString LocationSettingsAgpsEnabledKey = QStringLiteral("location/%1/enabled");
const QString LocationSettingsAgpsOnlineEnabledKey = QStringLiteral("location/%1/online_enabled");
const QString LocationSettingsAgpsAgreementAcceptedKey = QStringLiteral("location/%1/agreement_accepted");
const QString LocationSettingsAgpsProvidersKey = QStringLiteral("location/agps_providers");
// deprecated keys
const QString LocationSettingsOldAgpsEnabledKey = QStringLiteral("location/agreement_accepted");
const QString LocationSettingsOldAgpsAgreementAcceptedKey = QStringLiteral("location/here_agreement_accepted");

const int MaxXtraServers = 3;
const QString XtraConfigFile = QStringLiteral("/etc/gps_xtra.ini");

void gnssXtraDownloadRequest()
{
    QMetaObject::invokeMethod(staticProvider, "xtraDownloadRequest", Qt::QueuedConnection);
}
}

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
:   QObject(parent), m_backend(Q_NULLPTR),
    m_status(StatusUnavailable), m_positionInjectionConnected(false), m_xtraDownloadReply(Q_NULLPTR), m_xtraServerIndex(0),
    m_requestedConnect(false), m_gpsStarted(false), m_locationSettings(Q_NULLPTR),
    m_networkManager(new NetworkManager(this)), m_cellularTechnology(Q_NULLPTR),
    m_ofonoExtModemManager(new QOfonoExtModemManager(this)),
    m_connectionManager(new QOfonoConnectionManager(this)), m_connectionContext(Q_NULLPTR), m_ntpSocket(Q_NULLPTR),
    m_agpsEnabled(false), m_agpsOnlineEnabled(false), m_useForcedXtraInject(false), m_xtraUserAgent("")
{
    if (staticProvider)
        qFatal("Only a single instance of HybrisProvider is supported.");

    qRegisterMetaType<Location>();
    qRegisterMetaType<QHostAddress>();
    qDBusRegisterMetaType<Accuracy>();
    qDBusRegisterMetaType<SatelliteInfo>();
    qDBusRegisterMetaType<QList<SatelliteInfo> >();

    staticProvider = this;

    new GeoclueAdaptor(this);
    new PositionAdaptor(this);
    new VelocityAdaptor(this);
    new SatelliteAdaptor(this);

    m_manager = new QNetworkAccessManager(this);

    connect(m_networkManager, SIGNAL(technologiesChanged()), this, SLOT(technologiesChanged()));
    connect(m_networkManager, SIGNAL(stateChanged(QString)), this, SLOT(stateChanged(QString)));

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

    QString xtraUserAgentFileName;
    QSettings settings(XtraConfigFile, QSettings::IniFormat);
    QString xtraServer;

    for (int i = 0; i < MaxXtraServers; i++) {
        QString key = QString("xtra/XTRA_SERVER_%1").arg(i);
        xtraServer = settings.value(key, "").toString();
        if (xtraServer != "") {
            m_xtraServers.enqueue(xtraServer);
        }
    }

    m_useForcedXtraInject = settings.value("xtra/XTRA_FORCE_INJECT", "").toBool();

    xtraUserAgentFileName = settings.value("xtra/XTRA_USERAGENT_FILE", "").toString();
    if (xtraUserAgentFileName != "") {
        QFile xtraUserAgentFile(xtraUserAgentFileName);
        if (xtraUserAgentFile.open(QIODevice::ReadOnly)) {
            m_xtraUserAgent = xtraUserAgentFile.readLine();
        }
    }

    if (m_xtraServers.isEmpty()) {
        QFile gpsConf(QStringLiteral("/system/etc/gps.conf"));
        if (gpsConf.open(QIODevice::ReadOnly)) {

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
        }
    }

    m_backend = getLocationBackend();

    if (!m_backend || !m_backend->gnssInit()) {
        m_status = StatusError;
        return;
    }

    m_backend->aGnssInit();
    m_backend->gnssNiInit();
    m_backend->aGnssRilInit();
    m_backend->gnssXtraInit();
    m_backend->gnssDebugInit();
}

HybrisProvider::~HybrisProvider()
{
    if (m_backend) {
        m_backend->gnssCleanup();
        delete m_backend;
    }

    if (staticProvider == this)
        staticProvider = 0;
}

void HybrisProvider::setLocationSettings(LocationSettings *settings)
{
    if (!m_locationSettings) {
        m_locationSettings = settings;
        connect(m_locationSettings, &LocationSettings::locationEnabledChanged,
                this, &HybrisProvider::locationEnabledChanged);
        connect(m_locationSettings, &LocationSettings::allowedDataSourcesChanged,
                this, &HybrisProvider::locationEnabledChanged);
        connect(m_locationSettings, &LocationSettings::gpsEnabledChanged,
                this, &HybrisProvider::locationEnabledChanged);
        connect(m_locationSettings, &LocationSettings::gpsFlightModeChanged,
                this, &HybrisProvider::locationEnabledChanged);
        connect(m_locationSettings, &LocationSettings::hereStateChanged,
                this, &HybrisProvider::locationEnabledChanged);
        connect(m_locationSettings, &LocationSettings::mlsEnabledChanged,
                this, &HybrisProvider::locationEnabledChanged);
        connect(m_locationSettings, &LocationSettings::mlsOnlineStateChanged,
                this, &HybrisProvider::locationEnabledChanged);
    }
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

        m_backend->gnssSetPositionMode(m_agpsEnabled ? HYBRIS_GNSS_POSITION_MODE_MS_BASED
                                                     : HYBRIS_GNSS_POSITION_MODE_STANDALONE,
                                       HYBRIS_GNSS_POSITION_RECURRENCE_PERIODIC, updateInterval,
                                       PreferredAccuracy, PreferredInitialFixTime);
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

    m_backend->gnssInjectLocation(latitude, longitude, accuracy.horizontal());
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

        m_backend->gnssInjectTime(time, reference, certainty);

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

    m_xtraServerIndex = 0;

    xtraDownloadRequestSendNext();
}

void HybrisProvider::xtraDownloadRequestSendNext()
{
    if (m_xtraServerIndex >= m_xtraServers.count())
        return;

    qCDebug(lcGeoclueHybris) << m_xtraServers;

    QNetworkRequest network_request(m_xtraServers[m_xtraServerIndex]);
    if (m_xtraUserAgent != "") {
        network_request.setRawHeader("User-Agent", m_xtraUserAgent.toUtf8());
    }
    m_xtraDownloadReply = m_manager->get(network_request);
    connect(m_xtraDownloadReply, SIGNAL(finished()), this, SLOT(xtraDownloadFinished()));

    m_xtraServerIndex++;
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
        m_backend->gnssXtraInjectXtraData(xtraData);

        qCDebug(lcGeoclueHybris) << "injected " << xtraData.length() << " bytes of xtra data";

        m_xtraDownloadReply = 0;
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
        m_backend->aGnssDataConnFailed();
        return;
    }

    if (type != HYBRIS_AGNSS_TYPE_SUPL) {
        qWarning("Only SUPL AGPS is supported.");
        return;
    }

    switch (status) {
    case HYBRIS_GNSS_REQUEST_AGNSS_DATA_CONN:
        startDataConnection();
        break;
    case HYBRIS_GNSS_RELEASE_AGNSS_DATA_CONN:
        // Immediately inform that connection is closed.
        m_backend->aGnssDataConnClosed();
        stopDataConnection();
        break;
    case HYBRIS_GNSS_AGNSS_DATA_CONNECTED:
        break;
    case HYBRIS_GNSS_AGNSS_DATA_CONN_DONE:
        break;
    case HYBRIS_GNSS_AGNSS_DATA_CONN_FAILED:
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
        m_backend->aGnssDataConnFailed();
}

void HybrisProvider::connectionSelected(bool selected)
{
    if (!selected) {
        qCDebug(lcGeoclueHybris) << "User aborted mobile data connection";
        m_backend->aGnssDataConnFailed();
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

void HybrisProvider::stateChanged(const QString &state)
{
    if (state == "online") {
        if (m_gpsStarted && m_useForcedXtraInject) {
            gnssXtraDownloadRequest();
        }
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

        m_backend->aGnssDataConnOpen(apn.constData(), protocol);
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

    if (!m_backend)
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

    if (!m_backend->gnssSetPositionMode(m_agpsEnabled ? HYBRIS_GNSS_POSITION_MODE_MS_BASED
                                                      : HYBRIS_GNSS_POSITION_MODE_STANDALONE,
                                        HYBRIS_GNSS_POSITION_RECURRENCE_PERIODIC,
                                        minimumRequestedUpdateInterval(),
                                        PreferredAccuracy, PreferredInitialFixTime)) {
        return;
    }

    qCDebug(lcGeoclueHybris) << "Starting positioning";

    if (!m_backend->gnssStart()) {
        setStatus(StatusError);
        return;
    }

    m_gpsStarted = true;

    if (m_useForcedXtraInject && m_networkManager->state() == "online") {
        gnssXtraDownloadRequest();
    }
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

    if (m_backend) {
        qCDebug(lcGeoclueHybris) << "Stopping positioning";
        m_backend->gnssStop();
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
*/
bool HybrisProvider::positioningEnabled()
{
    // update our AGPS enablement states, used for position injection and mode capability.
    m_agpsEnabled = (m_locationSettings->hereAvailable() && m_locationSettings->hereState() == LocationSettings::OnlineAGpsEnabled)
                 || (m_locationSettings->mlsAvailable()  && m_locationSettings->mlsEnabled());
    m_agpsOnlineEnabled = (m_locationSettings->hereAvailable() && m_locationSettings->hereState() == LocationSettings::OnlineAGpsEnabled)
                       || (m_locationSettings->mlsAvailable()  && m_locationSettings->mlsOnlineState() == LocationSettings::OnlineAGpsEnabled);

    // enable GPS positioning if location and the GPS are enabled, and the GPS is not in flight mode - if it is allowed by MDM.
    return m_locationSettings->locationEnabled()
       &&  m_locationSettings->gpsAvailable()
       &&  m_locationSettings->gpsEnabled()
       && !m_locationSettings->gpsFlightMode()
       && (m_locationSettings->allowedDataSources() & LocationSettings::GpsData);
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
        m_backend->aGnssDataConnFailed();
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

        m_backend->aGnssDataConnFailed();

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
