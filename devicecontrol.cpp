/*
    Copyright (C) 2014 Jolla Ltd.
    Contact: Aaron McCarthy <aaron.mccarthy@jollamobile.com>
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
