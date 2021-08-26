/*
    Copyright (C) 2015 Jolla Ltd.
    Contact: Aaron McCarthy <aaron.mccarthy@jollamobile.com>

    This file is part of geoclue-hybris.

    Geoclue-hybris is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License.
*/

#include "hallocationbackend.h"

#include "hybrisprovider.h"

#include <QtNetwork/QHostAddress>

#include <android-config.h>

#include <errno.h>
#include <grp.h>
#include <strings.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

// Define versions of the Android GPS interface supported.
#if ANDROID_VERSION_MAJOR >= 7
    #define GEOCLUE_ANDROID_GPS_INTERFACE 3
#elif ANDROID_VERSION_MAJOR >= 5
    #define GEOCLUE_ANDROID_GPS_INTERFACE 2
#elif ANDROID_VERSION_MAJOR == 4 && ANDROID_VERSION_MINOR >= 2
    #define GEOCLUE_ANDROID_GPS_INTERFACE 1
#else
    // By default expects Android 4.1
#endif

HybrisLocationBackend *getLocationBackend()
{
    return qobject_cast<HybrisLocationBackend *>(new HalLocationBackend());
}

namespace
{

const double MpsToKnots = 1.943844;

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
        loc.setSpeed(location->speed * MpsToKnots);

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

#if GEOCLUE_ANDROID_GPS_INTERFACE == 3
void gnssSvStatusCallback(GnssSvStatus *svStatus)
{
    QList<SatelliteInfo> satellites;
    QList<int> usedPrns;

    for (int i = 0; i < svStatus->num_svs; ++i) {
        SatelliteInfo satInfo;
        GnssSvInfo &svInfo = svStatus->gnss_sv_list[i];
        satInfo.setSnr(svInfo.c_n0_dbhz);
        satInfo.setElevation(svInfo.elevation);
        satInfo.setAzimuth(svInfo.azimuth);
        int prn = svInfo.svid;
        // From https://github.com/barbeau/gpstest
        // and https://github.com/mvglasow/satstat/wiki/NMEA-IDs
        if (svInfo.constellation == GNSS_CONSTELLATION_SBAS) {
            prn -= 87;
        } else if (svInfo.constellation == GNSS_CONSTELLATION_GLONASS) {
            prn += 64;
        } else if (svInfo.constellation == GNSS_CONSTELLATION_BEIDOU) {
            prn += 200;
        } else if (svInfo.constellation == GNSS_CONSTELLATION_GALILEO) {
            prn += 300;
        }
        satInfo.setPrn(prn);
        satellites.append(satInfo);

        if (svInfo.flags & GNSS_SV_FLAGS_USED_IN_FIX)
            usedPrns.append(svInfo.svid);
    }

    QMetaObject::invokeMethod(staticProvider, "setSatellite", Qt::QueuedConnection,
                              Q_ARG(QList<SatelliteInfo>, satellites),
                              Q_ARG(QList<int>, usedPrns));
}
#endif

#ifdef USE_GPS_VENDOR_EXTENSION
void gnssSvStatusCallback_custom(GnssSvStatus *svStatus)
{
    QList<SatelliteInfo> satellites;
    QList<int> usedPrns;

    for (int i = 0; i < svStatus->num_svs; ++i) {
        SatelliteInfo satInfo;
        GnssSvInfo &svInfo = svStatus->sv_list[i];
        satInfo.setPrn(svInfo.prn);
        satInfo.setSnr(svInfo.snr);
        satInfo.setElevation(svInfo.elevation);
        satInfo.setAzimuth(svInfo.azimuth);
        satellites.append(satInfo);
    }

    QMetaObject::invokeMethod(staticProvider, "setSatellite", Qt::QueuedConnection,
                              Q_ARG(QList<SatelliteInfo>, satellites),
                              Q_ARG(QList<int>, usedPrns));
}
#endif

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
    qCDebug(lcGeoclueHybris) << "GNSS request UTC time";

    QMetaObject::invokeMethod(staticProvider, "injectUtcTime", Qt::QueuedConnection);
}

#if GEOCLUE_ANDROID_GPS_INTERFACE == 3
void gnssSetSystemInfoCallback(const GnssSystemInfo *info)
{
    Q_UNUSED(info)
    qCDebug(lcGeoclueHybris) << "GNSS set system info";
}
#endif

void agpsStatusCallback(AGpsStatus *status)
{
    QHostAddress ipv4;
    QHostAddress ipv6;
    QByteArray ssid;
    QByteArray password;

#if GEOCLUE_ANDROID_GPS_INTERFACE >= 2
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
    qCDebug(lcGeoclueHybris) << "GNSS NI notify";
}

void agpsRilRequestSetId(uint32_t flags)
{
    Q_UNUSED(flags)
    qCDebug(lcGeoclueHybris) << "AGNSS RIL request set ID flags" << showbase << hex << flags;
}

void agpsRilRequestRefLoc(uint32_t flags)
{
    Q_UNUSED(flags)
    qCDebug(lcGeoclueHybris) << "AGNSS RIL request ref location flags" << showbase << hex << flags;
}

void gnssXtraDownloadRequest()
{
    QMetaObject::invokeMethod(staticProvider, "xtraDownloadRequest", Qt::QueuedConnection);
}

#if GEOCLUE_ANDROID_GPS_INTERFACE >= 2
HybrisApnIpType fromContextProtocol(const QString &protocol)
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
HybrisAGpsBearerType fromContextProtocol(const QString &protocol)
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
#ifdef USE_GPS_VENDOR_EXTENSION
    gnssSvStatusCallback_custom,
#endif
    nmeaCallback,
    setCapabilitiesCallback,
    acquireWakelockCallback,
    releaseWakelockCallback,
    createThreadCallback,
    requestUtcTimeCallback,
#if GEOCLUE_ANDROID_GPS_INTERFACE == 3
    gnssSetSystemInfoCallback,
    gnssSvStatusCallback,
#endif
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
    { gnssXtraDownloadRequest, createThreadCallback },
    0
};

HalLocationBackend::HalLocationBackend(QObject *parent)
:   HybrisLocationBackend(parent), m_gps(Q_NULLPTR), m_agps(Q_NULLPTR), m_agpsril(Q_NULLPTR), m_gpsni(Q_NULLPTR), m_xtra(Q_NULLPTR), m_debug(Q_NULLPTR)
{
    uid_t realUid;
    uid_t effectiveUid;
    uid_t savedUid;

    int result = getresuid(&realUid, &effectiveUid, &savedUid);
    if (result == -1)
        qFatal("Failed to get process uids, %s", strerror(errno));

    gid_t supplementaryGroups[NGROUPS_MAX];
    int numberGroups = getgroups(NGROUPS_MAX, supplementaryGroups);
    if (numberGroups == -1)
        qFatal("Failed to get supplementary groups, %s", strerror(errno));

    if (numberGroups + 1 > NGROUPS_MAX)
        qFatal("Too many supplementary groups");

    group *group = getgrnam("gps");
    if (!group)
        qFatal("Failed to get id of gps group, %s", strerror(errno));

    supplementaryGroups[numberGroups++] = group->gr_gid;

    // remove nfc, audio, radio and bluetooth groups to avoid confusion in BSP
    const char *groups_to_remove[] = {"bluetooth", "radio", "audio", "nfc", NULL};

    for (int idx = 0; idx < numberGroups; idx++) {
        for (int j = 0; groups_to_remove[j]; j++) {
            group = getgrnam(groups_to_remove[j]);
            if (group) {
                if (supplementaryGroups[idx] == group->gr_gid) {
                    // remove it
                    qCDebug(lcGeoclueHybris, "Removing supplementary group %s (%i)", groups_to_remove[j], supplementaryGroups[idx]);
                    memmove((void*)&supplementaryGroups[idx], (void*)&supplementaryGroups[idx + 1], (numberGroups - idx) * sizeof(gid_t));
                    numberGroups--;
                }
            }
        }
    }

#if GEOCLUE_ANDROID_GPS_INTERFACE >= 2
    group = getgrnam("net_raw");
    if (group) {
        if (numberGroups + 1 > NGROUPS_MAX)
            qWarning("Too many supplementary groups, can't add net_raw");
        else
            supplementaryGroups[numberGroups++] = group->gr_gid;
    }
#endif

    numberGroups = setgroups(numberGroups, supplementaryGroups);
    if (numberGroups == -1)
        qFatal("Failed to set supplementary groups, %s", strerror(errno));

#if GEOCLUE_ANDROID_GPS_INTERFACE == 1
    // Drop privileges.
    result = setuid(realUid);
    if (result == -1)
        qFatal("Failed to set process uid to %d, %s", realUid, strerror(errno));
#endif
}

HalLocationBackend::~HalLocationBackend()
{
}

// Gnss
bool HalLocationBackend::gnssInit()
{
    const hw_module_t *hwModule;

    int error = hw_get_module(GPS_HARDWARE_MODULE_ID, &hwModule);
    if (error) {
        qWarning("Android GPS interface not found, error %d", error);
        return false;
    }

    qWarning("Android GPS hardware module \"%s\" \"%s\" %u.%u", hwModule->id, hwModule->name,
             hwModule->module_api_version, hwModule->hal_api_version);

    error = hwModule->methods->open(hwModule, GPS_HARDWARE_MODULE_ID,
                                    reinterpret_cast<hw_device_t **>(&m_gpsDevice));
    if (error) {
        qWarning("Failed to open GPS device, error %d", error);
        return false;
    }

    m_gps = m_gpsDevice->get_gps_interface(m_gpsDevice);
    if (!m_gps)
        return false;

    qWarning("Initialising GPS interface");

    error = m_gps->init(&gpsCallbacks);
    if (error) {
        qWarning("Failed to initialise GPS interface, error %d", error);
        return false;
    }

    return true;
}

bool HalLocationBackend::gnssStart()
{
    if (m_gps) {
        int error = m_gps->start();
        if (error) {
            qWarning("Failed to start positioning, error %d", error);
            return false;
        }
        return true;
    }
    return false;
}

bool HalLocationBackend::gnssStop()
{
    if (m_gps) {
        int error = m_gps->stop();
        if (error) {
            qWarning("Failed to stop positioning, error %d", error);
            return false;
        }
        return true;
    }
    return false;
}

void HalLocationBackend::gnssCleanup()
{
    if (m_gps)
        m_gps->cleanup();

    if (m_gpsDevice->common.close)
        m_gpsDevice->common.close(reinterpret_cast<hw_device_t *>(m_gpsDevice));
}

bool HalLocationBackend::gnssInjectLocation(double latitudeDegrees, double longitudeDegrees, float accuracyMeters)
{
    if (m_gps) {
        int error = m_gps->inject_location(latitudeDegrees, longitudeDegrees, accuracyMeters);
        if (error) {
            qWarning("Failed to inject location, error %d", error);
            return false;
        }
        return true;
    }
    return false;
}

bool HalLocationBackend::gnssInjectTime(HybrisGnssUtcTime timeMs, int64_t timeReferenceMs, int32_t uncertaintyMs)
{
    if (m_gps) {
        int error = m_gps->inject_time(timeMs, timeReferenceMs, uncertaintyMs);
        if (error) {
            qWarning("Failed to inject time, error %d", error);
            return false;
        }
        return true;
    }
    return false;
}

void HalLocationBackend::gnssDeleteAidingData(HybrisGnssAidingData aidingDataFlags)
{
    if (m_gps) {
        m_gps->delete_aiding_data(aidingDataFlags);
    }
}

bool HalLocationBackend::gnssSetPositionMode(HybrisGnssPositionMode mode, HybrisGnssPositionRecurrence recurrence,
                                    uint32_t minIntervalMs, uint32_t preferredAccuracyMeters,
                                    uint32_t preferredTimeMs)
{
    int error = m_gps->set_position_mode(mode, recurrence, minIntervalMs,
                                         preferredAccuracyMeters, preferredTimeMs);
    if (error) {
        qWarning("While updating the updateInterval, failed to set position mode, error %d", error);
        return false;
    }
    return true;
}

// GnssDebug
void HalLocationBackend::gnssDebugInit()
{
    m_debug = static_cast<const GpsDebugInterface *>(m_gps->get_extension(GPS_DEBUG_INTERFACE));
}

// GnnNi
void HalLocationBackend::gnssNiInit()
{
    m_gpsni = static_cast<const GpsNiInterface *>(m_gps->get_extension(GPS_NI_INTERFACE));
    if (m_gpsni) {
        qWarning("Initialising GPS NI Interface");
        m_gpsni->init(&gpsNiCallbacks);
    }
}

void HalLocationBackend::gnssNiRespond(int32_t notifId, HybrisGnssUserResponseType userResponse)
{
    m_gpsni->respond(notifId, userResponse);
}

// GnssXtra
void HalLocationBackend::gnssXtraInit()
{
    m_xtra = static_cast<const GpsXtraInterface *>(m_gps->get_extension(GPS_XTRA_INTERFACE));
    if (m_xtra) {
        qWarning("Initialising GPS Xtra Interface");
        int error = m_xtra->init(&gpsXtraCallbacks.callbacks);
        if (error)
            qWarning("GPS Xtra Interface init failed, error %d", error);
    }
}

bool HalLocationBackend::gnssXtraInjectXtraData(QByteArray &xtraData)
{
    if (!m_xtra->inject_xtra_data(xtraData.data(), xtraData.length())) {
        return true;
    }
    return false;
}

// AGnss
void HalLocationBackend::aGnssInit()
{
    m_agps = static_cast<const AGpsInterface *>(m_gps->get_extension(AGPS_INTERFACE));
    if (m_agps) {
        qWarning("Initialising AGPS Interface");
        m_agps->init(&agpsCallbacks);
    }
}

bool HalLocationBackend::aGnssDataConnClosed()
{
    if (
#if GEOCLUE_ANDROID_GPS_INTERFACE >= 1
        !m_agps->data_conn_closed()
#else
        !m_agps->data_conn_closed(AGPS_TYPE_SUPL)
#endif
        ) {
        return true;
    }
    return false;
}

bool HalLocationBackend::aGnssDataConnFailed()
{
    if (
#if GEOCLUE_ANDROID_GPS_INTERFACE >= 1
        !m_agps->data_conn_failed()
#else
        !m_agps->data_conn_failed(AGPS_TYPE_SUPL)
#endif
        ) {
        return true;
    }
    return false;
}

bool HalLocationBackend::aGnssDataConnOpen(const QByteArray &apn, const QString &protocol)
{
    int error;
#if GEOCLUE_ANDROID_GPS_INTERFACE >= 2
    if (m_agps->data_conn_open_with_apn_ip_type)
        error = m_agps->data_conn_open_with_apn_ip_type(apn.constData(), fromContextProtocol(protocol));
    else
        error = m_agps->data_conn_open(apn.constData());
#elif GEOCLUE_ANDROID_GPS_INTERFACE == 1
    error = m_agps->data_conn_open(apn.constData());
#else
    error = m_agps->data_conn_open(AGPS_TYPE_SUPL, apn.constData(), fromContextProtocol(protocol));
#endif
    return !error;
}

int HalLocationBackend::aGnssSetServer(HybrisAGnssType type, const char* hostname, int port)
{
    return m_agps->set_server(type, hostname, port);
}

// AGnssRil
void HalLocationBackend::aGnssRilInit()
{
    m_agpsril = static_cast<const AGpsRilInterface *>(m_gps->get_extension(AGPS_RIL_INTERFACE));
    if (m_agpsril) {
        qWarning("Initialising AGPS RIL Interface");
        m_agpsril->init(&agpsRilCallbacks);
    }
}
