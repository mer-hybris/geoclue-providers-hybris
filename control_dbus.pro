TEMPLATE = lib
CONFIG += static
TARGET = control_dbus

QT = core dbus

DBUS_ADAPTORS = \
    com.jollamobile.gps.Device.xml

QDBUSXML2CPP_ADAPTOR_HEADER_FLAGS += "-l DeviceControl -i devicecontrol.h"
QDBUSXML2CPP_ADAPTOR_SOURCE_FLAGS += "-l DeviceControl"
dbus_adaptor_source.depends = ${QMAKE_FILE_OUT_BASE}.h

OTHER_FILES = \
    $${DBUS_ADAPTORS}
