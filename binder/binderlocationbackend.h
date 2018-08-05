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

#ifndef BINDERLOCATIONBACKEND_H
#define BINDERLOCATIONBACKEND_H

#include "hybrislocationbackend.h"

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QBasicTimer>
#include <QtCore/QQueue>
#include <QtDBus/QDBusContext>
#include <QtNetwork/QNetworkReply>

#include <gbinder.h>
#include <locationsettings.h>

#include "gnss-binder-types.h"
#include "locationtypes.h"

class BinderLocationBackend : public HybrisLocationBackend
{
    Q_OBJECT
public:
    BinderLocationBackend(QObject *parent = 0);
    ~BinderLocationBackend();

    void dropGnss();

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

    // AGnssRil
    void aGnssRilInit();

private:
    bool isReplySuccess(GBinderRemoteReply *reply);
    GBinderRemoteObject *getExtensionObject(GBinderRemoteReply *reply);

    gulong m_death_id;
    char* m_fqname;
    GBinderServiceManager* m_sm;

    GBinderClient* m_clientGnss;
    GBinderRemoteObject* m_remoteGnss;
    GBinderLocalObject* m_callbackGnss;

    GBinderClient* m_clientGnssDebug;
    GBinderRemoteObject* m_remoteGnssDebug;

    GBinderClient* m_clientGnssNi;
    GBinderRemoteObject* m_remoteGnssNi;
    GBinderLocalObject* m_callbackGnssNi;

    GBinderClient* m_clientGnssXtra;
    GBinderRemoteObject* m_remoteGnssXtra;
    GBinderLocalObject* m_callbackGnssXtra;

    GBinderClient* m_clientAGnss;
    GBinderRemoteObject* m_remoteAGnss;
    GBinderLocalObject* m_callbackAGnss;

    GBinderClient* m_clientAGnssRil;
    GBinderRemoteObject* m_remoteAGnssRil;
    GBinderLocalObject* m_callbackAGnssRil;
};

#endif // BINDERLOCATIONBACKEND_H
