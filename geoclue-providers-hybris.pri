TARGET = geoclue-hybris
CONFIG   += console
CONFIG   -= app_bundle
TEMPLATE = app

target.path = /usr/libexec

QT = core dbus network

CONFIG += link_pkgconfig
PKGCONFIG += connman-qt5 qofono-qt5 qofonoext systemsettings

LIBS += -lrt

dbus_geoclue.files = \
    org.freedesktop.Geoclue.xml \
    org.freedesktop.Geoclue.Position.xml \
    org.freedesktop.Geoclue.Velocity.xml \
    org.freedesktop.Geoclue.Satellite.xml
dbus_geoclue.header_flags = "-l HybrisProvider -i hybrisprovider.h"
dbus_geoclue.source_flags = "-l HybrisProvider"

DBUS_ADAPTORS = \
    dbus_geoclue

DBUS_INTERFACES = \
    com.jollamobile.Connectiond.xml \
    com.jolla.lipstick.ConnectionSelector.xml

session_dbus_service.files = org.freedesktop.Geoclue.Providers.Hybris.service
session_dbus_service.path = /usr/share/dbus-1/services

system_dbus_conf.files = com.jollamobile.gps.conf
system_dbus_conf.path = /etc/dbus-1/system.d

systemd_dbus_service.files = geoclue-providers-hybris.service
systemd_dbus_service.path = /usr/lib/systemd/user

systemd_dbus_service_symlink.path = .
systemd_dbus_service_symlink.commands = ln -s geoclue-providers-hybris.service ${INSTALL_ROOT}/usr/lib/systemd/user/dbus-org.freedesktop.Geoclue.Providers.Hybris.service

geoclue_provider.files = geoclue-hybris.provider
geoclue_provider.path = /usr/share/geoclue-providers

HEADERS += \
    hybrislocationbackend.h \
    hybrisprovider.h \
    locationtypes.h

SOURCES += \
    main.cpp \
    hybrisprovider.cpp

OTHER_FILES = \
    $${session_dbus_service.files} \
    $${system_dbus_service.files} \
    $${system_dbus_conf.files} \
    $${systemd_dbus_service.files} \
    $${geoclue_provider.files} \
    rpm/geoclue-providers-hybris.spec

INSTALLS += target session_dbus_service system_dbus_conf geoclue_provider systemd_dbus_service systemd_dbus_service_symlink
