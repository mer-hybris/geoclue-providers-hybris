/*
    Copyright (C) 2015 Jolla Ltd.
    Contact: Aaron McCarthy <aaron.mccarthy@jollamobile.com>

    This file is part of geoclue-hybris.

    Geoclue-hybris is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License.
*/

#ifndef DEVICECONTROL_H
#define DEVICECONTROL_H

#include <QtCore/QObject>
#include <QtCore/QString>

#include <locationsettings.h>

QT_FORWARD_DECLARE_CLASS(QDBusVariant)

/* DeviceControl provides a mechanism which connman can use
 * to disable the GPS when Flight Mode is activated */

class DeviceControl : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool Powered READ powered WRITE setPowered NOTIFY poweredChanged)

public:
    explicit DeviceControl(LocationSettings *settings, QObject *parent = 0);

    bool powered() const;
    void setPowered(bool newPowered);

signals:
    void PropertyChanged(const QString &name, const QDBusVariant &value);
    void poweredChanged();

private:
    LocationSettings *m_settings;
    bool m_powered;
};

#endif // DEVICECONTROL_H
