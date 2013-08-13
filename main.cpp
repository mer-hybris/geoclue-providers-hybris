/*
    Copyright (C) 2013 Jolla Ltd.
    Contact: Aaron McCarthy <aaron.mccarthy@jollamobile.com>
*/

#include <QtCore/QCoreApplication>
#include <QtDBus/QDBusConnection>

#include "hybrisprovider.h"

#include <QtCore/QDebug>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    QDBusConnection connection = QDBusConnection::sessionBus();

    if (!connection.registerService(QStringLiteral("org.freedesktop.Geoclue.Providers.Hybris")))
        qFatal("Failed to register service org.freedesktop.Geoclue.Providers.Hybris");

    HybrisProvider provider;

    if (!connection.registerObject(QStringLiteral("/org/freedesktop/Geoclue/Providers/Hybris"), &provider))
        qFatal("Failed to register object /org/freedesktop/Geoclue/Providers/Hybris");

    return a.exec();
}
