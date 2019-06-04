Summary: Geoinformation Service Hybris Provider
Group: System/Libraries
URL: https://bitbucket.org/jolla/base-geoclue-providers-hybris
License: LGPLv2.1
Source: %{name}-%{version}.tar.gz
BuildRequires: pkgconfig(Qt5Core)
BuildRequires: pkgconfig(Qt5DBus)
BuildRequires: pkgconfig(Qt5Network)
BuildRequires: pkgconfig(connman-qt5) >= 1.0.68
BuildRequires: pkgconfig(qofono-qt5)
BuildRequires: pkgconfig(qofonoext)
BuildRequires: pkgconfig(systemsettings) >= 0.5.5
Requires: connectionagent-qt5 >= 0.9.20

Source100: geoclue-providers-hybris.inc

%description
%{summary}.


%prep
%setup -q -n %{name}-%{version}


%build
%{qmake_command}
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

