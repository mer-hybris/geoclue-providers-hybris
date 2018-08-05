include($$PWD/../geoclue-providers-hybris.pri)

HEADERS += \
    binderlocationbackend.h

SOURCES += \
    binderlocationbackend.cpp

PKGCONFIG += libgbinder libglibutil gobject-2.0 glib-2.0
