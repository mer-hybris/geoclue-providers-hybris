/*
    Copyright (C) 2014 Jolla Ltd.
    Contact: Aaron McCarthy <aaron.mccarthy@jollamobile.com>
*/

#ifndef DEVICECONTROL_H
#define DEVICECONTROL_H

#include <QtCore/QObject>

QT_FORWARD_DECLARE_CLASS(QDBusVariant)

class DeviceControl : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool Powered READ powered WRITE setPowered NOTIFY poweredChanged)

public:
    explicit DeviceControl(QObject *parent = 0);

    bool powered() const;
    void setPowered(bool powered);

signals:
    void PropertyChanged(const QString &name, const QDBusVariant &value);
    void poweredChanged();

private:
    bool m_powered;
};

#endif // DEVICECONTROL_H
