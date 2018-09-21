# libgbinder is currently not a common package, so we have to rely on tar_git.
BuildRequires: pkgconfig(libgbinder)

# hardcoded build command, can be autodetected
%define qmake_command qmake -qt=5 binder/binderlocationbackend.pro

%include rpm/geoclue-providers-hybris.inc

