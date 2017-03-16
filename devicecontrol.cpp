/*
    Copyright (C) 2015 Jolla Ltd.
    Contact: Aaron McCarthy <aaron.mccarthy@jollamobile.com>

    This file is part of geoclue-hybris.

    Geoclue-hybris is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License.
*/

#include "devicecontrol.h"

#include "device_adaptor.h"

#include <QtDBus/QDBusVariant>

DeviceControl::DeviceControl(LocationSettings *settings, QObject *parent)
:   QObject(parent), m_settings(settings)
{
    connect(m_settings, &LocationSettings::gpsFlightModeChanged,
            [this] {
                emit this->poweredChanged();
                emit PropertyChanged(QStringLiteral("Powered"), QDBusVariant(!this->m_settings->gpsFlightMode()));
            });

    new DeviceAdaptor(this);
}

bool DeviceControl::powered() const
{
    return !m_settings->gpsFlightMode();
}

void DeviceControl::setPowered(bool newPowered)
{
    // powered is the inverse of flight mode, so if they're the same it needs updating.
    if (m_settings->gpsFlightMode() == newPowered) {
        m_settings->setGpsFlightMode(!newPowered);
    }
}
