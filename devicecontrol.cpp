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

DeviceControl::DeviceControl(QObject *parent)
:   QObject(parent), m_powered(false)
{
    new DeviceAdaptor(this);
}

bool DeviceControl::powered() const
{
    return m_powered;
}

void DeviceControl::setPowered(bool powered)
{
    if (m_powered == powered)
        return;

    m_powered = powered;
    emit poweredChanged();
    emit PropertyChanged(QStringLiteral("Powered"), QDBusVariant(powered));
}
