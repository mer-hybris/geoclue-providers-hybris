/*
    Copyright (c) 2015 Jolla Ltd.
    Copyright (c) 2018 Matti Lehtimäki <matti.lehtimaki@gmail.com>
    Copyright (c) 2025 Jolla Mobile Ltd

    This file is part of geoclue-hybris.

    Geoclue-hybris is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License.
*/

#ifndef GNSS_BINDER_TYPES_H
#define GNSS_BINDER_TYPES_H

#include <gutil_types.h>

#define ALIGNED(x) __attribute__ ((aligned(x)))

typedef enum gnss_interface {
    GNSS_INTERFACE_1_0,
    GNSS_INTERFACE_1_1,
    GNSS_INTERFACE_2_0,
    GNSS_INTERFACE_2_1,
    GNSS_INTERFACE_COUNT
} GnssInterface;

typedef struct geoclue_binder_gnss GeoclueBinderGnss;

typedef enum gnss_max {
    SVS_COUNT = 64u,
} GnssMax;

typedef int64_t GnssUtcTime;

enum class GnssConstellationType : guint8 {
    UNKNOWN = 0,
    GPS = 1,
    SBAS = 2,
    GLONASS = 3,
    QZSS = 4,
    BEIDOU = 5,
    GALILEO = 6,
    IRNSS = 7,
};

enum class GnssLocationFlags : guint16 {
    HAS_LAT_LONG = 1, // 0x0001
    HAS_ALTITUDE = 2, // 0x0002
    HAS_SPEED = 4, // 0x0004
    HAS_BEARING = 8, // 0x0008
    HAS_HORIZONTAL_ACCURACY = 16, // 0x0010
    HAS_VERTICAL_ACCURACY = 32, // 0x0020
    HAS_SPEED_ACCURACY = 64, // 0x0040
    HAS_BEARING_ACCURACY = 128, // 0x0080
};

enum {
    HYBRIS_GNSS_SV_FLAGS_NONE = 0,
    HYBRIS_GNSS_SV_FLAGS_HAS_EPHEMERIS_DATA = 1, //(1 << 0)
    HYBRIS_GNSS_SV_FLAGS_HAS_ALMANAC_DATA = 2, //(1 << 1)
    HYBRIS_GNSS_SV_FLAGS_USED_IN_FIX = 4, //(1 << 2)
    HYBRIS_GNSS_SV_FLAGS_HAS_CARRIER_FREQUENCY = 8, //(1 << 3)
};

typedef struct gnss_location {
    guint16 gnssLocationFlags ALIGNED(2);
    gdouble latitudeDegrees ALIGNED(8);
    gdouble longitudeDegrees ALIGNED(8);
    gdouble altitudeMeters ALIGNED(8);
    gfloat speedMetersPerSec ALIGNED(4);
    gfloat bearingDegrees ALIGNED(4);
    gfloat horizontalAccuracyMeters ALIGNED(4);
    gfloat verticalAccuracyMeters ALIGNED(4);
    gfloat speedAccuracyMetersPerSecond ALIGNED(4);
    gfloat bearingAccuracyDegrees ALIGNED(4);
    gint64 timestamp ALIGNED(8);
} ALIGNED(8) GnssLocation;

G_STATIC_ASSERT(sizeof(GnssLocation) == 64);

typedef struct gnss_elapsed_realtime {
    uint16_t flags ALIGNED(2);
    uint64_t timestampNs ALIGNED(8);
    uint64_t timeUncertaintyNs ALIGNED(8);
} ALIGNED(8) GnssElapsedRealtime;

typedef struct gnss_location_2_0 {
    GnssLocation v1_0 ALIGNED(8);
    GnssElapsedRealtime elapsedRealtime ALIGNED(8);
} ALIGNED(8) GnssLocation_2_0;

G_STATIC_ASSERT(sizeof(GnssLocation_2_0) == 88);

typedef struct gnss_sv_info {
    gint16 svid ALIGNED(2);
    GnssConstellationType constellation ALIGNED(1);
    gfloat cN0Dbhz ALIGNED(4);
    gfloat elevationDegrees ALIGNED(4);
    gfloat azimuthDegrees ALIGNED(4);
    gfloat carrierFrequencyHz ALIGNED(4);
    guint8 svFlag ALIGNED(1);
} ALIGNED(4) GnssSvInfo;

G_STATIC_ASSERT(sizeof(GnssSvInfo) == 24);

typedef struct gnss_sv_status {
    gint32 numSvs ALIGNED(4);
    GnssSvInfo gnssSvList[64] ALIGNED(4);
} ALIGNED(4) GnssSvStatus;

G_STATIC_ASSERT(sizeof(GnssSvStatus) == 1540);

typedef struct gnss_sv_info_2_0 {
    GnssSvInfo v1_0 ALIGNED(4);
    GnssConstellationType constellation ALIGNED(1);
} ALIGNED(4) GnssSvInfo_2_0;

G_STATIC_ASSERT(sizeof(GnssSvInfo_2_0) == 28);

typedef struct gnss_sv_status_2_0 {
    GBinderHidlVec gnssSvList ALIGNED(4); /* GnssSvInfo_2_0 */
} ALIGNED(4) GnssSvStatus_2_0;

G_STATIC_ASSERT(sizeof(GnssSvStatus_2_0) == 16);

typedef struct gnss_sv_info_2_1 {
    GnssSvInfo_2_0 v2_0 ALIGNED(4);
    gdouble basebandCN0DbHz ALIGNED(8);
} ALIGNED(4) GnssSvInfo_2_1;

G_STATIC_ASSERT(sizeof(GnssSvInfo_2_1) == 40);

typedef struct gnss_sv_status_2_1 {
    GBinderHidlVec gnssSvList ALIGNED(4); /* GnssSvInfo_2_1 */
} ALIGNED(4) GnssSvStatus_2_1;

G_STATIC_ASSERT(sizeof(GnssSvStatus_2_1) == 16);

typedef uint8_t AGnssType;
typedef uint8_t AGnssStatusValue;

typedef struct gnss_aidl_location {
    gint32 gnssLocationFlags;
    gdouble latitudeDegrees;
    gdouble longitudeDegrees;
    gdouble altitudeMeters;
    gdouble speedMetersPerSec;
    gdouble bearingDegrees;
    gdouble horizontalAccuracyMeters;
    gdouble verticalAccuracyMeters;
    gdouble speedAccuracyMetersPerSecond;
    gdouble bearingAccuracyDegrees;
    gint64 timestamp;
} GnssAidlLocation;

typedef struct gnss_aidl_sv_info {
    gint32 svid;
    GnssConstellationType constellation;
    gfloat cN0Dbhz;
    gfloat basebandCN0DbHz;
    gfloat elevationDegrees;
    gfloat azimuthDegrees;
    gint64 carrierFrequencyHz;
    gint32 svFlag;
} GnssAidlSvInfo;

enum {
    HYBRIS_GNSS_STATUS_NONE = 0,
    HYBRIS_GNSS_STATUS_SESSION_BEGIN = 1,
    HYBRIS_GNSS_STATUS_SESSION_END = 2,
    HYBRIS_GNSS_STATUS_ENGINE_ON = 3,
    HYBRIS_GNSS_STATUS_ENGINE_OFF = 4,
};

enum {
    HYBRIS_GNSS_LOCATION_HAS_LAT_LONG = 1, // 0x0001
    HYBRIS_GNSS_LOCATION_HAS_ALTITUDE = 2, // 0x0002
    HYBRIS_GNSS_LOCATION_HAS_SPEED = 4, // 0x0004
    HYBRIS_GNSS_LOCATION_HAS_BEARING = 8, // 0x0008
    HYBRIS_GNSS_LOCATION_HAS_HORIZONTAL_ACCURACY = 16, // 0x0010
    HYBRIS_GNSS_LOCATION_HAS_VERTICAL_ACCURACY = 32, // 0x0020
    HYBRIS_GNSS_LOCATION_HAS_SPEED_ACCURACY = 64, // 0x0040
    HYBRIS_GNSS_LOCATION_HAS_BEARING_ACCURACY = 128, // 0x0080
};

typedef struct agnss_status_ip_v4 {
    AGnssType type ALIGNED(1);
    AGnssStatusValue status ALIGNED(1);
    gint32 ipV4Addr ALIGNED(4);
} ALIGNED(4) AGnssStatusIpV4;

G_STATIC_ASSERT(sizeof(AGnssStatusIpV4) == 8);

typedef struct agnss_status_ip_v6 {
    AGnssType type ALIGNED(1);
    AGnssStatusValue status ALIGNED(1);
    uint8_t ipV6Addr[16] ALIGNED(1);
} ALIGNED(1) AGnssStatusIpV6;

G_STATIC_ASSERT(sizeof(AGnssStatusIpV6) == 18);

typedef struct agnss_ref_location_cell_id {
    guint8 type ALIGNED(1);
    guint16 mcc ALIGNED(2);
    guint16 mnc ALIGNED(2);
    guint16 lac ALIGNED(2);
    guint32 cid ALIGNED(4);
    guint16 tac ALIGNED(2);
    guint16 pcid ALIGNED(2);
} ALIGNED(4) AGnssRefLocationCellID;

G_STATIC_ASSERT(sizeof(AGnssRefLocationCellID) == 16);

typedef struct agnss_ref_location {
    guint8 type ALIGNED(1);
    AGnssRefLocationCellID cellID ALIGNED(4);
} ALIGNED(4) AGnssRefLocation;

G_STATIC_ASSERT(sizeof(AGnssRefLocation) == 20);

#endif // GNSS_BINDER_TYPES_H
