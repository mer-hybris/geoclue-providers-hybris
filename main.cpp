/*
    Copyright (C) 2013 Jolla Ltd.
    Contact: Aaron McCarthy <aaron.mccarthy@jollamobile.com>
*/

#include <QtCore/QCoreApplication>
#include <QtDBus/QDBusConnection>

#include "hybrisprovider.h"

#include <unistd.h>
#include <sys/types.h>
#include <grp.h>
#include <errno.h>

int main(int argc, char *argv[])
{
    uid_t realUid;
    uid_t effectiveUid;
    uid_t savedUid;

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

    numberGroups = setgroups(numberGroups, supplementaryGroups);
    if (numberGroups == -1)
        qFatal("Failed to set supplementary groups, %s", strerror(errno));

    result = setuid(realUid);
    if (result == -1)
        qFatal("Failed to set process uid to %d, %s", realUid, strerror(errno));

    QCoreApplication a(argc, argv);

    QDBusConnection connection = QDBusConnection::sessionBus();

    if (!connection.registerService(QStringLiteral("org.freedesktop.Geoclue.Providers.Hybris")))
        qFatal("Failed to register service org.freedesktop.Geoclue.Providers.Hybris");

    HybrisProvider provider;

    if (!connection.registerObject(QStringLiteral("/org/freedesktop/Geoclue/Providers/Hybris"), &provider))
        qFatal("Failed to register object /org/freedesktop/Geoclue/Providers/Hybris");

    return a.exec();
}
