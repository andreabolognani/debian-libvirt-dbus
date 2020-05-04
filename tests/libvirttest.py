from enum import IntEnum
from dbus.mainloop.glib import DBusGMainLoop
from gi.repository import GLib
import dbus
import os
import pytest
import subprocess
import sys
import time
import xmldata


root = os.environ.get('abs_top_builddir')
exe = os.path.join(root, 'src', 'libvirt-dbus')

DBusGMainLoop(set_as_default=True)


def run():
    exit(pytest.main(sys.argv))


class BaseTestClass():
    """ Base test class for whole test suite
    """
    connect = None
    bus = None
    libvirt_dbus = None
    loop = False

    @pytest.fixture(autouse=True)
    def libvirt_dbus_setup(self, request):
        """Start libvirt-dbus for each test function
        """
        os.environ['LIBVIRT_DEBUG'] = '3'
        self.libvirt_dbus = subprocess.Popen([exe, '--session'])
        self.bus = dbus.SessionBus()

        for i in range(10):
            if self.bus.name_has_owner('org.libvirt'):
                break
            time.sleep(0.1)
        else:
            raise TimeoutError('error starting libvirt-dbus')

        obj = self.bus.get_object('org.libvirt', '/org/libvirt/Test')
        self.connect = dbus.Interface(obj, 'org.libvirt.Connect')

    @pytest.fixture(autouse=True)
    def libvirt_dbus_teardown(self):
        """Terminate libvirt-dbus at the teardown of each test
        """
        yield
        self.libvirt_dbus.terminate()
        self.libvirt_dbus.wait(timeout=10)

    def main_loop(self):
        """Initializes the mainloop
        """
        assert getattr(self, 'loop', False) is False

        def timeout():
            self.loop.quit()
            del self.loop
            self.timeout = True

        self.timeout = False
        self.loop = GLib.MainLoop()
        GLib.timeout_add(2000, timeout)
        self.loop.run()
        if self.timeout:
            raise TimeoutError()

    @pytest.fixture
    def interface_create(self):
        """ Fixture to define dummy interface on the test driver

        This fixture should be used in the setup of every test manipulating
        with interfaces.
        """
        path = self.connect.InterfaceDefineXML(xmldata.minimal_interface_xml, 0)
        obj = self.bus.get_object('org.libvirt', path)
        interface_obj = dbus.Interface(obj, 'org.libvirt.Interface')
        interface_obj.Create(0)
        return path, interface_obj

    def node_device_create(self):
        # We need a usable parent nodedev: possible candidates are
        # scsi_host2 (available since libvirt 3.1.0) and
        # test-scsi-host-vport (available until libvirt 3.0.0).
        #
        # Looking up a non-existing nodedev raises an exception, so
        # we need to ignore them here: that's okay, because if we
        # can't find any then NodeDeviceCreateXML() itself will fail
        xml = xmldata.minimal_node_device_xml
        for parent in ['scsi_host2', 'test-scsi-host-vport']:
            try:
                if self.connect.NodeDeviceLookupByName(parent):
                    xml = xml.replace('@parent@', parent)
                    break
            except dbus.exceptions.DBusException:
                pass
        path = self.connect.NodeDeviceCreateXML(xml, 0)
        return path

    @pytest.fixture(name="node_device_create")
    def fixture_node_device_create(self):
        """ Fixture to create dummy node device on the test driver

        This fixture should be used in the setup of every test manipulating
        with node devices.
        """
        return self.node_device_create()

    @pytest.fixture
    def snapshot_create(self):
        """ Fixture to create simple domain snapshot on the test driver

        This fixture should be used in the setup of every test manipulating
        with domain snaphots.
        """
        _, test_domain = self.get_test_domain()
        path = test_domain.SnapshotCreateXML(xmldata.minimal_snapshot_xml, 0)
        obj = self.bus.get_object('org.libvirt', path)
        return dbus.Interface(obj, 'org.libvirt.DomainSnapshot')

    @pytest.fixture
    def storage_volume_create(self):
        """ Fixture to create dummy storage volume on the test driver

        This fixture should be used in the setup of every test manipulating
        with test volume.
        """
        _, test_storage_pool = self.get_test_storage_pool()
        interface_obj = dbus.Interface(test_storage_pool,
                                       'org.libvirt.StoragePool')
        path = interface_obj.StorageVolCreateXML(xmldata.minimal_storage_vol_xml, 0)
        yield path

    def get_test_domain(self):
        path = self.connect.ListDomains(0)[0]
        obj = self.bus.get_object('org.libvirt', path)
        return obj, dbus.Interface(obj, 'org.libvirt.Domain')

    def get_test_network(self):
        """Fetch information for the test network from test driver

        Returns:
            (dbus.proxies.ProxyObject, dbus.proxies.ProxyObject):
            Test Network Object, Local proxy for the test Network Object.

        """
        path = self.connect.ListNetworks(0)[0]
        obj = self.bus.get_object('org.libvirt', path)
        return path, obj

    def get_test_storage_pool(self):
        """Fetch information for the test storage pool from test driver

        Returns:
            (dbus.proxies.ProxyObject, dbus.proxies.ProxyObject):
            Test StoragePool Object, Local proxy for the test StoragePool
            Object.

        """
        path = self.connect.ListStoragePools(0)[0]
        obj = self.bus.get_object('org.libvirt', path)
        return path, obj

    def get_test_storage_volume(self):
        """Fetch information for the test storage vol from test driver

        Returns:
            (dbus.proxies.ProxyObject, dbus.proxies.ProxyObject):
            Test StorageVol Object, Local proxy for the test StorageVol
            Object.

        """
        _, test_storage_pool = self.get_test_storage_pool()
        pool_iface = dbus.Interface(test_storage_pool,
                                    'org.libvirt.StoragePool')
        path = pool_iface.ListStorageVolumes(0)[0]
        obj = self.bus.get_object('org.libvirt', path)
        return path, obj


class DomainEvent(IntEnum):
    DEFINED = 0
    UNDEFINED = 1
    STARTED = 2
    SUSPENDED = 3
    RESUMED = 4
    STOPPED = 5
    SHUTDOWN = 6
    PMSUSPENDED = 7
    CRASHED = 8


class DomainEventDefinedDetailType(IntEnum):
    ADDED = 0
    UPDATED = 1
    RENAMED = 2
    FROM_SNAPSHOT = 3
    LAST = 4


class DomainEventResumedDetailType(IntEnum):
    UNPAUSED = 0
    MIGRATED = 1
    FROM_SNAPSHOT = 2
    POSTCOPY = 3
    LAST = 4


class DomainEventStartedDetailType(IntEnum):
    BOOTED = 0
    MIGRATED = 1
    RESTORED = 2
    FROM_SNAPSHOT = 3
    WAKEUP = 4
    LAST = 5


class DomainEventStoppedDetailType(IntEnum):
    SHUTDOWN = 0
    DESTROYED = 1
    CRASHED = 2
    MIGRATED = 3
    SAVED = 4
    FAILED = 5
    FROM_SNAPSHOT = 6
    LAST = 7


class DomainEventSuspendedDetailType(IntEnum):
    PAUSED = 0
    MIGRATED = 1
    IOERROR = 2
    WATCHDOG = 3
    RESTORED = 4
    FROM_SNAPSHOT = 5
    API_ERROR = 6
    POSTCOPY = 7
    POSTCOPY_FAILED = 8
    LAST = 9


class DomainEventUndefinedDetailType(IntEnum):
    REMOVED = 0
    RENAMED = 1
    LAST = 2


class DomainState(IntEnum):
    NOSTATE = 0
    RUNNING = 1
    BLOCKED = 2
    PAUSED = 3
    SHUTDOWN = 4
    SHUTOFF = 5
    CRASHED = 6
    PMSUSPENDED = 7
    LAST = 8


class NetworkEvent(IntEnum):
    DEFINED = 0
    UNDEFINED = 1
    STARTED = 2
    STOPPED = 3


class NodeDeviceEvent(IntEnum):
    CREATED = 0
    DELETED = 1


class StoragePoolBuildFlags(IntEnum):
    NEW = 0
    REPAIR = 1
    RESIZE = 2
    NO_OVERWRITE = 4
    OVERWRITE = 8


class StoragePoolEvent(IntEnum):
    DEFINED = 0
    UNDEFINED = 1
    STARTED = 2
    STOPPED = 3
    LAST = 4
