Name: geoclue-provider-hybris
Version: 0.0.1
Release: 1
Summary: Geoinformation Service Hybris Provider
Group: System/Libraries
URL: https://bitbucket.org/jolla/base-geoclue-providers-hybris
License: Proprietary
Source: %{name}-%{version}.tar.gz
BuildRequires: pkgconfig(Qt5Core)
BuildRequires: pkgconfig(Qt5DBus)
BuildRequires: pkgconfig(Qt5Network)
BuildRequires: pkgconfig(libhardware)
BuildRequires: pkgconfig(android-headers)
BuildRequires: pkgconfig(contextkit-statefs)
BuildRequires: pkgconfig(connman-qt5)
BuildRequires: pkgconfig(qofono-qt5)

%description
%{summary}.


%prep
%setup -q -n %{name}-%{version}


%build
qmake -qt=5
make %{?_smp_mflags}


%install
make INSTALL_ROOT=%{buildroot} install

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(04755,root,root,-)
%{_libexecdir}/geoclue-hybris
%defattr(-,root,root,-)
%{_datadir}/dbus-1/services/org.freedesktop.Geoclue.Providers.Hybris.service
%{_datadir}/geoclue-providers/geoclue-hybris.provider

