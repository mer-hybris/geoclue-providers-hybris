TEMPLATE = lib
CONFIG += static
TARGET = hybris_dbus

QT = core dbus

CONFIG += link_pkgconfig
PKGCONFIG += libhardware android-headers

DBUS_ADAPTORS = \
    org.freedesktop.Geoclue.xml \
    org.freedesktop.Geoclue.Position.xml \
    org.freedesktop.Geoclue.Velocity.xml \
    org.freedesktop.Geoclue.Satellite.xml

QDBUSXML2CPP_ADAPTOR_HEADER_FLAGS += "-l HybrisProvider -i hybrisprovider.h"
QDBUSXML2CPP_ADAPTOR_SOURCE_FLAGS += "-l HybrisProvider"
dbus_adaptor_source.depends = ${QMAKE_FILE_OUT_BASE}.h

OTHER_FILES = \
    $${DBUS_ADAPTORS}
