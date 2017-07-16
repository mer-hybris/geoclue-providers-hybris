/*
    Copyright (C) 2015 Jolla Ltd.
    Contact: Aaron McCarthy <aaron.mccarthy@jollamobile.com>

    This file is part of geoclue-hybris.

    Geoclue-hybris is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License.
*/

#ifndef HYBRISPROVIDER_H
#define HYBRISPROVIDER_H

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtCore/QBasicTimer>
#include <QtCore/QQueue>
#include <QtDBus/QDBusContext>
#include <QtNetwork/QNetworkReply>

#include <android-version.h>
#include <hardware/gps.h>

#include <locationsettings.h>

#include "locationtypes.h"

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

QT_FORWARD_DECLARE_CLASS(QFileSystemWatcher)
QT_FORWARD_DECLARE_CLASS(QDBusServiceWatcher)
QT_FORWARD_DECLARE_CLASS(QNetworkAccessManager)
QT_FORWARD_DECLARE_CLASS(QHostAddress)
QT_FORWARD_DECLARE_CLASS(QUdpSocket)
QT_FORWARD_DECLARE_CLASS(QHostInfo)

class ComJollaConnectiondInterface;
class ComJollaLipstickConnectionSelectorIfInterface;
class MGConfItem;
class DeviceControl;
class NetworkManager;
class NetworkTechnology;
class QOfonoExtModemManager;
class QOfonoConnectionManager;
class QOfonoConnectionContext;

class HybrisProvider : public QObject, public QDBusContext
{
    Q_OBJECT

public:
    explicit HybrisProvider(QObject *parent = 0);
    ~HybrisProvider();

    void setLocationSettings(LocationSettings *settings);

    // org.freedesktop.Geoclue
    void AddReference();
    void RemoveReference();

    // org.freedesktop.Geoclue
    QString GetProviderInfo(QString &description);

    // Must match GeoclueStatus enum
    enum Status {
        StatusError,
        StatusUnavailable,
        StatusAcquiring,
        StatusAvailable
    };

    // org.freedesktop.Geoclue
    int GetStatus();

    // org.freedesktop.Geoclue
    void SetOptions(const QVariantMap &options);

    // Must match GeocluePositionFields enum
    enum PositionField {
        NoPositionFields = 0x00,
        LatitudePresent = 0x01,
        LongitudePresent = 0x02,
        AltitudePresent = 0x04
    };
    Q_DECLARE_FLAGS(PositionFields, PositionField)

    // org.freedesktop.Geoclue.Position
    int GetPosition(int &timestamp, double &latitude, double &longitude, double &altitude, Accuracy &accuracy);

    // Must match GeoclueVelocityFields enum
    enum VelocityField {
        NoVelocityFields = 0x00,
        SpeedPresent = 0x01,
        DirectionPresent = 0x02,
        ClimbPresent = 0x04
    };
    Q_DECLARE_FLAGS(VelocityFields, VelocityField)

    // org.freedesktop.Geoclue.Velocity
    int GetVelocity(int &timestamp, double &speed, double &direction, double &climb);

    // org.freedesktop.Geoclue.Satellite
    int GetLastSatellite(int &satelliteUsed, int &satelliteVisible, QList<int> &usedPrn, QList<SatelliteInfo> &satInfo);
    int GetSatellite(int &satelliteUsed, int &satelliteVisible, QList<int> &usedPrn, QList<SatelliteInfo> &satInfo);

signals:
    // org.freedesktop.Geoclue
    void StatusChanged(int status);

    // org.freedesktop.Geoclue.Position
    void PositionChanged(int fields, int timestamp, double latitude, double longitude, double altitude, const Accuracy &accuracy);

    // org.freedesktop.Geoclue.Velocity
    void VelocityChanged(int fields, int timestamp, double speed, double direction, double climb);

    // org.freedesktop.Geoclue.Satellite
    void SatelliteChanged(int timestamp, int satelliteUsed, int satelliteVisible, const QList<int> &usedPrn, const QList<SatelliteInfo> &satInfos);

protected:
    void timerEvent(QTimerEvent *event);

private slots:
    void setLocation(const Location &location);
    void setSatellite(const QList<SatelliteInfo> &satellites, const QList<int> &used);
    void serviceUnregistered(const QString &service);
    void locationEnabledChanged();
    void injectPosition(int fields, int timestamp, double latitude, double longitude,
                        double altitude, const Accuracy &accuracy);
    void injectUtcTime();
    void sendNtpRequest(const QHostInfo &host);
    void handleNtpResponse();
    void xtraDownloadRequest();
    void xtraDownloadRequestSendNext();
    void xtraDownloadFinished();
    void agpsStatus(qint16 type, quint16 status, const QHostAddress &ipv4,
                    const QHostAddress &ipv6, const QByteArray &ssid, const QByteArray &password);
    void dataServiceConnected();
    void connectionErrorReported(const QString &path, const QString &error);
    void connectionSelected(bool selected);

    void setMagneticVariation(double variation);

    void engineOn();
    void engineOff();

    void technologiesChanged();
    void defaultDataModemChanged(const QString &modem);
    void connectionManagerValidChanged();
    void connectionContextValidChanged();
    void cellularConnected(bool connected);

private:
    void emitLocationChanged();
    void emitSatelliteChanged();
    void startPositioningIfNeeded();
    void stopPositioningIfNeeded();
    void setStatus(Status status);
    bool positioningEnabled();
    quint32 minimumRequestedUpdateInterval() const;

    void startDataConnection();
    void stopDataConnection();

    void sendNtpRequest();

    void processConnectionContexts();
    void processNextConnectionContext();

    gps_device_t *m_gpsDevice;

    const GpsInterface *m_gps;

    const AGpsInterface *m_agps;
    const AGpsRilInterface *m_agpsril;
    const GpsNiInterface *m_gpsni;
    const GpsXtraInterface *m_xtra;

    const GpsDebugInterface *m_debug;

    Location m_currentLocation;

    qint64 m_satelliteTimestamp;
    QList<SatelliteInfo> m_visibleSatellites;
    QList<int> m_usedPrns;

    qint64 m_previousSatelliteTimestamp;
    QList<SatelliteInfo> m_previousVisibleSatellites;
    QList<int> m_previousUsedPrns;

    QDBusServiceWatcher *m_watcher;
    struct ServiceData {
        ServiceData()
        :   referenceCount(0), updateInterval(0)
        {
        }

        int referenceCount;
        quint32 updateInterval;
    };
    QMap<QString, ServiceData> m_watchedServices;

    QBasicTimer m_idleTimer;
    QBasicTimer m_fixLostTimer;

    Status m_status;

    bool m_positionInjectionConnected;

    QNetworkAccessManager *m_manager;
    QNetworkReply *m_xtraDownloadReply;
    QQueue<QUrl> m_xtraServers;

    ComJollaConnectiondInterface *m_connectiond;
    ComJollaLipstickConnectionSelectorIfInterface *m_connectionSelector;

    QString m_networkServicePath;
    QString m_agpsInterface;
    QStringList m_connectionContexts;
    bool m_requestedConnect;

    bool m_gpsStarted;

    LocationSettings *m_locationSettings;

    NetworkManager *m_networkManager;
    NetworkTechnology *m_cellularTechnology;

    QOfonoExtModemManager *m_ofonoExtModemManager;
    QOfonoConnectionManager *m_connectionManager;
    QOfonoConnectionContext *m_connectionContext;

    QUdpSocket *m_ntpSocket;
    QBasicTimer m_ntpRetryTimer;
    QStringList m_ntpServers;
    qint64 m_ntpRequestTime;
    qint64 m_ntpRequestTicks;

    bool m_agpsEnabled;
    bool m_agpsOnlineEnabled;
    double m_magneticVariation;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(HybrisProvider::PositionFields)
Q_DECLARE_OPERATORS_FOR_FLAGS(HybrisProvider::VelocityFields)

#endif // HYBRISPROVIDER_H
