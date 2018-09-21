/*
    Copyright (C) 2015 Jolla Ltd.
    Copyright (C) 2018 Matti Lehtimäki <matti.lehtimaki@gmail.com>
    Contact: Aaron McCarthy <aaron.mccarthy@jollamobile.com>

    This file is part of geoclue-hybris.

    Geoclue-hybris is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License.
*/

#include "binderlocationbackend.h"

#include "hybrisprovider.h"

#include <QtNetwork/QHostAddress>

#include <strings.h>
#include <sys/time.h>

#define GNSS_BINDER_DEFAULT_DEV  "/dev/hwbinder"

HybrisLocationBackend *getLocationBackend()
{
    return qobject_cast<HybrisLocationBackend *>(new BinderLocationBackend());
}

enum GnssFunctions {
    GNSS_SET_CALLBACK = 1,
    GNSS_START = 2,
    GNSS_STOP = 3,
    GNSS_CLEANUP = 4,
    GNSS_INJECT_TIME = 5,
    GNSS_INJECT_LOCATION = 6,
    GNSS_DELETE_AIDING_DATA = 7,
    GNSS_SET_POSITION_MODE = 8,
    GNSS_GET_EXTENSION_AGNSS_RIL = 9,
    GNSS_GET_EXTENSION_GNSS_GEOFENCING = 10,
    GNSS_GET_EXTENSION_AGNSS = 11,
    GNSS_GET_EXTENSION_GNSS_NI = 12,
    GNSS_GET_EXTENSION_GNSS_MEASUREMENT = 13,
    GNSS_GET_EXTENSION_GNSS_NAVIGATION_MESSAGE = 14,
    GNSS_GET_EXTENSION_XTRA = 15,
    GNSS_GET_EXTENSION_GNSS_CONFIGURATION = 16,
    GNSS_GET_EXTENSION_GNSS_DEBUG = 17,
    GNSS_GET_EXTENSION_GNSS_BATCHING = 18
};

enum GnssCallbacks {
    GNSS_LOCATION_CB = 1,
    GNSS_STATUS_CB = 2,
    GNSS_SV_STATUS_CB = 3,
    GNSS_NMEA_CB = 4,
    GNSS_SET_CAPABILITIES_CB = 5,
    GNSS_ACQUIRE_WAKELOCK_CB = 6,
    GNSS_RELEASE_WAKELOCK_CB = 7,
    GNSS_REQUEST_TIME_CB = 8,
    GNSS_SET_SYSTEM_INFO_CB = 9
};

enum GnssDebudFunctions {
    GNSS_DEBUG_GET_DEBUG_DATA = 1
};

enum GnssNiFunctions {
    GNSS_NI_SET_CALLBACK = 1,
    GNSS_NI_RESPOND = 2
};

enum GnssNiCallbacks {
    GNSS_NI_NOTIFY_CB = 1
};

enum GnssXtraFunctions {
    GNSS_XTRA_SET_CALLBACK = 1,
    GNSS_XTRA_INJECT_XTRA_DATA = 2
};

enum GnssXtraCallbacks {
    GNSS_XTRA_DOWNLOAD_REQUEST_CB = 1
};

enum AGnssFunctions {
    AGNSS_SET_CALLBACK = 1,
    AGNSS_DATA_CONN_CLOSED = 2,
    AGNSS_DATA_CONN_FAILED = 3,
    AGNSS_SET_SERVER = 4,
    AGNSS_DATA_CONN_OPEN = 5
};

enum AGnssCallbacks {
    AGNSS_STATUS_IP_V4_CB = 1,
    AGNSS_STATUS_IP_V6_CB = 2
};

enum AGnssRilFunctions {
    AGNSS_RIL_SET_CALLBACK = 1,
    AGNSS_RIL_SET_REF_LOCATION = 2,
    AGNSS_RIL_SET_ID = 3,
    AGNSS_RIL_UPDATE_NETWORK_STATE = 4,
    AGNSS_RIL_UPDATE_NETWORK_AVAILABILITY = 5
};

enum AGnssRilCallbacks {
    AGNSS_RIL_REQUEST_REF_ID_CB = 1,
    AGNSS_RIL_REQUEST_REF_LOC_CB = 2
};

enum HybrisApnIpTypeEnum {
    HYBRIS_APN_IP_INVALID  = 0,
    HYBRIS_APN_IP_IPV4     = 1,
    HYBRIS_APN_IP_IPV6     = 2,
    HYBRIS_APN_IP_IPV4V6   = 3
};

#define GNSS_IFACE(x)       "android.hardware.gnss@1.0::" x
#define GNSS_REMOTE         GNSS_IFACE("IGnss")
#define GNSS_CALLBACK       GNSS_IFACE("IGnssCallback")
#define GNSS_DEBUG_REMOTE   GNSS_IFACE("IGnssDebug")
#define GNSS_NI_REMOTE      GNSS_IFACE("IGnssNi")
#define GNSS_NI_CALLBACK    GNSS_IFACE("IGnssNiCallback")
#define GNSS_XTRA_REMOTE    GNSS_IFACE("IGnssXtra")
#define GNSS_XTRA_CALLBACK  GNSS_IFACE("IGnssXtraCallback")
#define AGNSS_REMOTE        GNSS_IFACE("IAGnss")
#define AGNSS_CALLBACK      GNSS_IFACE("IAGnssCallback")
#define AGNSS_RIL_REMOTE    GNSS_IFACE("IAGnssRil")
#define AGNSS_RIL_CALLBACK  GNSS_IFACE("IAGnssRilCallback")


/*==========================================================================*
 * Implementation
 *==========================================================================*/

HybrisApnIpType fromContextProtocol(const QString &protocol)
{
    if (protocol == QLatin1String("ip"))
        return HYBRIS_APN_IP_IPV4;
    else if (protocol == QLatin1String("ipv6"))
        return HYBRIS_APN_IP_IPV6;
    else if (protocol == QLatin1String("dual"))
        return HYBRIS_APN_IP_IPV4V6;
    else
        return HYBRIS_APN_IP_INVALID;
}

static const void *geoclue_binder_gnss_decode_struct1(
    GBinderReader *in,
    guint size)
{
    const void *result = Q_NULLPTR;
    GBinderBuffer *buf = gbinder_reader_read_buffer(in);

    if (buf && buf->size == size) {
        result = buf->data;
    }
    gbinder_buffer_free(buf);
    return result;
}

#define geoclue_binder_gnss_decode_struct(type,in) \
    ((const type*)geoclue_binder_gnss_decode_struct1(in, sizeof(type)))

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

void processNmea(gint64 timestamp, const char *nmeaData)
{
    int length = strlen(nmeaData);
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

static GBinderLocalReply *geoclue_binder_gnss_callback(
    GBinderLocalObject *obj,
    GBinderRemoteRequest *req,
    guint code,
    guint flags,
    int *status,
    void *user_data)
{
    Q_UNUSED(flags)
    Q_UNUSED(user_data)
    const char *iface = gbinder_remote_request_interface(req);

    if (!g_strcmp0(iface, GNSS_CALLBACK)) {
        GBinderReader reader;

        gbinder_remote_request_init_reader(req, &reader);
        switch (code) {
        case GNSS_LOCATION_CB:
            {
            Location loc;

            const GnssLocation *location = geoclue_binder_gnss_decode_struct
                (GnssLocation, &reader);

            loc.setTimestamp(location->timestamp);

            if (location->gnssLocationFlags & HYBRIS_GNSS_LOCATION_HAS_LAT_LONG) {
                loc.setLatitude(location->latitudeDegrees);
                loc.setLongitude(location->longitudeDegrees);
            }

            if (location->gnssLocationFlags & HYBRIS_GNSS_LOCATION_HAS_ALTITUDE)
                loc.setAltitude(location->altitudeMeters);

            if (location->gnssLocationFlags & HYBRIS_GNSS_LOCATION_HAS_SPEED)
                loc.setSpeed(location->speedMetersPerSec);

            if (location->gnssLocationFlags & HYBRIS_GNSS_LOCATION_HAS_BEARING)
                loc.setDirection(location->bearingDegrees);

            if ((location->gnssLocationFlags & HYBRIS_GNSS_LOCATION_HAS_HORIZONTAL_ACCURACY) ||
                (location->gnssLocationFlags & HYBRIS_GNSS_LOCATION_HAS_VERTICAL_ACCURACY)) {
                Accuracy accuracy;
                if (location->gnssLocationFlags & HYBRIS_GNSS_LOCATION_HAS_HORIZONTAL_ACCURACY) {
                    accuracy.setHorizontal(location->horizontalAccuracyMeters);
                }
                if (location->gnssLocationFlags & HYBRIS_GNSS_LOCATION_HAS_VERTICAL_ACCURACY) {
                    accuracy.setVertical(location->verticalAccuracyMeters);
                }
                loc.setAccuracy(accuracy);
            }

            QMetaObject::invokeMethod(staticProvider, "setLocation", Qt::QueuedConnection,
                                      Q_ARG(Location, loc));
            }
            break;
        case GNSS_STATUS_CB:
            {
            guint32 stat;
            if (gbinder_reader_read_uint32(&reader, &stat)) {
                if (stat == HYBRIS_GNSS_STATUS_ENGINE_ON) {
                    QMetaObject::invokeMethod(staticProvider, "engineOn", Qt::QueuedConnection);
                }
                if (stat == HYBRIS_GNSS_STATUS_ENGINE_OFF) {
                    QMetaObject::invokeMethod(staticProvider, "engineOff", Qt::QueuedConnection);
                }
            }
            }
            break;
        case GNSS_SV_STATUS_CB:
            {
            const GnssSvStatus *svStatus = geoclue_binder_gnss_decode_struct
                (GnssSvStatus, &reader);

            QList<SatelliteInfo> satellites;
            QList<int> usedPrns;

            for (int i = 0; i < svStatus->numSvs; ++i) {
                SatelliteInfo satInfo;
                GnssSvInfo svInfo = svStatus->gnssSvList.data[i];
                satInfo.setSnr(svInfo.cN0Dbhz);
                satInfo.setElevation(svInfo.elevationDegrees);
                satInfo.setAzimuth(svInfo.azimuthDegrees);
                int prn = svInfo.svid;
                // From https://github.com/barbeau/gpstest
                // and https://github.com/mvglasow/satstat/wiki/NMEA-IDs
                if (svInfo.constellation == GnssConstellationType::SBAS) {
                    prn -= 87;
                } else if (svInfo.constellation == GnssConstellationType::GLONASS) {
                    prn += 64;
                } else if (svInfo.constellation == GnssConstellationType::BEIDOU) {
                    prn += 200;
                } else if (svInfo.constellation == GnssConstellationType::GALILEO) {
                    prn += 300;
                }
                satInfo.setPrn(prn);
                satellites.append(satInfo);

                if (svInfo.svFlag & HYBRIS_GNSS_SV_FLAGS_USED_IN_FIX)
                    usedPrns.append(svInfo.svid);
            }

            QMetaObject::invokeMethod(staticProvider, "setSatellite", Qt::QueuedConnection,
                                      Q_ARG(QList<SatelliteInfo>, satellites),
                                      Q_ARG(QList<int>, usedPrns));
            }
            break;
        case GNSS_NMEA_CB:
            {
            gint64 timestamp;
            if (gbinder_reader_read_int64(&reader, &timestamp)) {
                char *nmeaData = gbinder_reader_read_hidl_string(&reader);
                if (nmeaData) {
                    processNmea(timestamp, nmeaData);
                    g_free(nmeaData);
                }
            }
            }
            break;
        case GNSS_SET_CAPABILITIES_CB:
            {
            guint32 capabilities;
            if (gbinder_reader_read_uint32(&reader, &capabilities)) {
                qCDebug(lcGeoclueHybris) << "capabilities" << showbase << hex << capabilities;
            }
            }
            break;
        case GNSS_ACQUIRE_WAKELOCK_CB:
        case GNSS_RELEASE_WAKELOCK_CB:
            break;
        case GNSS_REQUEST_TIME_CB:
            qCDebug(lcGeoclueHybris) << "GNSS request UTC time";
            QMetaObject::invokeMethod(staticProvider, "injectUtcTime", Qt::QueuedConnection);
            break;
        case GNSS_SET_SYSTEM_INFO_CB:
            qCDebug(lcGeoclueHybris) << "GNSS set system info";
            break;
        default:
            qWarning("Failed to decode callback %u\n", code);
            break;
        }
        *status = GBINDER_STATUS_OK;
        return gbinder_local_reply_append_int32(gbinder_local_object_new_reply(obj), 0);
    } else {
        qWarning("Unknown interface %s and code %u\n", iface, code);
        *status = GBINDER_STATUS_FAILED;
    }
    return Q_NULLPTR;
}

static GBinderLocalReply *geoclue_binder_gnss_xtra_callback(
    GBinderLocalObject *obj,
    GBinderRemoteRequest *req,
    guint code,
    guint flags,
    int *status,
    void *user_data)
{
    Q_UNUSED(flags)
    Q_UNUSED(user_data)
    const char *iface = gbinder_remote_request_interface(req);

    if (!g_strcmp0(iface, GNSS_XTRA_CALLBACK)) {
        GBinderReader reader;

        gbinder_remote_request_init_reader(req, &reader);
        switch (code) {
        case GNSS_XTRA_DOWNLOAD_REQUEST_CB:
            QMetaObject::invokeMethod(staticProvider, "xtraDownloadRequest", Qt::QueuedConnection);
            break;
        default:
            qWarning("Failed to decode callback %u\n", code);
            break;
        }
        *status = GBINDER_STATUS_OK;
        return gbinder_local_reply_append_int32(gbinder_local_object_new_reply(obj), 0);
    } else {
        qWarning("Unknown interface %s and code %u\n", iface, code);
        *status = GBINDER_STATUS_FAILED;
    }
    return Q_NULLPTR;
}

static GBinderLocalReply *geoclue_binder_agnss_callback(
    GBinderLocalObject *obj,
    GBinderRemoteRequest *req,
    guint code,
    guint flags,
    int *status,
    void *user_data)
{
    Q_UNUSED(flags)
    Q_UNUSED(user_data)
    const char *iface = gbinder_remote_request_interface(req);

    if (!g_strcmp0(iface, AGNSS_CALLBACK)) {
        GBinderReader reader;

        gbinder_remote_request_init_reader(req, &reader);
        switch (code) {
        case AGNSS_STATUS_IP_V4_CB:
            {
            QHostAddress ipv4;
            QHostAddress ipv6;
            QByteArray ssid;
            QByteArray password;

            const AGnssStatusIpV4 *status = geoclue_binder_gnss_decode_struct
                (AGnssStatusIpV4, &reader);

            ipv4.setAddress(status->ipV4Addr);

            QMetaObject::invokeMethod(staticProvider, "agpsStatus", Qt::QueuedConnection,
                                      Q_ARG(qint16, status->type), Q_ARG(quint16, status->status),
                                      Q_ARG(QHostAddress, ipv4), Q_ARG(QHostAddress, ipv6),
                                      Q_ARG(QByteArray, ssid), Q_ARG(QByteArray, password));
            }
            break;
        case AGNSS_STATUS_IP_V6_CB:
            {
            QHostAddress ipv4;
            QHostAddress ipv6;
            QByteArray ssid;
            QByteArray password;

            const AGnssStatusIpV6 *status = geoclue_binder_gnss_decode_struct
                (AGnssStatusIpV6, &reader);

            ipv6.setAddress(status->ipV6Addr.data);

            QMetaObject::invokeMethod(staticProvider, "agpsStatus", Qt::QueuedConnection,
                                      Q_ARG(qint16, status->type), Q_ARG(quint16, status->status),
                                      Q_ARG(QHostAddress, ipv4), Q_ARG(QHostAddress, ipv6),
                                      Q_ARG(QByteArray, ssid), Q_ARG(QByteArray, password));
            }
            break;
        default:
            qWarning("Failed to decode callback %u\n", code);
            break;
        }
        *status = GBINDER_STATUS_OK;
        return gbinder_local_reply_append_int32(gbinder_local_object_new_reply(obj), 0);
    } else {
        qWarning("Unknown interface %s and code %u\n", iface, code);
        *status = GBINDER_STATUS_FAILED;
    }
    return Q_NULLPTR;
}


static GBinderLocalReply *geoclue_binder_agnss_ril_callback(
    GBinderLocalObject *obj,
    GBinderRemoteRequest *req,
    guint code,
    guint flags,
    int *status,
    void *user_data)
{
    Q_UNUSED(flags)
    Q_UNUSED(user_data)
    const char *iface = gbinder_remote_request_interface(req);

    if (!g_strcmp0(iface, AGNSS_RIL_CALLBACK)) {
        GBinderReader reader;

        gbinder_remote_request_init_reader(req, &reader);
        switch (code) {
        case AGNSS_RIL_REQUEST_REF_ID_CB:
            qCDebug(lcGeoclueHybris) << "AGNSS RIL request ref ID";
            break;
        case AGNSS_RIL_REQUEST_REF_LOC_CB:
            qCDebug(lcGeoclueHybris) << "AGNSS RIL request ref location";
            break;
        default:
            qWarning("Failed to decode callback %u\n", code);
            break;
        }
        *status = GBINDER_STATUS_OK;
        return gbinder_local_reply_append_int32(gbinder_local_object_new_reply(obj), 0);
    } else {
        qWarning("Unknown interface %s and code %u\n", iface, code);
        *status = GBINDER_STATUS_FAILED;
    }
    return Q_NULLPTR;
}


static GBinderLocalReply *geoclue_binder_gnss_ni_callback(
    GBinderLocalObject *obj,
    GBinderRemoteRequest *req,
    guint code,
    guint flags,
    int *status,
    void *user_data)
{
    Q_UNUSED(flags)
    Q_UNUSED(user_data)
    const char *iface = gbinder_remote_request_interface(req);

    if (!g_strcmp0(iface, GNSS_NI_CALLBACK)) {
        GBinderReader reader;

        gbinder_remote_request_init_reader(req, &reader);
        switch (code) {
        case GNSS_NI_NOTIFY_CB:
            qCDebug(lcGeoclueHybris) << "GNSS NI notify";
            break;
        default:
            qWarning("Failed to decode callback %u\n", code);
            break;
        }
        *status = GBINDER_STATUS_OK;
        return gbinder_local_reply_append_int32(gbinder_local_object_new_reply(obj), 0);
    } else {
        qWarning("Unknown interface %s and code %u\n", iface, code);
        *status = GBINDER_STATUS_FAILED;
    }
    return Q_NULLPTR;
}

static void geoclue_binder_gnss_gnss_died(
    GBinderRemoteObject */*obj*/,
    void *user_data)
{
    BinderLocationBackend *self = (BinderLocationBackend *)user_data;
    self->dropGnss();
}


/*==========================================================================*
 * Backend class
 *==========================================================================*/

BinderLocationBackend::BinderLocationBackend(QObject *parent)
:   HybrisLocationBackend(parent), m_death_id(0), m_fqname(Q_NULLPTR), m_sm(Q_NULLPTR),
    m_clientGnss(Q_NULLPTR), m_remoteGnss(Q_NULLPTR), m_callbackGnss(Q_NULLPTR),
    m_clientGnssDebug(Q_NULLPTR), m_remoteGnssDebug(Q_NULLPTR),
    m_clientGnssNi(Q_NULLPTR), m_remoteGnssNi(Q_NULLPTR), m_callbackGnssNi(Q_NULLPTR),
    m_clientGnssXtra(Q_NULLPTR), m_remoteGnssXtra(Q_NULLPTR), m_callbackGnssXtra(Q_NULLPTR),
    m_clientAGnss(Q_NULLPTR), m_remoteAGnss(Q_NULLPTR), m_callbackAGnss(Q_NULLPTR),
    m_clientAGnssRil(Q_NULLPTR), m_remoteAGnssRil(Q_NULLPTR), m_callbackAGnssRil(Q_NULLPTR)
{
}

BinderLocationBackend::~BinderLocationBackend()
{
    dropGnss();
}

void BinderLocationBackend::dropGnss()
{
    if (m_callbackGnss) {
        gbinder_local_object_drop(m_callbackGnss);
        m_callbackGnss = Q_NULLPTR;
    }
    if (m_remoteGnss) {
        gbinder_remote_object_remove_handler(m_remoteGnss, m_death_id);
        gbinder_remote_object_unref(m_remoteGnss);
        m_death_id = 0;
        m_remoteGnss = Q_NULLPTR;
    }
    if (m_remoteGnssDebug) {
        gbinder_remote_object_unref(m_remoteGnssDebug);
        m_remoteGnssDebug = Q_NULLPTR;
    }
    if (m_callbackGnssNi) {
        gbinder_local_object_drop(m_callbackGnssNi);
        m_callbackGnssNi = Q_NULLPTR;
    }
    if (m_remoteGnssNi) {
        gbinder_remote_object_unref(m_remoteGnssNi);
        m_remoteGnssNi = Q_NULLPTR;
    }
    if (m_callbackGnssXtra) {
        gbinder_local_object_drop(m_callbackGnssXtra);
        m_callbackGnssXtra = Q_NULLPTR;
    }
    if (m_remoteGnssXtra) {
        gbinder_remote_object_unref(m_remoteGnssXtra);
        m_remoteGnssXtra = Q_NULLPTR;
    }
    if (m_callbackAGnss) {
        gbinder_local_object_drop(m_callbackAGnss);
        m_callbackAGnss = Q_NULLPTR;
    }
    if (m_remoteAGnss) {
        gbinder_remote_object_unref(m_remoteAGnss);
        m_remoteAGnss = Q_NULLPTR;
    }
    if (m_callbackAGnssRil) {
        gbinder_local_object_drop(m_callbackAGnssRil);
        m_callbackAGnssRil = Q_NULLPTR;
    }
    if (m_remoteAGnssRil) {
        gbinder_remote_object_unref(m_remoteAGnssRil);
        m_remoteAGnssRil = Q_NULLPTR;
    }
}

bool BinderLocationBackend::isReplySuccess(GBinderRemoteReply *reply)
{
    GBinderReader reader;
    gint32 status;
    gboolean result;
    gbinder_remote_reply_init_reader(reply, &reader);

    if (!gbinder_reader_read_int32(&reader, &status) || status != 0) {
        return false;
    }
    if (!gbinder_reader_read_bool(&reader, &result) || !result) {
        return false;
    }

    return true;
}

GBinderRemoteObject *BinderLocationBackend::getExtensionObject(GBinderRemoteReply *reply)
{
    GBinderReader reader;
    gint32 status;
    gbinder_remote_reply_init_reader(reply, &reader);

    if (!gbinder_reader_read_int32(&reader, &status) || status != 0) {
        qWarning("Failed to get extension object %d\n", status);
        return Q_NULLPTR;
    }

    return gbinder_reader_read_object(&reader);
}

// Gnss
bool BinderLocationBackend::gnssInit()
{
    bool ret = false;

    qWarning("Initialising GNSS interface\n");

    m_sm = gbinder_servicemanager_new(GNSS_BINDER_DEFAULT_DEV);
    if (m_sm) {
        int status = 0;

        /* Fetch remote reference from hwservicemanager */
        m_fqname = g_strconcat(GNSS_REMOTE "/default", Q_NULLPTR);
        m_remoteGnss = gbinder_servicemanager_get_service_sync(m_sm,
            m_fqname, &status);

        if (m_remoteGnss) {
            GBinderLocalRequest *req;
            GBinderRemoteReply *reply;

            /* get_service returns auto-released reference,
             * we need to add a reference of our own */
            gbinder_remote_object_ref(m_remoteGnss);
            m_clientGnss = gbinder_client_new(m_remoteGnss, GNSS_REMOTE);
            m_death_id = gbinder_remote_object_add_death_handler
                (m_remoteGnss, geoclue_binder_gnss_gnss_died, this);
            m_callbackGnss = gbinder_servicemanager_new_local_object
                (m_sm, GNSS_CALLBACK, geoclue_binder_gnss_callback, this);

            /* IGnss::setCallback */
            req = gbinder_client_new_request(m_clientGnss);
            gbinder_local_request_append_local_object(req, m_callbackGnss);
            reply = gbinder_client_transact_sync_reply(m_clientGnss,
                GNSS_SET_CALLBACK, req, &status);

            if (!status) {
                ret = isReplySuccess(reply);
            }

            gbinder_local_request_unref(req);
            gbinder_remote_reply_unref(reply);
        }
    }

    if (!ret) {
        qWarning("Failed to initialise GNSS interface\n");
    }
    return ret;
}

bool BinderLocationBackend::gnssStart()
{
    bool ret = false;

    if (m_clientGnss) {
        int status = 0;
        GBinderRemoteReply *reply;

        reply = gbinder_client_transact_sync_reply(m_clientGnss,
            GNSS_START, Q_NULLPTR, &status);

        if (!status) {
            ret = isReplySuccess(reply);
        }

        gbinder_remote_reply_unref(reply);
    }

    if (!ret) {
        qWarning("Failed to start positioning\n");
    }
    return ret;
}

bool BinderLocationBackend::gnssStop()
{
    bool ret = false;

    if (m_clientGnss) {
        int status = 0;
        GBinderRemoteReply *reply;

        reply = gbinder_client_transact_sync_reply(m_clientGnss,
            GNSS_STOP, Q_NULLPTR, &status);

        if (!status) {
            ret = isReplySuccess(reply);
        }

        gbinder_remote_reply_unref(reply);
    }

    if (!ret) {
        qWarning("Failed to stop positioning\n");
    }
    return ret;
}

void BinderLocationBackend::gnssCleanup()
{
    if (m_clientGnss) {
        const int status = gbinder_client_transact_sync_oneway(m_clientGnss,
            GNSS_CLEANUP, Q_NULLPTR);

        if (status) {
            qWarning("Failed to cleanup\n");
        }
    }
}

bool BinderLocationBackend::gnssInjectLocation(double latitudeDegrees, double longitudeDegrees, float accuracyMeters)
{
    bool ret = false;

    if (m_clientGnss) {
        int status = 0;

        GBinderLocalRequest *req;
        GBinderRemoteReply *reply;
        GBinderWriter writer;

        req = gbinder_client_new_request(m_clientGnss);
        gbinder_local_request_init_writer(req, &writer);
        gbinder_writer_append_double(&writer, latitudeDegrees);
        gbinder_writer_append_double(&writer, longitudeDegrees);
        gbinder_writer_append_float(&writer, accuracyMeters);
        reply = gbinder_client_transact_sync_reply(m_clientGnss,
            GNSS_INJECT_LOCATION, req, &status);

        if (!status) {
            ret = isReplySuccess(reply);
        }
        if (!ret) {
            qWarning("Failed to inject location\n");
        }

        gbinder_local_request_unref(req);
        gbinder_remote_reply_unref(reply);
    }
    return ret;
}

bool BinderLocationBackend::gnssInjectTime(HybrisGnssUtcTime timeMs, int64_t timeReferenceMs, int32_t uncertaintyMs)
{
    bool ret = false;

    if (m_clientGnss) {
        int status = 0;
        GBinderLocalRequest *req;
        GBinderRemoteReply *reply;
        GBinderWriter writer;

        req = gbinder_client_new_request(m_clientGnss);
        gbinder_local_request_init_writer(req, &writer);
        gbinder_writer_append_int64(&writer, timeMs);
        gbinder_writer_append_int64(&writer, timeReferenceMs);
        gbinder_writer_append_int32(&writer, uncertaintyMs);

        reply = gbinder_client_transact_sync_reply(m_clientGnss,
            GNSS_INJECT_TIME, req, &status);

        if (!status) {
            ret = isReplySuccess(reply);
        }

        if (!ret) {
            qWarning("Failed to inject time\n");
        }
        gbinder_local_request_unref(req);
        gbinder_remote_reply_unref(reply);
    }
    return ret;
}

void BinderLocationBackend::gnssDeleteAidingData(HybrisGnssAidingData aidingDataFlags)
{
    if (m_clientGnss) {
        GBinderLocalRequest *req;

        req = gbinder_client_new_request(m_clientGnss);
        gbinder_local_request_append_int32(req, aidingDataFlags);
        gbinder_client_transact_sync_oneway(m_clientGnss,
            GNSS_DELETE_AIDING_DATA, req);

        gbinder_local_request_unref(req);
    }
}

bool BinderLocationBackend::gnssSetPositionMode(HybrisGnssPositionMode mode, HybrisGnssPositionRecurrence recurrence,
                                       uint32_t minIntervalMs, uint32_t preferredAccuracyMeters,
                                       uint32_t preferredTimeMs)
{
    bool ret = false;

    if (m_clientGnss) {
        int status = 0;
        GBinderLocalRequest *req;
        GBinderRemoteReply *reply;
        GBinderWriter writer;

        req = gbinder_client_new_request(m_clientGnss);
        gbinder_local_request_init_writer(req, &writer);
        gbinder_writer_append_int32(&writer, mode);
        gbinder_writer_append_int32(&writer, recurrence);
        gbinder_writer_append_int32(&writer, minIntervalMs);
        gbinder_writer_append_int32(&writer, preferredAccuracyMeters);
        gbinder_writer_append_int32(&writer, preferredTimeMs);
        reply = gbinder_client_transact_sync_reply(m_clientGnss,
            GNSS_SET_POSITION_MODE, req, &status);

        if (!status) {
            ret = isReplySuccess(reply);
        }

        if (!ret) {
            qWarning("GNSS set position mode failed\n");
        }
        gbinder_local_request_unref(req);
        gbinder_remote_reply_unref(reply);
    }
    return ret;
}

// GnssDebug
void BinderLocationBackend::gnssDebugInit()
{
    GBinderRemoteReply *reply;
    int status = 0;

    reply = gbinder_client_transact_sync_reply(m_clientGnss,
        GNSS_GET_EXTENSION_GNSS_DEBUG, Q_NULLPTR, &status);

    if (!status) {
        m_remoteGnssDebug = getExtensionObject(reply);
        if (m_remoteGnssDebug) {
            qWarning("Initialising GNSS Debug interface\n");
            m_clientGnssDebug = gbinder_client_new(m_remoteGnssDebug, GNSS_DEBUG_REMOTE);
        }
    }
    gbinder_remote_reply_unref(reply);
}

// GnnNi
void BinderLocationBackend::gnssNiInit()
{
    GBinderRemoteReply *reply;
    int status = 0;

    reply = gbinder_client_transact_sync_reply(m_clientGnss,
        GNSS_GET_EXTENSION_GNSS_NI, Q_NULLPTR, &status);

    if (!status) {
        m_remoteGnssNi = getExtensionObject(reply);

        if (m_remoteGnssNi) {
            qWarning("Initialising GNSS NI interface\n");
            GBinderLocalRequest *req;
            m_clientGnssNi = gbinder_client_new(m_remoteGnssNi, GNSS_NI_REMOTE);
            m_callbackGnssNi = gbinder_servicemanager_new_local_object
                (m_sm, GNSS_NI_CALLBACK, geoclue_binder_gnss_ni_callback, this);

            gbinder_remote_reply_unref(reply);

            /* IGnssNi::setCallback */
            req = gbinder_client_new_request(m_clientGnssNi);
            gbinder_local_request_append_local_object(req, m_callbackGnssNi);
            reply = gbinder_client_transact_sync_reply(m_clientGnssNi,
                GNSS_NI_SET_CALLBACK, req, &status);

            if (!status) {
                if (!gbinder_remote_reply_read_int32(reply, &status) || status != 0) {
                    qWarning("Initialising GNSS NI interface failed %d\n", status);
                }
            }
            gbinder_local_request_unref(req);
        }
    }
    gbinder_remote_reply_unref(reply);
}

void BinderLocationBackend::gnssNiRespond(int32_t notifId, HybrisGnssUserResponseType userResponse)
{
    if (m_clientGnssNi) {
        int status = 0;
        GBinderLocalRequest *req;
        GBinderRemoteReply *reply;
        GBinderWriter writer;

        req = gbinder_client_new_request(m_clientGnssNi);
        gbinder_local_request_init_writer(req, &writer);
        gbinder_writer_append_int32(&writer, notifId);
        gbinder_writer_append_int32(&writer, userResponse);

        reply = gbinder_client_transact_sync_reply(m_clientGnssNi,
            GNSS_NI_RESPOND, req, &status);

        if (!status) {
            if (!gbinder_remote_reply_read_int32(reply, &status) || status != 0) {
                qWarning("GNSS NI respond failed %d\n", status);
            }
        }

        gbinder_local_request_unref(req);
        gbinder_remote_reply_unref(reply);
    }
}

// GnssXtra
void BinderLocationBackend::gnssXtraInit()
{
    GBinderRemoteReply *reply;
    int status = 0;

    reply = gbinder_client_transact_sync_reply(m_clientGnss,
        GNSS_GET_EXTENSION_XTRA, Q_NULLPTR, &status);

    if (!status) {
        m_remoteGnssXtra = getExtensionObject(reply);

        if (m_remoteGnssXtra) {
            qWarning("Initialising GNSS Xtra interface\n");
            GBinderLocalRequest *req;
            m_clientGnssXtra = gbinder_client_new(m_remoteGnssXtra, GNSS_XTRA_REMOTE);
            m_callbackGnssXtra = gbinder_servicemanager_new_local_object
                (m_sm, GNSS_XTRA_CALLBACK, geoclue_binder_gnss_xtra_callback, this);

            gbinder_remote_reply_unref(reply);

            /* IGnssXtra::setCallback */
            req = gbinder_client_new_request(m_clientGnssXtra);
            gbinder_local_request_append_local_object(req, m_callbackGnssXtra);
            reply = gbinder_client_transact_sync_reply(m_clientGnssXtra,
                GNSS_XTRA_SET_CALLBACK, req, &status);

            if (status || !isReplySuccess(reply)) {
                qWarning("Initialising GNSS Xtra interface failed\n");
            }
            gbinder_local_request_unref(req);
        }
    }
    gbinder_remote_reply_unref(reply);
}

bool BinderLocationBackend::gnssXtraInjectXtraData(QByteArray &xtraData)
{
    bool ret = false;
    if (m_clientGnssXtra) {
        int status = 0;

        GBinderLocalRequest *req;
        GBinderRemoteReply *reply;

        req = gbinder_client_new_request(m_clientGnssXtra);
        gbinder_local_request_append_hidl_string(req, xtraData.constData());
        reply = gbinder_client_transact_sync_reply(m_clientGnssXtra,
            GNSS_XTRA_INJECT_XTRA_DATA, req, &status);

        if (!status) {
            ret = isReplySuccess(reply);
        }

        if (!ret) {
            qWarning("GNSS Xtra inject xtra data failed\n");
        }
        gbinder_local_request_unref(req);
        gbinder_remote_reply_unref(reply);
    }
    return ret;
}

// AGnss
void BinderLocationBackend::aGnssInit()
{
    GBinderRemoteReply *reply;
    int status = 0;

    reply = gbinder_client_transact_sync_reply(m_clientGnss,
        GNSS_GET_EXTENSION_AGNSS, Q_NULLPTR, &status);

    if (!status) {
        m_remoteAGnss = getExtensionObject(reply);

        if (m_remoteAGnss) {
            qWarning("Initialising AGNSS interface\n");
            GBinderLocalRequest *req;
            m_clientAGnss = gbinder_client_new(m_remoteAGnss, AGNSS_REMOTE);
            m_callbackAGnss = gbinder_servicemanager_new_local_object
                (m_sm, AGNSS_CALLBACK, geoclue_binder_agnss_callback, this);

            gbinder_remote_reply_unref(reply);

            /* IAGnss::setCallback */
            req = gbinder_client_new_request(m_clientAGnss);
            gbinder_local_request_append_local_object(req, m_callbackAGnss);
            reply = gbinder_client_transact_sync_reply(m_clientAGnss,
                AGNSS_SET_CALLBACK, req, &status);

            if (!status) {
                if (!gbinder_remote_reply_read_int32(reply, &status) || status != 0) {
                    qWarning("Initialising AGNSS interface failed %d\n", status);
                }
            }
            gbinder_local_request_unref(req);
        }
    }
    gbinder_remote_reply_unref(reply);
}

bool BinderLocationBackend::aGnssDataConnClosed()
{
    int status = 0;
    bool ret = false;
    GBinderRemoteReply *reply;

    reply = gbinder_client_transact_sync_reply(m_clientAGnss,
        AGNSS_DATA_CONN_CLOSED, Q_NULLPTR, &status);

    if (!status) {
        ret = isReplySuccess(reply);
    }

    gbinder_remote_reply_unref(reply);
    return ret;
}

bool BinderLocationBackend::aGnssDataConnFailed()
{
    int status = 0;
    bool ret = false;
    GBinderRemoteReply *reply;

    reply = gbinder_client_transact_sync_reply(m_clientAGnss,
        AGNSS_DATA_CONN_FAILED, Q_NULLPTR, &status);

    if (!status) {
        ret = isReplySuccess(reply);
    }

    gbinder_remote_reply_unref(reply);
    return ret;
}

bool BinderLocationBackend::aGnssDataConnOpen(const QByteArray &apn, const QString &protocol)
{
    int status = 0;
    bool ret = false;
    GBinderLocalRequest *req;
    GBinderRemoteReply *reply;
    GBinderWriter writer;

    req = gbinder_client_new_request(m_clientAGnss);

    gbinder_local_request_init_writer(req, &writer);
    gbinder_writer_append_hidl_string(&writer, apn.constData());
    gbinder_writer_append_int32(&writer, fromContextProtocol(protocol));
    reply = gbinder_client_transact_sync_reply(m_clientAGnss,
        AGNSS_DATA_CONN_OPEN, req, &status);

    if (!status) {
        ret = isReplySuccess(reply);
    }

    gbinder_local_request_unref(req);
    gbinder_remote_reply_unref(reply);

    return ret;
}

// AGnssRil
void BinderLocationBackend::aGnssRilInit()
{
    GBinderRemoteReply *reply;
    int status = 0;

    reply = gbinder_client_transact_sync_reply(m_clientGnss,
        GNSS_GET_EXTENSION_AGNSS_RIL, Q_NULLPTR, &status);

    if (!status) {
        m_remoteAGnssRil = getExtensionObject(reply);

        if (m_remoteAGnssRil) {
            qWarning("Initialising AGNSS RIL interface\n");
            GBinderLocalRequest *req;
            m_clientAGnssRil = gbinder_client_new(m_remoteAGnssRil, AGNSS_RIL_REMOTE);
            m_callbackAGnssRil = gbinder_servicemanager_new_local_object
                (m_sm, AGNSS_RIL_CALLBACK, geoclue_binder_agnss_ril_callback, this);

            gbinder_remote_reply_unref(reply);

            /* IAGnssRil::setCallback */
            req = gbinder_client_new_request(m_clientAGnssRil);
            gbinder_local_request_append_local_object(req, m_callbackAGnssRil);
            reply = gbinder_client_transact_sync_reply(m_clientAGnssRil,
                AGNSS_RIL_SET_CALLBACK, req, &status);

            if (!status) {
                if (!gbinder_remote_reply_read_int32(reply, &status) || status != 0) {
                    qWarning("Initialising AGNSS RIL interface failed %d\n", status);
                }
            }
            gbinder_local_request_unref(req);
        }
    }
    gbinder_remote_reply_unref(reply);
}
