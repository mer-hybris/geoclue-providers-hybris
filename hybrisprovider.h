/*
    Copyright (C) 2013 Jolla Ltd.
    Contact: Aaron McCarthy <aaron.mccarthy@jollamobile.com>
*/

#ifndef HYBRISPROVIDER_H
#define HYBRISPROVIDER_H

#include <QtCore/QObject>
#include <QtCore/QStringList>
#include <QtDBus/QDBusContext>

#include <android/hardware/gps.h>

#include "locationtypes.h"

QT_FORWARD_DECLARE_CLASS(QDBusServiceWatcher)
class MGConfItem;

class HybrisProvider : public QObject, public QDBusContext
{
    Q_OBJECT

public:
    explicit HybrisProvider(QObject *parent = 0);
    ~HybrisProvider();

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
    void requestPhoneContext(UlpPhoneContextRequest *req);
    void setLocation(const Location &location);
    void setSatellite(const QList<SatelliteInfo> &satellites, const QList<int> &used);
    void serviceUnregistered(const QString &service);
    void locationEnabledChanged();

private:
    void emitLocationChanged();
    void emitSatelliteChanged();
    void startPositioningIfNeeded();
    void stopPositioningIfNeeded();
    void setStatus(Status status);

    gps_device_t *m_gpsDevice;

    const GpsInterface *m_gps;
    const UlpNetworkInterface *m_ulpNetwork;
    const UlpPhoneContextInterface *m_ulpPhoneContext;

    const AGpsInterface *m_agps;
    const AGpsRilInterface *m_agpsril;
    const GpsNiInterface *m_gpsni;
    const GpsXtraInterface *m_xtra;

    const GpsDebugInterface *m_debug;

    UlpPhoneContextSettings m_settings;

    Location m_currentLocation;

    qint64 m_satelliteTimestamp;
    QList<SatelliteInfo> m_visibleSatellites;
    QList<int> m_usedPrns;

    qint64 m_previousSatelliteTimestamp;
    QList<SatelliteInfo> m_previousVisibleSatellites;
    QList<int> m_previousUsedPrns;

    QDBusServiceWatcher *m_watcher;
    QStringList m_watchedServices;

    int m_idleTimer;
    Status m_status;

    MGConfItem *m_locationEnabled;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(HybrisProvider::PositionFields)
Q_DECLARE_OPERATORS_FOR_FLAGS(HybrisProvider::VelocityFields)

Q_DECLARE_METATYPE(UlpPhoneContextRequest *)

#endif // HYBRISPROVIDER_H
