# -*- rpm-spec -*-

%global meson_version 0.49.0
%global glib2_version 2.44.0
%global libvirt_version 3.0.0
%global libvirt_glib_version 0.0.7
%global system_user libvirtdbus

Name: libvirt-dbus
Version: 1.4.1
Release: 1%{?dist}
Summary: libvirt D-Bus API binding
License: LGPLv2+
URL: https://libvirt.org/
Source0: https://libvirt.org/sources/dbus/%{name}-%{version}.tar.xz

BuildRequires: gcc
BuildRequires: meson >= %{meson_version}
BuildRequires: glib2-devel >= %{glib2_version}
BuildRequires: libvirt-devel >= %{libvirt_version}
BuildRequires: libvirt-glib-devel >= %{libvirt_glib_version}
%if 0%{?rhel} == 7
BuildRequires: python36-docutils
%else
BuildRequires: python3-docutils
%endif

Requires: dbus
Requires: glib2 >= %{glib2_version}
Requires: libvirt-libs >= %{libvirt_version}
Requires: libvirt-glib >= %{libvirt_glib_version}
Requires: polkit

Requires(pre): shadow-utils

%description
This package provides D-Bus API for libvirt

%prep
%autosetup

%build
%meson \
    -Dinit_script=systemd
%meson_build

%install
%meson_install

%pre
getent group %{system_user} >/dev/null || groupadd -r %{system_user}
getent passwd %{system_user} >/dev/null || \
    useradd -r -g %{system_user} -d / -s /sbin/nologin \
    -c "Libvirt D-Bus bridge" %{system_user}
exit 0

%files
%doc AUTHORS.rst NEWS.rst
%license COPYING
%{_sbindir}/libvirt-dbus
%{_unitdir}/libvirt-dbus.service
%{_prefix}/lib/systemd/user/libvirt-dbus.service
%{_datadir}/dbus-1/services/org.libvirt.service
%{_datadir}/dbus-1/system-services/org.libvirt.service
%{_datadir}/dbus-1/system.d/org.libvirt.conf
%{_datadir}/dbus-1/interfaces/org.libvirt.*.xml
%{_datadir}/polkit-1/rules.d/libvirt-dbus.rules
%{_mandir}/man8/libvirt-dbus.8*

%changelog
