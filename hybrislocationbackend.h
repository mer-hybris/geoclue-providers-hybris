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

#ifndef HYBRIS_LOCATION_BACKEND_H
#define HYBRIS_LOCATION_BACKEND_H

#include <cstdint>

#include <QtCore/QObject>
#include <QtCore/QString>

/** Milliseconds since January 1, 1970 */
typedef int64_t HybrisGnssUtcTime;

/** Requested operational mode for GPS operation. */
typedef uint32_t HybrisGnssPositionMode;

/** Requested recurrence mode for GPS operation. */
typedef uint32_t HybrisGnssPositionRecurrence;

/** GPS status event values. */
typedef uint16_t HybrisGnssStatusValue;

/** Flags to indicate which values are valid in a GpsLocation. */
typedef uint16_t HybrisGnssLocationFlags;

/**
 * Flags used to specify which aiding data to delete when calling
 * delete_aiding_data().
 */
typedef uint16_t HybrisGnssAidingData;

/** AGPS type */
typedef uint16_t HybrisAGnssType;

typedef uint16_t HybrisAGnssSetIDType;

typedef uint16_t HybrisApnIpType;
/**
 * HybrisGnssNiType constants
 */
typedef uint32_t HybrisGnssNiType;

/**
 * HybrisGnssNiNotifyFlags constants
 */
typedef uint32_t HybrisGnssNiNotifyFlags;

/**
 * GPS NI responses, used to define the response in
 * NI structures
 */
typedef int HybrisGnssUserResponseType;

/**
 * NI data encoding scheme
 */
typedef int HybrisGnssNiEncodingType;

/** AGPS status event values. */
typedef uint16_t HybrisAGnssStatusValue;

typedef uint16_t HybrisAGnssRefLocationType;

typedef int HybrisNetworkType;

enum {
    HYBRIS_GNSS_POSITION_MODE_STANDALONE = 0,
    HYBRIS_GNSS_POSITION_MODE_MS_BASED = 1,
    HYBRIS_GNSS_POSITION_MODE_MS_ASSISTED = 2,
};

enum {
    HYBRIS_GNSS_POSITION_RECURRENCE_PERIODIC = 0,
    HYBRIS_GNSS_POSITION_RECURRENCE_SINGLE = 1,
};

enum {
    HYBRIS_AGNSS_TYPE_SUPL = 1,
    HYBRIS_AGNSS_TYPE_C2K = 2,
};

enum {
    HYBRIS_GNSS_REQUEST_AGNSS_DATA_CONN = 1,
    HYBRIS_GNSS_RELEASE_AGNSS_DATA_CONN = 2,
    HYBRIS_GNSS_AGNSS_DATA_CONNECTED = 3,
    HYBRIS_GNSS_AGNSS_DATA_CONN_DONE = 4,
    HYBRIS_GNSS_AGNSS_DATA_CONN_FAILED = 5,
};

class HybrisLocationBackend : public QObject
{
    Q_OBJECT
public:
    explicit HybrisLocationBackend(QObject *parent = 0) : QObject(parent) {};
    virtual ~HybrisLocationBackend() {}

    // Gnss
    virtual bool gnssInit() = 0;
    virtual bool gnssStart() = 0;
    virtual bool gnssStop() = 0;
    virtual void gnssCleanup() = 0;
    virtual bool gnssInjectTime(HybrisGnssUtcTime timeMs, int64_t timeReferenceMs, int32_t uncertaintyMs) = 0;
    virtual bool gnssInjectLocation(double latitudeDegrees, double longitudeDegrees, float accuracyMeters) = 0;
    virtual void gnssDeleteAidingData(HybrisGnssAidingData aidingDataFlags) = 0;
    virtual bool gnssSetPositionMode(HybrisGnssPositionMode mode, HybrisGnssPositionRecurrence recurrence,
                                     uint32_t minIntervalMs, uint32_t preferredAccuracyMeters,
                                     uint32_t preferredTimeMs) = 0;

    // GnssDebug
    virtual void gnssDebugInit() = 0;

    // GnnNi
    virtual void gnssNiInit() = 0;
    virtual void gnssNiRespond(int32_t notifId, HybrisGnssUserResponseType userResponse) = 0;

    // GnssXtra
    virtual void gnssXtraInit() = 0;
    virtual bool gnssXtraInjectXtraData(QByteArray &xtraData) = 0;

    // AGnss
    virtual void aGnssInit() = 0;
    virtual bool aGnssDataConnClosed() = 0;
    virtual bool aGnssDataConnFailed() = 0;
    virtual bool aGnssDataConnOpen(const QByteArray &apn, const QString &protocol) = 0;

    // AGnssRil
    virtual void aGnssRilInit() = 0;

};

HybrisLocationBackend *getLocationBackend();

#endif // HYBRIS_LOCATION_BACKEND_H
