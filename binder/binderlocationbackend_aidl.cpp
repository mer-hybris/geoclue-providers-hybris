/*
    Copyright (c) 2015 - 2021 Jolla Ltd.
    Copyright (c) 2018 Matti Lehtimäki <matti.lehtimaki@gmail.com>
    Copyright (c) 2025 Jolla Mobile Ltd

    This file is part of geoclue-hybris.

    Geoclue-hybris is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License.
*/

#include "binderlocationbackend_aidl.h"

#include "hybrisprovider.h"

#include <QtNetwork/QHostAddress>

#define GNSS_BINDER_DEFAULT_DEV  "/dev/binder"

enum GnssAidlFunctions {
    GNSS_AIDL_SET_CALLBACK = 1,
    GNSS_AIDL_CLOSE = 2,
    GNSS_AIDL_GET_EXTENSION_PDSD = 3,
    GNSS_AIDL_GET_EXTENSION_GNSS_CONFIGURATION = 4,
    GNSS_AIDL_GET_EXTENSION_GNSS_MEASUREMENT = 5,
    GNSS_AIDL_GET_EXTENSION_GNSS_POWER_INDICATION = 6,
    GNSS_AIDL_GET_EXTENSION_GNSS_BATCHING = 7,
    GNSS_AIDL_GET_EXTENSION_GNSS_GEOFENCING = 8,
    GNSS_AIDL_GET_EXTENSION_GNSS_NAVIGATION_MESSAGE = 9,
    GNSS_AIDL_GET_EXTENSION_AGNSS = 10,
    GNSS_AIDL_GET_EXTENSION_AGNSS_RIL = 11,
    GNSS_AIDL_GET_EXTENSION_GNSS_DEBUG = 12,
    GNSS_AIDL_GET_EXTENSION_GNSS_VISIBILITY_CONTROL = 13,
    GNSS_AIDL_START = 14,
    GNSS_AIDL_STOP = 15,
    GNSS_AIDL_INJECT_TIME = 16,
    GNSS_AIDL_INJECT_LOCATION = 17,
    GNSS_AIDL_INJECT_BEST_LOCATION = 18,
    GNSS_AIDL_DELETE_AIDING_DATA = 19,
    GNSS_AIDL_SET_POSITION_MODE = 20,
    GNSS_AIDL_GET_EXTENSION_GNSS_ANTENNA_INFO = 21,
    GNSS_AIDL_GET_EXTENSION_MEASUREMENT_CORRECTIONS = 22,
    GNSS_AIDL_START_SV_STATUS = 23,
    GNSS_AIDL_STOP_SV_STATUS = 24,
    GNSS_AIDL_START_NMEA = 25,
    GNSS_AIDL_STOP_NMEA = 26
};

enum GnssAidlCallbacks {
    GNSS_AIDL_SET_CAPABILITIES_CB = 1,
    GNSS_AIDL_STATUS_CB = 2,
    GNSS_AIDL_SV_STATUS_CB = 3,
    GNSS_AIDL_LOCATION_CB = 4,
    GNSS_AIDL_NMEA_CB = 5,
    GNSS_AIDL_ACQUIRE_WAKELOCK_CB = 6,
    GNSS_AIDL_RELEASE_WAKELOCK_CB = 7,
    GNSS_AIDL_SET_SYSTEM_INFO_CB = 8,
    GNSS_AIDL_REQUEST_TIME_CB = 9,
    GNSS_AIDL_REQUEST_LOCATION_CB = 10
};

enum GnssAidlDebugFunctions {
    GNSS_AIDL_DEBUG_GET_DEBUG_DATA = 1
};

enum GnssAidlNfwFunctions {
    GNSS_AIDL_VISIBILITY_CONTROL_RESPOND = 1,
    GNSS_AIDL_VISIBILITY_CONTROL_SET_CALLBACK = 2
};

enum GnssAidlNfwCallbacks {
    GNSS_AIDL_NFW_NOTIFY_CB = 1
};

enum GnssAidlPsdsFunctions {
    GNSS_AIDL_PSDS_INJECT_PSDS_DATA = 1,
    GNSS_AIDL_PSDS_SET_CALLBACK = 2
};

enum GnssAidlPsdsCallbacks {
    GNSS_AIDL_PSDS_DOWNLOAD_REQUEST_CB = 1
};

enum AGnssAidlFunctions {
    AGNSS_AIDL_SET_CALLBACK = 1,
    AGNSS_AIDL_DATA_CONN_CLOSED = 2,
    AGNSS_AIDL_DATA_CONN_FAILED = 3,
    AGNSS_AIDL_SET_SERVER = 4,
    AGNSS_AIDL_DATA_CONN_OPEN = 5
};

enum AGnssAidlCallbacks {
    AGNSS_AIDL_STATUS_CB = 1
};

enum AGnssAidlRilFunctions {
    AGNSS_AIDL_RIL_SET_CALLBACK = 1,
    AGNSS_AIDL_RIL_SET_REF_LOCATION = 2,
    AGNSS_AIDL_RIL_SET_ID = 3,
    AGNSS_AIDL_RIL_UPDATE_NETWORK_STATE = 4,
    AGNSS_AIDL_RIL_INJECT_NI_SUPL_MESSAGE_DATA = 5
};

enum AGnssAidlRilCallbacks {
    AGNSS_AIDL_RIL_REQUEST_SET_ID_CB = 1,
    AGNSS_AIDL_RIL_REQUEST_REF_LOC_CB = 2
};

#define GNSS_AIDL_IFACE(x)                    "android.hardware.gnss." x
#define GNSS_AIDL_REMOTE                       GNSS_AIDL_IFACE("IGnss")
#define GNSS_AIDL_CALLBACK                     GNSS_AIDL_IFACE("IGnssCallback")
#define GNSS_AIDL_DEBUG_REMOTE                 GNSS_AIDL_IFACE("IGnssDebug")
#define GNSS_AIDL_VISIBILITY_CONTROL_REMOTE    GNSS_AIDL_IFACE("visibility_control.IGnssVisibilityControl")
#define GNSS_AIDL_VISIBILITY_CONTROL_CALLBACK  GNSS_AIDL_IFACE("visibility_control.IGnssVisibilityControlCallback")
#define GNSS_AIDL_PSDS_REMOTE                  GNSS_AIDL_IFACE("IGnssPsds")
#define GNSS_AIDL_PSDS_CALLBACK                GNSS_AIDL_IFACE("IGnssPsdsCallback")
#define AGNSS_AIDL_REMOTE                      GNSS_AIDL_IFACE("IAGnss")
#define AGNSS_AIDL_CALLBACK                    GNSS_AIDL_IFACE("IAGnssCallback")
#define AGNSS_AIDL_RIL_REMOTE                  GNSS_AIDL_IFACE("IAGnssRil")
#define AGNSS_AIDL_RIL_CALLBACK                GNSS_AIDL_IFACE("IAGnssRilCallback")


/*==========================================================================*
 * Implementation
 *==========================================================================*/

namespace
{

const double MpsToKnots = 1.943844;

GBinderLocalReply *geoclue_binder_gnss_callback(
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

    if (!g_strcmp0(iface, GNSS_AIDL_CALLBACK)) {
        GBinderReader reader;
        gbinder_remote_request_init_reader(req, &reader);
        switch (code) {
        case GNSS_AIDL_LOCATION_CB:
            {
            Location loc;
            GnssAidlLocation location;

            /* Non-null parcelable */
            gbinder_reader_read_int32(&reader, NULL);
            /* Parcelable size */
            gbinder_reader_read_int32(&reader, NULL);

            gbinder_reader_read_int32(&reader, &location.gnssLocationFlags);
            gbinder_reader_read_double(&reader, &location.latitudeDegrees);
            gbinder_reader_read_double(&reader, &location.longitudeDegrees);
            gbinder_reader_read_double(&reader, &location.altitudeMeters);
            gbinder_reader_read_double(&reader, &location.speedMetersPerSec);
            gbinder_reader_read_double(&reader, &location.bearingDegrees);
            gbinder_reader_read_double(&reader, &location.horizontalAccuracyMeters);
            gbinder_reader_read_double(&reader, &location.verticalAccuracyMeters);
            gbinder_reader_read_double(&reader, &location.speedAccuracyMetersPerSecond);
            gbinder_reader_read_double(&reader, &location.bearingAccuracyDegrees);
            gbinder_reader_read_int64(&reader, &location.timestamp);

            loc.setTimestamp(location.timestamp);

            if (location.gnssLocationFlags & HYBRIS_GNSS_LOCATION_HAS_LAT_LONG) {
                loc.setLatitude(location.latitudeDegrees);
                loc.setLongitude(location.longitudeDegrees);
            }

            if (location.gnssLocationFlags & HYBRIS_GNSS_LOCATION_HAS_ALTITUDE)
                loc.setAltitude(location.altitudeMeters);

            if (location.gnssLocationFlags & HYBRIS_GNSS_LOCATION_HAS_SPEED)
                loc.setSpeed(location.speedMetersPerSec * MpsToKnots);

            if (location.gnssLocationFlags & HYBRIS_GNSS_LOCATION_HAS_BEARING)
                loc.setDirection(location.bearingDegrees);

            if ((location.gnssLocationFlags & HYBRIS_GNSS_LOCATION_HAS_HORIZONTAL_ACCURACY) ||
                (location.gnssLocationFlags & HYBRIS_GNSS_LOCATION_HAS_VERTICAL_ACCURACY)) {
                Accuracy accuracy;
                if (location.gnssLocationFlags & HYBRIS_GNSS_LOCATION_HAS_HORIZONTAL_ACCURACY) {
                    accuracy.setHorizontal(location.horizontalAccuracyMeters);
                }
                if (location.gnssLocationFlags & HYBRIS_GNSS_LOCATION_HAS_VERTICAL_ACCURACY) {
                    accuracy.setVertical(location.verticalAccuracyMeters);
                }
                loc.setAccuracy(accuracy);
            }

            QMetaObject::invokeMethod(staticProvider, "setLocation", Qt::QueuedConnection,
                                      Q_ARG(Location, loc));
            }
            break;
        case GNSS_AIDL_STATUS_CB:
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
        case GNSS_AIDL_SV_STATUS_CB:
            {
            QList<SatelliteInfo> satellites;
            QList<int> usedPrns;
            gint32 numSvs = 0;
            gbinder_reader_read_int32(&reader, &numSvs);
            for (int i = 0; i < numSvs; ++i) {
                GnssAidlSvInfo svInfo;

                /* Non-null parcelable */
                gbinder_reader_read_int32(&reader, NULL);
                /* Parcelable size */
                gbinder_reader_read_int32(&reader, NULL);

                gbinder_reader_read_int32(&reader, &svInfo.svid);
                gbinder_reader_read_int32(&reader, (gint32 *)&svInfo.constellation);
                gbinder_reader_read_float(&reader, &svInfo.cN0Dbhz);
                gbinder_reader_read_float(&reader, &svInfo.basebandCN0DbHz);
                gbinder_reader_read_float(&reader, &svInfo.elevationDegrees);
                gbinder_reader_read_float(&reader, &svInfo.azimuthDegrees);
                gbinder_reader_read_int64(&reader, &svInfo.carrierFrequencyHz);
                gbinder_reader_read_int32(&reader, &svInfo.svFlag);

                SatelliteInfo satInfo;
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
                    usedPrns.append(prn);
            }

            QMetaObject::invokeMethod(staticProvider, "setSatellite", Qt::QueuedConnection,
                                      Q_ARG(QList<SatelliteInfo>, satellites),
                                      Q_ARG(QList<int>, usedPrns));
            }
            break;
        case GNSS_AIDL_NMEA_CB:
            {
            gint64 timestamp;
            if (gbinder_reader_read_int64(&reader, &timestamp)) {
                const char *nmeaData = gbinder_reader_read_string8(&reader);
                if (nmeaData) {
                    processNmea(timestamp, nmeaData);
                }
            }
            }
            break;
        case GNSS_AIDL_SET_CAPABILITIES_CB:
            {
            guint32 capabilities;
            if (gbinder_reader_read_uint32(&reader, &capabilities)) {
                qCDebug(lcGeoclueHybris) << "capabilities" << showbase << hex << capabilities;
            }
            }
            break;
        case GNSS_AIDL_ACQUIRE_WAKELOCK_CB:
        case GNSS_AIDL_RELEASE_WAKELOCK_CB:
            break;
        case GNSS_AIDL_REQUEST_TIME_CB:
            qCDebug(lcGeoclueHybris) << "GNSS request UTC time";
            QMetaObject::invokeMethod(staticProvider, "injectUtcTime", Qt::QueuedConnection);
            break;
        case GNSS_AIDL_REQUEST_LOCATION_CB:
            qCDebug(lcGeoclueHybris) << "GNSS request location";
            break;
        case GNSS_AIDL_SET_SYSTEM_INFO_CB:
            qCDebug(lcGeoclueHybris) << "GNSS set system info";
            break;
        default:
            qWarning("Failed to decode callback %u", code);
            break;
        }
        *status = GBINDER_STATUS_OK;
        return gbinder_local_reply_append_int32(gbinder_local_object_new_reply(obj), 0);
    } else {
        qWarning("Unknown interface %s and code %u", iface, code);
        *status = GBINDER_STATUS_FAILED;
    }
    return Q_NULLPTR;
}

GBinderLocalReply *geoclue_binder_gnss_xtra_callback(
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

    if (!g_strcmp0(iface, GNSS_AIDL_PSDS_CALLBACK)) {
        GBinderReader reader;

        gbinder_remote_request_init_reader(req, &reader);
        switch (code) {
        case GNSS_AIDL_PSDS_DOWNLOAD_REQUEST_CB:
            QMetaObject::invokeMethod(staticProvider, "xtraDownloadRequest", Qt::QueuedConnection);
            break;
        default:
            qWarning("Failed to decode callback %u", code);
            break;
        }
        *status = GBINDER_STATUS_OK;
        return gbinder_local_reply_append_int32(gbinder_local_object_new_reply(obj), 0);
    } else {
        qWarning("Unknown interface %s and code %u", iface, code);
        *status = GBINDER_STATUS_FAILED;
    }
    return Q_NULLPTR;
}

GBinderLocalReply *geoclue_binder_agnss_callback(
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

    if (!g_strcmp0(iface, AGNSS_AIDL_CALLBACK)) {
        GBinderReader reader;

        gbinder_remote_request_init_reader(req, &reader);
        switch (code) {
        case AGNSS_AIDL_STATUS_CB:
            {
            gint32 status;
            gint32 type;

            QHostAddress ipv4;
            QHostAddress ipv6;
            QByteArray ssid;
            QByteArray password;

            gbinder_reader_read_int32(&reader, &type);
            gbinder_reader_read_int32(&reader, &status);

            QMetaObject::invokeMethod(staticProvider, "agpsStatus", Qt::QueuedConnection,
                                      Q_ARG(qint16, type), Q_ARG(quint16, status),
                                      Q_ARG(QHostAddress, ipv4), Q_ARG(QHostAddress, ipv6),
                                      Q_ARG(QByteArray, ssid), Q_ARG(QByteArray, password));
            }
            break;
        default:
            qWarning("Failed to decode callback %u", code);
            break;
        }
        *status = GBINDER_STATUS_OK;
        return gbinder_local_reply_append_int32(gbinder_local_object_new_reply(obj), 0);
    } else {
        qWarning("Unknown interface %s and code %u", iface, code);
        *status = GBINDER_STATUS_FAILED;
    }
    return Q_NULLPTR;
}


GBinderLocalReply *geoclue_binder_agnss_ril_callback(
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

    if (!g_strcmp0(iface, AGNSS_AIDL_RIL_CALLBACK)) {
        GBinderReader reader;

        gbinder_remote_request_init_reader(req, &reader);
        switch (code) {
        case AGNSS_AIDL_RIL_REQUEST_SET_ID_CB:
            qCDebug(lcGeoclueHybris) << "AGNSS RIL request ref ID";
            break;
        case AGNSS_AIDL_RIL_REQUEST_REF_LOC_CB:
            qCDebug(lcGeoclueHybris) << "AGNSS RIL request ref location";
            break;
        default:
            qWarning("Failed to decode callback %u", code);
            break;
        }
        *status = GBINDER_STATUS_OK;
        return gbinder_local_reply_append_int32(gbinder_local_object_new_reply(obj), 0);
    } else {
        qWarning("Unknown interface %s and code %u", iface, code);
        *status = GBINDER_STATUS_FAILED;
    }
    return Q_NULLPTR;
}


GBinderLocalReply *geoclue_binder_gnss_ni_callback(
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

    if (!g_strcmp0(iface, GNSS_AIDL_VISIBILITY_CONTROL_CALLBACK)) {
        GBinderReader reader;

        gbinder_remote_request_init_reader(req, &reader);
        switch (code) {
        case GNSS_AIDL_NFW_NOTIFY_CB:
            qCDebug(lcGeoclueHybris) << "GNSS NFW notify";
            break;
        default:
            qWarning("Failed to decode callback %u", code);
            break;
        }
        *status = GBINDER_STATUS_OK;
        return gbinder_local_reply_append_int32(gbinder_local_object_new_reply(obj), 0);
    } else {
        qWarning("Unknown interface %s and code %u", iface, code);
        *status = GBINDER_STATUS_FAILED;
    }
    return Q_NULLPTR;
}

void geoclue_binder_gnss_gnss_died(
    GBinderRemoteObject *obj,
    void *user_data)
{
    Q_UNUSED(obj);
    BinderLocationBackendAidl *self = (BinderLocationBackendAidl *)user_data;
    self->dropGnss();
}

}

/*==========================================================================*
 * Backend class
 *==========================================================================*/

BinderLocationBackendAidl::BinderLocationBackendAidl(QObject *parent)
:   BinderLocationBackend(parent)
{
}

bool BinderLocationBackendAidl::isSupported()
{
    bool ret = false;
    if (qgetenv("GEOCLUE_HYBRIS_DISABLE_AIDL") == "1"){
        return false;
    }

    GBinderServiceManager *sm =
        gbinder_servicemanager_new(GNSS_BINDER_DEFAULT_DEV);

    if (sm) {
        /* Fetch remote reference from hwservicemanager */
        char *fqname = g_strconcat(GNSS_AIDL_REMOTE "/default", Q_NULLPTR);
        GBinderRemoteObject *remoteGnss =
            gbinder_servicemanager_get_service_sync(sm, fqname, NULL);

        if (remoteGnss) {
            ret = true;
        }
        g_free(fqname);
    }

    gbinder_servicemanager_unref(sm);

    return ret;
}

// Gnss
bool BinderLocationBackendAidl::gnssInit()
{
    bool ret = false;

    qWarning("Initialising GNSS interface");

    m_sm = gbinder_servicemanager_new(GNSS_BINDER_DEFAULT_DEV);
    if (m_sm) {
        int status = 0;

        /* Fetch remote reference from hwservicemanager */
        m_fqname = g_strconcat(GNSS_AIDL_REMOTE "/default", Q_NULLPTR);
        m_remoteGnss = gbinder_servicemanager_get_service_sync(m_sm,
            m_fqname, &status);

        if (m_remoteGnss) {
            GBinderLocalRequest *req;
            GBinderRemoteReply *reply;

            /* get_service returns auto-released reference,
             * we need to add a reference of our own */
            gbinder_remote_object_ref(m_remoteGnss);
            m_clientGnss = gbinder_client_new(m_remoteGnss, GNSS_AIDL_REMOTE);
            m_death_id = gbinder_remote_object_add_death_handler
                (m_remoteGnss, geoclue_binder_gnss_gnss_died, this);
            m_callbackGnss = gbinder_servicemanager_new_local_object
                (m_sm, GNSS_AIDL_CALLBACK, geoclue_binder_gnss_callback, this);
            gbinder_local_object_set_stability(m_callbackGnss,
                GBINDER_STABILITY_VINTF);

            /* IGnss::setCallback */
            req = gbinder_client_new_request(m_clientGnss);
            gbinder_local_request_append_local_object(req, m_callbackGnss);
            reply = gbinder_client_transact_sync_reply(m_clientGnss,
                GNSS_AIDL_SET_CALLBACK, req, &status);

            if (!status && isReplySuccess(reply)) {
                ret = true;
            }

            gbinder_local_request_unref(req);
            gbinder_remote_reply_unref(reply);
        }
    }

    if (!ret) {
        qWarning("Failed to initialise AIDL GNSS interface");
    }
    return ret;
}

bool BinderLocationBackendAidl::gnssStart()
{
    bool ret = false;

    if (m_clientGnss) {
        int status = 0;
        GBinderRemoteReply *reply;

        reply = gbinder_client_transact_sync_reply(m_clientGnss,
            GNSS_AIDL_START, Q_NULLPTR, &status);

        reply = gbinder_client_transact_sync_reply(m_clientGnss,
            GNSS_AIDL_START_SV_STATUS, Q_NULLPTR, &status);

        if (!status && isReplySuccess(reply)) {
            ret = true;
        }

        gbinder_remote_reply_unref(reply);
    }

    if (!ret) {
        qWarning("Failed to start positioning");
    }
    return ret;
}

bool BinderLocationBackendAidl::gnssStop()
{
    bool ret = false;

    if (m_clientGnss) {
        int status = 0;
        GBinderRemoteReply *reply;

        reply = gbinder_client_transact_sync_reply(m_clientGnss,
            GNSS_AIDL_STOP_SV_STATUS, Q_NULLPTR, &status);

        reply = gbinder_client_transact_sync_reply(m_clientGnss,
            GNSS_AIDL_STOP, Q_NULLPTR, &status);

        if (!status && isReplySuccess(reply)) {
            ret = true;
        }

        gbinder_remote_reply_unref(reply);
    }

    if (!ret) {
        qWarning("Failed to stop positioning");
    }
    return ret;
}

void BinderLocationBackendAidl::gnssCleanup()
{
    if (m_clientGnss) {
        gbinder_client_transact(m_clientGnss, GNSS_AIDL_CLOSE, 0, NULL, NULL, NULL, NULL);
    }
}

bool BinderLocationBackendAidl::gnssInjectLocation(
    int timestamp,
    double latitudeDegrees,
    double longitudeDegrees,
    float accuracyMeters)
{
    bool ret = false;

    if (m_clientGnss) {
        int status = 0;

        GBinderLocalRequest *req;
        GBinderRemoteReply *reply;
        GBinderWriter writer;

        req = gbinder_client_new_request(m_clientGnss);
        gbinder_local_request_init_writer(req, &writer);

        /* Non-null parcelable */
        gbinder_writer_append_int32(&writer, 1);
        /* Parcelable size */
        gbinder_writer_append_int32(&writer,
            sizeof(gint32) + 3 * sizeof(gdouble) + 6 * sizeof(gfloat));

        gbinder_writer_append_int32(&writer,
            HYBRIS_GNSS_LOCATION_HAS_LAT_LONG |
            HYBRIS_GNSS_LOCATION_HAS_HORIZONTAL_ACCURACY);
        gbinder_writer_append_double(&writer, latitudeDegrees);
        gbinder_writer_append_double(&writer, longitudeDegrees);
        gbinder_writer_append_double(&writer, 0);
        gbinder_writer_append_float(&writer, 0);
        gbinder_writer_append_float(&writer, 0);
        gbinder_writer_append_float(&writer, accuracyMeters);
        gbinder_writer_append_float(&writer, 0);
        gbinder_writer_append_float(&writer, 0);
        gbinder_writer_append_float(&writer, 0);
        gbinder_writer_append_int64(&writer, timestamp);

        reply = gbinder_client_transact_sync_reply(m_clientGnss,
            GNSS_AIDL_INJECT_LOCATION, req, &status);

        if (!status && isReplySuccess(reply)) {
            ret = true;
        }
        if (!ret) {
            qWarning("Failed to inject location");
        }

        gbinder_local_request_unref(req);
        gbinder_remote_reply_unref(reply);
    }
    return ret;
}

bool BinderLocationBackendAidl::gnssInjectTime(
    HybrisGnssUtcTime timeMs,
    int64_t timeReferenceMs,
    int32_t uncertaintyMs)
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
            GNSS_AIDL_INJECT_TIME, req, &status);

        if (!status && isReplySuccess(reply)) {
            ret = true;
        }

        if (!ret) {
            qWarning("Failed to inject time");
        }
        gbinder_local_request_unref(req);
        gbinder_remote_reply_unref(reply);
    }
    return ret;
}

void BinderLocationBackendAidl::gnssDeleteAidingData(
    HybrisGnssAidingData aidingDataFlags)
{
    if (m_clientGnss) {
        GBinderLocalRequest *req;

        req = gbinder_client_new_request(m_clientGnss);
        gbinder_local_request_append_int32(req, aidingDataFlags);
        gbinder_client_transact(m_clientGnss, GNSS_AIDL_DELETE_AIDING_DATA,
                                0, req, NULL, NULL, NULL);

        gbinder_local_request_unref(req);
    }
}

bool BinderLocationBackendAidl::gnssSetPositionMode(
    HybrisGnssPositionMode mode,
    HybrisGnssPositionRecurrence recurrence,
    uint32_t minIntervalMs,
    uint32_t preferredAccuracyMeters,
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

        /* Non-null parcelable */
        gbinder_writer_append_int32(&writer, 1);
        /* Parcelable size */
        gbinder_writer_append_int32(&writer, 6 * sizeof(gint32));
        gbinder_writer_append_int32(&writer, mode);
        gbinder_writer_append_int32(&writer, recurrence);
        gbinder_writer_append_int32(&writer, minIntervalMs);
        gbinder_writer_append_int32(&writer, preferredAccuracyMeters);
        gbinder_writer_append_int32(&writer, preferredTimeMs);
        gbinder_writer_append_int32(&writer, false);
        reply = gbinder_client_transact_sync_reply(m_clientGnss,
            GNSS_AIDL_SET_POSITION_MODE, req, &status);

        if (!status && isReplySuccess(reply)) {
            ret = true;
        }

        if (!ret) {
            qWarning("GNSS set position mode failed");
        }
        gbinder_local_request_unref(req);
        gbinder_remote_reply_unref(reply);
    }
    return ret;
}

// GnssDebug
void BinderLocationBackendAidl::gnssDebugInit()
{
    GBinderRemoteReply *reply;
    int status = 0;

    reply = gbinder_client_transact_sync_reply(m_clientGnss,
        GNSS_AIDL_GET_EXTENSION_GNSS_DEBUG, Q_NULLPTR, &status);

    if (!status) {
        m_remoteGnssDebug = getExtensionObject(reply);
        if (m_remoteGnssDebug) {
            qWarning("Initialising GNSS Debug interface");
            m_clientGnssDebug = gbinder_client_new(m_remoteGnssDebug, GNSS_AIDL_DEBUG_REMOTE);
        }
    }
    gbinder_remote_reply_unref(reply);
}

// GnnNi
void BinderLocationBackendAidl::gnssNiInit()
{
    GBinderRemoteReply *reply;
    int status = 0;

    reply = gbinder_client_transact_sync_reply(m_clientGnss,
        GNSS_AIDL_GET_EXTENSION_GNSS_VISIBILITY_CONTROL, Q_NULLPTR, &status);

    if (!status) {
        m_remoteGnssNi = getExtensionObject(reply);

        if (m_remoteGnssNi) {
            qWarning("Initialising GNSS NI interface");
            GBinderLocalRequest *req;
            m_clientGnssNi = gbinder_client_new(m_remoteGnssNi, GNSS_AIDL_VISIBILITY_CONTROL_REMOTE);
            m_callbackGnssNi = gbinder_servicemanager_new_local_object
                (m_sm, GNSS_AIDL_VISIBILITY_CONTROL_CALLBACK, geoclue_binder_gnss_ni_callback, this);
            gbinder_local_object_set_stability(m_callbackGnssNi,
                GBINDER_STABILITY_VINTF);

            gbinder_remote_reply_unref(reply);

            /* IGnssNi::setCallback */
            req = gbinder_client_new_request(m_clientGnssNi);
            gbinder_local_request_append_local_object(req, m_callbackGnssNi);
            reply = gbinder_client_transact_sync_reply(m_clientGnssNi,
                GNSS_AIDL_VISIBILITY_CONTROL_SET_CALLBACK, req, &status);

            if (status) {
                qWarning("Initialising GNSS NI interface failed %d", status);
            }
            gbinder_local_request_unref(req);
        }
    }
    gbinder_remote_reply_unref(reply);
}

void BinderLocationBackendAidl::gnssNiRespond(
    int32_t notifId,
    HybrisGnssUserResponseType userResponse)
{
    Q_UNUSED(notifId)
    Q_UNUSED(userResponse)
}

// GnssXtra
void BinderLocationBackendAidl::gnssXtraInit()
{
    GBinderRemoteReply *reply;
    int status = 0;

    reply = gbinder_client_transact_sync_reply(m_clientGnss,
        GNSS_AIDL_GET_EXTENSION_PDSD, Q_NULLPTR, &status);

    if (!status) {
        m_remoteGnssXtra = getExtensionObject(reply, true);

        if (m_remoteGnssXtra) {
            qWarning("Initialising GNSS Xtra interface");
            GBinderLocalRequest *req;
            m_clientGnssXtra = gbinder_client_new(m_remoteGnssXtra, GNSS_AIDL_PSDS_REMOTE);
            m_callbackGnssXtra = gbinder_servicemanager_new_local_object
                (m_sm, GNSS_AIDL_PSDS_CALLBACK,
                 geoclue_binder_gnss_xtra_callback, this);
            gbinder_local_object_set_stability(m_callbackGnssXtra,
                GBINDER_STABILITY_VINTF);

            gbinder_remote_reply_unref(reply);

            /* IGnssXtra::setCallback */
            req = gbinder_client_new_request(m_clientGnssXtra);
            gbinder_local_request_append_local_object(req, m_callbackGnssXtra);
            reply = gbinder_client_transact_sync_reply(m_clientGnssXtra,
                GNSS_AIDL_PSDS_SET_CALLBACK, req, &status);

            if (status) {
                qWarning("Initialising GNSS Xtra interface failed");
            }
            gbinder_local_request_unref(req);
        }
    }
    gbinder_remote_reply_unref(reply);
}

bool BinderLocationBackendAidl::gnssXtraInjectXtraData(QByteArray &xtraData)
{
    bool ret = false;
    if (m_clientGnssXtra) {
        int status = 0;

        GBinderLocalRequest *req;
        GBinderRemoteReply *reply;

        req = gbinder_client_new_request(m_clientGnssXtra);

        GBinderWriter writer;
        gbinder_local_request_init_writer(req, &writer);
        gbinder_writer_append_int32(&writer, 0);
        gbinder_writer_append_byte_array(&writer, xtraData.data(), xtraData.length());
        reply = gbinder_client_transact_sync_reply(m_clientGnssXtra,
            GNSS_AIDL_PSDS_INJECT_PSDS_DATA, req, &status);

        if (!status && isReplySuccess(reply)) {
            ret = true;
        }

        if (!ret) {
            qWarning("GNSS Xtra inject xtra data failed");
        }
        gbinder_local_request_unref(req);
        gbinder_remote_reply_unref(reply);
    }
    return ret;
}

// AGnss
void BinderLocationBackendAidl::aGnssInit()
{
    GBinderRemoteReply *reply;
    int status = 0;

    reply = gbinder_client_transact_sync_reply(m_clientGnss,
        GNSS_AIDL_GET_EXTENSION_AGNSS, Q_NULLPTR, &status);

    if (!status) {
        m_remoteAGnss = getExtensionObject(reply);

        if (m_remoteAGnss) {
            qWarning("Initialising AGNSS interface");
            GBinderLocalRequest *req;
            m_clientAGnss = gbinder_client_new(m_remoteAGnss, AGNSS_AIDL_REMOTE);
            m_callbackAGnss = gbinder_servicemanager_new_local_object
                (m_sm, AGNSS_AIDL_CALLBACK,
                geoclue_binder_agnss_callback, this);
            gbinder_local_object_set_stability(m_callbackAGnss,
                GBINDER_STABILITY_VINTF);

            gbinder_remote_reply_unref(reply);

            /* IAGnss::setCallback */
            req = gbinder_client_new_request(m_clientAGnss);
            gbinder_local_request_append_local_object(req, m_callbackAGnss);
            reply = gbinder_client_transact_sync_reply(m_clientAGnss,
                AGNSS_AIDL_SET_CALLBACK, req, &status);

            if (status) {
                qWarning("Initialising AGNSS interface failed %d", status);
            }
            gbinder_local_request_unref(req);
        }
    }
    gbinder_remote_reply_unref(reply);
}

bool BinderLocationBackendAidl::aGnssDataConnClosed()
{
    int status = 0;
    bool ret = false;
    GBinderRemoteReply *reply;

    reply = gbinder_client_transact_sync_reply(m_clientAGnss,
        AGNSS_AIDL_DATA_CONN_CLOSED, Q_NULLPTR, &status);

    if (!status && isReplySuccess(reply)) {
        ret = true;
    }

    gbinder_remote_reply_unref(reply);
    return ret;
}

bool BinderLocationBackendAidl::aGnssDataConnFailed()
{
    int status = 0;
    bool ret = false;
    GBinderRemoteReply *reply;

    reply = gbinder_client_transact_sync_reply(m_clientAGnss,
         AGNSS_AIDL_DATA_CONN_FAILED, Q_NULLPTR, &status);

    if (!status && isReplySuccess(reply)) {
        ret = true;
    }

    gbinder_remote_reply_unref(reply);
    return ret;
}

bool BinderLocationBackendAidl::aGnssDataConnOpen(
    const QByteArray &apn,
    const QString &protocol)
{
    int status = 0;
    bool ret = false;
    GBinderLocalRequest *req;
    GBinderRemoteReply *reply;
    GBinderWriter writer;

    req = gbinder_client_new_request(m_clientAGnss);

    gbinder_local_request_init_writer(req, &writer);
    gbinder_writer_append_int64(&writer, 0);
    gbinder_writer_append_string16(&writer, apn.constData());
    gbinder_writer_append_int32(&writer, fromContextProtocol(protocol));
    reply = gbinder_client_transact_sync_reply(m_clientAGnss,
         AGNSS_AIDL_DATA_CONN_OPEN, req, &status);

    if (!status && isReplySuccess(reply)) {
        ret = true;
    }

    gbinder_local_request_unref(req);
    gbinder_remote_reply_unref(reply);

    return ret;
}

int BinderLocationBackendAidl::aGnssSetServer(
    HybrisAGnssType type,
    const char* hostname,
    int port)
{
    int status = 0;
    bool ret = false;
    GBinderLocalRequest *req;
    GBinderRemoteReply *reply;
    GBinderWriter writer;

    req = gbinder_client_new_request(m_clientAGnss);

    gbinder_local_request_init_writer(req, &writer);
    gbinder_writer_append_int32(&writer, type);
    gbinder_writer_append_string16(&writer, hostname);
    gbinder_writer_append_int32(&writer, port);
    reply = gbinder_client_transact_sync_reply(m_clientAGnss,
        AGNSS_AIDL_SET_SERVER, req, &status);

    if (!status && isReplySuccess(reply)) {
        ret = true;
    }

    gbinder_local_request_unref(req);
    gbinder_remote_reply_unref(reply);

    return ret;
}

// AGnssRil
void BinderLocationBackendAidl::aGnssRilInit()
{
    GBinderRemoteReply *reply;
    int status = 0;

    reply = gbinder_client_transact_sync_reply(m_clientGnss,
        GNSS_AIDL_GET_EXTENSION_AGNSS_RIL, Q_NULLPTR, &status);

    if (!status) {
        m_remoteAGnssRil = getExtensionObject(reply);

        if (m_remoteAGnssRil) {
            qWarning("Initialising AGNSS RIL interface");
            GBinderLocalRequest *req;
            m_clientAGnssRil = gbinder_client_new(m_remoteAGnssRil, AGNSS_AIDL_RIL_REMOTE);
            m_callbackAGnssRil = gbinder_servicemanager_new_local_object
                (m_sm, AGNSS_AIDL_RIL_CALLBACK, geoclue_binder_agnss_ril_callback, this);
            gbinder_local_object_set_stability(m_callbackAGnssRil,
                GBINDER_STABILITY_VINTF);

            gbinder_remote_reply_unref(reply);

            /* IAGnssRil::setCallback */
            req = gbinder_client_new_request(m_clientAGnssRil);
            gbinder_local_request_append_local_object(req, m_callbackAGnssRil);
            reply = gbinder_client_transact_sync_reply(m_clientAGnssRil,
                AGNSS_AIDL_RIL_SET_CALLBACK, req, &status);

            if (status) {
                qWarning("Initialising AGNSS RIL interface failed %d", status);
            }
            gbinder_local_request_unref(req);
        }
    }
    gbinder_remote_reply_unref(reply);
}

bool BinderLocationBackendAidl::isReplySuccess(GBinderRemoteReply *reply)
{
    GBinderReader reader;
    gint32 status;

    if (!reply) {
        return false;
    }

    gbinder_remote_reply_init_reader(reply, &reader);

    if (!gbinder_reader_read_int32(&reader, &status) || status != 0) {
        return false;
    }

    return true;
}
