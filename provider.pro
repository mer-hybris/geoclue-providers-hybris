TARGET = geoclue-hybris
CONFIG   += console
CONFIG   -= app_bundle
TEMPLATE = app

target.path = /usr/libexec

QT = core dbus network

CONFIG += link_pkgconfig
PKGCONFIG += libhardware android-headers connman-qt5 qofono-qt5

LIBS += -lrt -L. -lhybris_dbus -lcontrol_dbus

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
    $${DBUS_INTERFACES} \
    $${session_dbus_service.files} \
    $${system_dbus_service.files} \
    $${system_dbus_conf.files} \
    $${geoclue_provider.files}

INSTALLS += target session_dbus_service system_dbus_conf geoclue_provider
