TARGET = geoclue-hybris
CONFIG   += console
CONFIG   -= app_bundle
TEMPLATE = app

target.path = /usr/libexec

QT = core dbus network

CONFIG += link_pkgconfig
PKGCONFIG += libhardware android-headers connman-qt5 qofono-qt5 qofonoext

LIBS += -lrt

dbus_power_control.files = com.jollamobile.gps.Device.xml
dbus_power_control.header_flags = "-l DeviceControl -i devicecontrol.h"
dbus_power_control.source_flags = "-l DeviceControl"

dbus_geoclue.files = \
    org.freedesktop.Geoclue.xml \
    org.freedesktop.Geoclue.Position.xml \
    org.freedesktop.Geoclue.Velocity.xml \
    org.freedesktop.Geoclue.Satellite.xml
dbus_geoclue.header_flags = "-l HybrisProvider -i hybrisprovider.h"
dbus_geoclue.source_flags = "-l HybrisProvider"

DBUS_ADAPTORS = \
    dbus_power_control \
    dbus_geoclue

DBUS_INTERFACES = \
    com.jollamobile.Connectiond.xml \
    com.jolla.lipstick.ConnectionSelector.xml

session_dbus_service.files = org.freedesktop.Geoclue.Providers.Hybris.service
session_dbus_service.path = /usr/share/dbus-1/services

system_dbus_conf.files = com.jollamobile.gps.conf
system_dbus_conf.path = /etc/dbus-1/system.d

geoclue_provider.files = geoclue-hybris.provider
geoclue_provider.path = /usr/share/geoclue-providers

HEADERS += \
    hybrisprovider.h \
    locationtypes.h \
    devicecontrol.h

SOURCES += \
    main.cpp \
    hybrisprovider.cpp \
    devicecontrol.cpp

OTHER_FILES = \
    $${session_dbus_service.files} \
    $${system_dbus_service.files} \
    $${system_dbus_conf.files} \
    $${geoclue_provider.files} \
    rpm/geoclue-providers-hybris.spec

INSTALLS += target session_dbus_service system_dbus_conf geoclue_provider
