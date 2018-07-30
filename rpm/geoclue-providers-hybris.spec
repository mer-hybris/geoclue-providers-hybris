Name: geoclue-provider-hybris
Version: 0.0.1
Release: 1
Summary: Geoinformation Service Hybris Provider
Group: System/Libraries
URL: https://bitbucket.org/jolla/base-geoclue-providers-hybris
License: LGPLv2.1
Source: %{name}-%{version}.tar.gz
BuildRequires: pkgconfig(Qt5Core)
BuildRequires: pkgconfig(Qt5DBus)
BuildRequires: pkgconfig(Qt5Network)
BuildRequires: pkgconfig(libhardware)
BuildRequires: pkgconfig(android-headers)
BuildRequires: pkgconfig(connman-qt5) >= 1.0.68
BuildRequires: pkgconfig(qofono-qt5)
BuildRequires: pkgconfig(qofonoext)
BuildRequires: pkgconfig(systemsettings)
Requires: connectionagent-qt5 >= 0.9.20

%description
%{summary}.


%prep
%setup -q -n %{name}-%{version}


%build
qmake -qt=5 hal/hallocationbackend.pro
make %{?_smp_mflags}


%install
make INSTALL_ROOT=%{buildroot} install

%files
%defattr(04755,root,root,-)
%{_libexecdir}/geoclue-hybris
%defattr(-,root,root,-)
%{_sysconfdir}/dbus-1
%{_datadir}/dbus-1
%{_datadir}/geoclue-providers/geoclue-hybris.provider

