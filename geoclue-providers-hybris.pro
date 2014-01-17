TARGET = geoclue-hybris
CONFIG   += console
CONFIG   -= app_bundle
TEMPLATE = app

target.path = /usr/libexec

QT = core dbus network

CONFIG += link_pkgconfig
PKGCONFIG += libhardware android-headers contextkit-statefs connman-qt5 qofono-qt5 mlite5

DBUS_INTERFACES = \
    com.jollamobile.Connectiond.xml

DBUS_ADAPTORS = \
    org.freedesktop.Geoclue.xml \
    org.freedesktop.Geoclue.Position.xml \
    org.freedesktop.Geoclue.Velocity.xml \
    org.freedesktop.Geoclue.Satellite.xml

QDBUSXML2CPP_ADAPTOR_HEADER_FLAGS += "-l HybrisProvider -i hybrisprovider.h"
QDBUSXML2CPP_ADAPTOR_SOURCE_FLAGS += "-l HybrisProvider"
dbus_adaptor_source.depends = ${QMAKE_FILE_OUT_BASE}.h

dbus_service.files = org.freedesktop.Geoclue.Providers.Hybris.service
dbus_service.path = /usr/share/dbus-1/services

geoclue_provider.files = geoclue-hybris.provider
geoclue_provider.path = /usr/share/geoclue-providers

HEADERS += \
    hybrisprovider.h \
    locationtypes.h

SOURCES += \
    main.cpp \
    hybrisprovider.cpp

OTHER_FILES = \
    $${DBUS_INTERFACES} \
    $${DBUS_ADAPTORS} \
    $${dbus_service.files} \
    geoclue-hybris.provider \
    rpm/geoclue-providers-hybris.spec

INSTALLS += target dbus_service geoclue_provider
