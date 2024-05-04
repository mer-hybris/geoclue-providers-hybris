/*
    Copyright (C) 2015 Jolla Ltd.
    Contact: Aaron McCarthy <aaron.mccarthy@jollamobile.com>

    This file is part of geoclue-hybris.

    Geoclue-hybris is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License.
*/

#ifndef HALLOCATIONBACKEND_H
#define HALLOCATIONBACKEND_H

#include "hybrislocationbackend.h"

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QBasicTimer>
#include <QtCore/QQueue>
#include <QtDBus/QDBusContext>
#include <QtNetwork/QNetworkReply>

#include <android-version.h>
#include <hardware/gps.h>
#include <hardware/fused_location.h>

#include <locationsettings.h>

#include "locationtypes.h"

class HalLocationBackend : public HybrisLocationBackend
{
    Q_OBJECT
public:
    HalLocationBackend(QObject *parent = 0);
    ~HalLocationBackend();

    // Gnss
    bool gnssInit();
    bool gnssStart();
    bool gnssStop();
    void gnssCleanup();
    bool gnssInjectTime(HybrisGnssUtcTime timeMs, int64_t timeReferenceMs, int32_t uncertaintyMs);
    bool gnssInjectLocation(double latitudeDegrees, double longitudeDegrees, float accuracyMeters);
    void gnssDeleteAidingData(HybrisGnssAidingData aidingDataFlags);
    bool gnssSetPositionMode(HybrisGnssPositionMode mode, HybrisGnssPositionRecurrence recurrence,
                                        uint32_t minIntervalMs, uint32_t preferredAccuracyMeters,
                                        uint32_t preferredTimeMs);

    // GnssDebug
    void gnssDebugInit();

    // GnnNi
    void gnssNiInit();
    void gnssNiRespond(int32_t notifId, HybrisGnssUserResponseType userResponse);

    // GnssXtra
    void gnssXtraInit();
    bool gnssXtraInjectXtraData(QByteArray &xtraData);

    // AGnss
    void aGnssInit();
    bool aGnssDataConnClosed();
    bool aGnssDataConnFailed();
    bool aGnssDataConnOpen(const QByteArray &apn, const QString &protocol);
    int aGnssSetServer(HybrisAGnssType type, const char* hostname, int port);

    // AGnssRil
    void aGnssRilInit();

    // AFlp
    bool aFlpInit();

private:
    gps_device_t *m_gpsDevice;
    flp_device_t *m_flpDevice;

    const GpsInterface *m_gps;

    const AGpsInterface *m_agps;
    const AGpsRilInterface *m_agpsril;
    const GpsNiInterface *m_gpsni;
    const GpsXtraInterface *m_xtra;

    const GpsDebugInterface *m_debug;

    const FlpLocationInterface *m_flp;
};

#endif // HALLOCATIONBACKEND_H
