/*
    Copyright (C) 2015 Jolla Ltd.
    Contact: Aaron McCarthy <aaron.mccarthy@jollamobile.com>

    This file is part of geoclue-hybris.

    Geoclue-hybris is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License.
*/

#include <QtCore/QCoreApplication>
#include <QtCore/QLoggingCategory>
#include <QtDBus/QDBusConnection>

#include "hybrisprovider.h"

#include <locationsettings.h>

int main(int argc, char *argv[])
{
    QCoreApplication::setSetuidAllowed(true);
    QLoggingCategory::setFilterRules(QStringLiteral("geoclue.provider.hybris.debug=false\n"
                                                    "geoclue.provider.hybris.nmea.debug=false\n"
                                                    "geoclue.provider.hybris.position.debug=false"));
    QCoreApplication a(argc, argv);

    QDBusConnection session = QDBusConnection::sessionBus();
    LocationSettings settings;
    HybrisProvider provider;
    provider.setLocationSettings(&settings);
    if (!session.registerObject(QStringLiteral("/org/freedesktop/Geoclue/Providers/Hybris"), &provider))
        qFatal("Failed to register object /org/freedesktop/Geoclue/Providers/Hybris");
    if (!session.registerService(QStringLiteral("org.freedesktop.Geoclue.Providers.Hybris")))
        qFatal("Failed to register service org.freedesktop.Geoclue.Providers.Hybris");

    return a.exec();
}
