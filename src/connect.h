#pragma once

#define VIR_ENUM_SENTINELS

#include "util.h"

#include <libvirt/libvirt.h>

#define VIRT_DBUS_CONNECT_INTERFACE "org.libvirt.Connect"

struct virtDBusConnect {
    GDBusConnection *bus;
    const gchar *uri;
    const gchar *connectPath;
    gchar *domainPath;
    gchar *domainSnapshotPath;
    gchar *interfacePath;
    gchar *networkPath;
    gchar *nodeDevPath;
    gchar *nwfilterPath;
    gchar *secretPath;
    gchar *storagePoolPath;
    gchar *storageVolPath;
    virConnectPtr connection;
    GMutex lock;

    gint domainCallbackIds[VIR_DOMAIN_EVENT_ID_LAST];
    gint networkCallbackIds[VIR_NETWORK_EVENT_ID_LAST];
    gint nodeDevCallbackIds[VIR_NODE_DEVICE_EVENT_ID_LAST];
    gint secretCallbackIds[VIR_SECRET_EVENT_ID_LAST];
    gint storagePoolCallbackIds[VIR_STORAGE_POOL_EVENT_ID_LAST];
};
typedef struct virtDBusConnect virtDBusConnect;

void
virtDBusConnectNew(virtDBusConnect **connectp,
                   GDBusConnection *bus,
                   const gchar *uri,
                   const gchar *connectPath,
                   GError **error);

gboolean
virtDBusConnectOpen(virtDBusConnect *connect,
                    GError **error);

void
virtDBusConnectListFree(virtDBusConnect **connectList);

void
virtDBusConnectFree(virtDBusConnect *connect);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(virtDBusConnect, virtDBusConnectFree);
