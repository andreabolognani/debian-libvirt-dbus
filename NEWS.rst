=====================
libvirt-dbus releases
=====================

v1.4.0 (2020-05-01)
===================

* New features

  - Support for all relevant domain snapshot APIs

* Build-system changes

  - Converted to meson/ninja build system

* Documentation

  - Everything was converted to reStructuredText

v1.3.0 (2019-01-18)
===================

* New features

  - Support for all relevant interface APIs up to libvirt 3.0.0

* Bug fixes

  - Storage volumes and with special characters in the name are now correctly listed

* Build-system improvements

  - Detect flake8 automatically

  - Install daemon under @sbindir@

  - Fix version check for dependencies


v1.2.0 (2018-07-04)
===================

* Bug fixes

  - Properly deregister NodeDevice event callback

* Improvements

  - Allow system access to users in the libvirt group

  - Switch to xz compression for release archives

* Build-system fixes

  - Fix default polkit rule directory and make it configurable

  - Fix typo to properly build binaries with PIE


v1.1.0 (2018-06-21)
===================

* New features

  - Support for all relevant nwfilter APIs up to libvirt 3.0.0

  - Support for all relevant storage volume APIs up to libvirt 3.0.0

  - Support for all relevant node device APIs up to libvirt 3.0.0

* Bug fixes

  - Don't report error for GetAll on properties if some property is not accessible


v1.0.0 (2018-05-16)
===================

* Fist stable release, from now on we will not change existing APIs

* New features

  - Support for almost all relevant domain APIs and events up to libvirt 3.0.0

  - Support for all relevant connect APIs up to libvirt 3.0.0

  - Support for all relevant network APIs and events up to libvirt 3.0.0

  - Support for all relevant storage pool APIs and events up to libvirt 3.0.0

  - Support for all relevant secret APIs and evetns up to libvirt 3.0.0

* Improvements

  - Added org.gtk.GDBus.DocString annotation for D-Bus APIs

  - Added man page for libvirt-dbus daemon


v0.0.1 (2018-03-22)
===================

* First unstable release

  - Basic support for starting and destroying domains

  - Basic support for getting some information about domains

  - Basic support for listing existing and creating new domains
