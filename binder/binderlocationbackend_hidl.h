/*
    Copyright (c) 2015 - 2020 Jolla Ltd.
    Copyright (c) 2018 Matti Lehtimäki <matti.lehtimaki@gmail.com>
    Copyright (c) 2025 Jolla Mobile Ltd

    This file is part of geoclue-hybris.

    Geoclue-hybris is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License.
*/

#ifndef BINDERLOCATIONBACKEND_HIDL_H
#define BINDERLOCATIONBACKEND_HIDL_H

#include "binderlocationbackend.h"

class BinderLocationBackendHidl : public BinderLocationBackend
{
    Q_OBJECT
public:
    BinderLocationBackendHidl(QObject *parent = 0);

    static bool isSupported();

    // Gnss
    bool gnssInit();
    bool gnssStart();
    bool gnssStop();
    void gnssCleanup();
    bool gnssInjectTime(HybrisGnssUtcTime timeMs,
                        int64_t timeReferenceMs,
                        int32_t uncertaintyMs);
    bool gnssInjectLocation(int timestamp,
                            double latitudeDegrees,
                            double longitudeDegrees,
                            float accuracyMeters);
    void gnssDeleteAidingData(HybrisGnssAidingData aidingDataFlags);
    bool gnssSetPositionMode(HybrisGnssPositionMode mode,
                             HybrisGnssPositionRecurrence recurrence,
                             uint32_t minIntervalMs,
                             uint32_t preferredAccuracyMeters,
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
    bool aGnssRilSetSetId(int type, const QByteArray &setId);
    bool aGnssRilSetRefLocation(const HybrisAGnssCell &cell);

protected:
    bool isReplySuccess(GBinderRemoteReply *reply);

private:
    bool gnssInitInternal(const char *remote,
                          gint32 set_callback,
                          GnssInterface gnssInterface);
    GnssInterface m_gnssInterface;
};

#endif // BINDERLOCATIONBACKEND_HIDL_H
