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
#include "devicecontrol.h"

#include <unistd.h>
#include <sys/types.h>
#include <grp.h>
#include <errno.h>

int main(int argc, char *argv[])
{
    uid_t realUid;
    uid_t effectiveUid;
    uid_t savedUid;

#if QT_VERSION >= QT_VERSION_CHECK(5, 3, 0)
    QCoreApplication::setSetuidAllowed(true);
#endif

    int result = getresuid(&realUid, &effectiveUid, &savedUid);
    if (result == -1)
        qFatal("Failed to get process uids, %s", strerror(errno));

    gid_t supplementaryGroups[NGROUPS_MAX];
    int numberGroups = getgroups(NGROUPS_MAX, supplementaryGroups);
    if (numberGroups == -1)
        qFatal("Failed to get supplementary groups, %s", strerror(errno));

    if (numberGroups + 1 > NGROUPS_MAX)
        qFatal("Too many supplementary groups");

    group *group = getgrnam("gps");
    if (!group)
        qFatal("Failed to get id of gps group, %s", strerror(errno));

    supplementaryGroups[numberGroups++] = group->gr_gid;

#if GEOCLUE_ANDROID_GPS_INTERFACE == 2
    group = getgrnam("net_raw");
    if (group) {
        if (numberGroups + 1 > NGROUPS_MAX)
            qWarning("Too many supplementary groups, can't add net_raw");
        else
            supplementaryGroups[numberGroups++] = group->gr_gid;
    }
#endif

    numberGroups = setgroups(numberGroups, supplementaryGroups);
    if (numberGroups == -1)
        qFatal("Failed to set supplementary groups, %s", strerror(errno));

    // Register service on DBus system bus prior to dropping privileges.
    QDBusConnection system = QDBusConnection::systemBus();
    if (!system.registerService(QStringLiteral("com.jollamobile.gps")))
        qFatal("Failed to register service com.jollamobile.gps");

#if GEOCLUE_ANDROID_GPS_INTERFACE != 2
    // Drop privileges.
    result = setuid(realUid);
    if (result == -1)
        qFatal("Failed to set process uid to %d, %s", realUid, strerror(errno));
#endif

    QLoggingCategory::setFilterRules(QStringLiteral("geoclue.provider.hybris.debug=false\n"
                                                    "geoclue.provider.hybris.nmea.debug=false\n"
                                                    "geoclue.provider.hybris.position.debug=false"));

    QCoreApplication a(argc, argv);


    DeviceControl control;

    if (!system.registerObject(QStringLiteral("/com/jollamobile/gps/Device"), &control))
        qFatal("Failed to register object /com/jollamobile/gps/Device");


    QDBusConnection connection = QDBusConnection::sessionBus();

    if (!connection.registerService(QStringLiteral("org.freedesktop.Geoclue.Providers.Hybris")))
        qFatal("Failed to register service org.freedesktop.Geoclue.Providers.Hybris");

    HybrisProvider provider;

    if (!connection.registerObject(QStringLiteral("/org/freedesktop/Geoclue/Providers/Hybris"), &provider))
        qFatal("Failed to register object /org/freedesktop/Geoclue/Providers/Hybris");

    provider.setDeviceController(&control);

    return a.exec();
}
