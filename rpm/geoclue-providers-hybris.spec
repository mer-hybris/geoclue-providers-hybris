BuildRequires: pkgconfig(libhardware)
BuildRequires: pkgconfig(android-headers)

# hardcoded build command, can be autodetected
%define qmake_command qmake -qt=5 hal/hallocationbackend.pro

%include rpm/geoclue-providers-hybris.inc

