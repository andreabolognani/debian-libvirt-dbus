#pragma once

#include "gdbus.h"

#include <libvirt/libvirt.h>

#define VIRT_DBUS_EMPTY_STR(s) ((s) ? (s) : "")

#define VIRT_DBUS_ERROR virtDBusErrorQuark()

#define virtDBusUtilAutoLock g_autoptr(GMutexLocker) G_GNUC_UNUSED

typedef enum {
    VIRT_DBUS_ERROR_LIBVIRT,
    VIRT_DBUS_N_ERRORS /*< skip >*/
} VirtDBusError;

GQuark
virtDBusErrorQuark(void);

struct _virtDBusUtilTypedParams {
    virTypedParameterPtr params;
    gint nparams;
};
typedef struct _virtDBusUtilTypedParams virtDBusUtilTypedParams;

void
virtDBusUtilTypedParamsClear(virtDBusUtilTypedParams *params);

G_DEFINE_AUTO_CLEANUP_CLEAR_FUNC(virtDBusUtilTypedParams, virtDBusUtilTypedParamsClear);

GVariant *
virtDBusUtilTypedParamsToGVariant(virTypedParameterPtr params,
                                  gint nparams);

gboolean
virtDBusUtilGVariantToTypedParams(GVariantIter *iter,
                                  virTypedParameterPtr *params,
                                  gint *nparams,
                                  GError **error);

void
virtDBusUtilSetLastVirtError(GError **error);

gchar *
virtDBusUtilEncodeStr(const gchar *str);

gchar *
virtDBusUtilDecodeStr(const gchar *str);

gchar *
virtDBusUtilBusPathForVirDomain(virDomainPtr domain,
                                const gchar *domainPath);

virDomainPtr
virtDBusUtilVirDomainFromBusPath(virConnectPtr connection,
                                 const gchar *path,
                                 const gchar *domainPath);

void
virtDBusUtilVirDomainListFree(virDomainPtr *domains);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(virDomain, virDomainFree);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(virDomainPtr, virtDBusUtilVirDomainListFree);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(virDomainStatsRecordPtr, virDomainStatsRecordListFree);

virDomainSnapshotPtr
virtDBusUtilVirDomainSnapshotFromBusPath(virConnectPtr connection,
                                         const gchar *path,
                                         const gchar *domainSnapshotPath);

gchar *
virtDBusUtilBusPathForVirDomainSnapshot(virDomainPtr domain,
                                        virDomainSnapshotPtr domainSnapshot,
                                        const gchar *domainSnapshotPath);

void
virtDBusUtilVirDomainSnapshotListFree(virDomainSnapshotPtr* domainSnapshots);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(virDomainSnapshot, virDomainSnapshotFree);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(virDomainSnapshotPtr, virtDBusUtilVirDomainSnapshotListFree);

virInterfacePtr
virtDBusUtilVirInterfaceFromBusPath(virConnectPtr connection,
                                    const gchar *path,
                                    const gchar *interfacePath);

gchar *
virtDBusUtilBusPathForVirInterface(virInterfacePtr interface,
                                   const gchar *interfacePath);

void
virtDBusUtilVirInterfaceListFree(virInterfacePtr *interfaces);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(virInterface, virInterfaceFree);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(virInterfacePtr, virtDBusUtilVirInterfaceListFree);

virNetworkPtr
virtDBusUtilVirNetworkFromBusPath(virConnectPtr connection,
                                  const gchar *path,
                                  const gchar *networkPath);

gchar *
virtDBusUtilBusPathForVirNetwork(virNetworkPtr network,
                                 const gchar *networkPath);

void
virtDBusUtilVirNetworkListFree(virNetworkPtr *networks);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(virNetwork, virNetworkFree);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(virNetworkPtr, virtDBusUtilVirNetworkListFree);

virNodeDevicePtr
virtDBusUtilVirNodeDeviceFromBusPath(virConnectPtr connection,
                                     const gchar *path,
                                     const gchar *nodeDevPath);

gchar *
virtDBusUtilBusPathForVirNodeDevice(virNodeDevicePtr NodeDevice,
                                    const gchar *nodeDevPath);

void
virtDBusUtilVirNodeDeviceListFree(virNodeDevicePtr *devs);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(virNodeDevice, virNodeDeviceFree);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(virNodeDevicePtr, virtDBusUtilVirNodeDeviceListFree);

virNWFilterPtr
virtDBusUtilVirNWFilterFromBusPath(virConnectPtr connection,
                                   const gchar *path,
                                   const gchar *nwfilterPath);

gchar *
virtDBusUtilBusPathForVirNWFilter(virNWFilterPtr nwfilter,
                                  const gchar *nwfilterPath);

void
virtDBusUtilVirNWFilterListFree(virNWFilterPtr *nwfilters);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(virNWFilter, virNWFilterFree);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(virNWFilterPtr, virtDBusUtilVirNWFilterListFree);

typedef gchar *virtDBusCharArray;

void
virtDBusUtilStringListFree(virtDBusCharArray *item);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(virtDBusCharArray, virtDBusUtilStringListFree);

virSecretPtr
virtDBusUtilVirSecretFromBusPath(virConnectPtr connection,
                                 const gchar *path,
                                 const gchar *secretPath);

gchar *
virtDBusUtilBusPathForVirSecret(virSecretPtr secret,
                                const gchar *secretPath);

void
virtDBusUtilVirSecretListFree(virSecretPtr *secrets);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(virSecret, virSecretFree);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(virSecretPtr,
                              virtDBusUtilVirSecretListFree);

virStoragePoolPtr
virtDBusUtilVirStoragePoolFromBusPath(virConnectPtr connection,
                                      const gchar *path,
                                      const gchar *storagePoolPath);

gchar *
virtDBusUtilBusPathForVirStoragePool(virStoragePoolPtr storagePool,
                                     const gchar *storagePoolPath);

void
virtDBusUtilVirStoragePoolListFree(virStoragePoolPtr *storagePools);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(virStoragePool, virStoragePoolFree);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(virStoragePoolPtr,
                              virtDBusUtilVirStoragePoolListFree);

virStorageVolPtr
virtDBusUtilVirStorageVolFromBusPath(virConnectPtr connection,
                                     const gchar *path,
                                     const gchar *storageVolPath);

gchar *
virtDBusUtilBusPathForVirStorageVol(virStorageVolPtr storageVol,
                                    const gchar *storageVolPath);

void
virtDBusUtilVirStorageVolListFree(virStorageVolPtr *storageVols);

G_DEFINE_AUTOPTR_CLEANUP_FUNC(virStorageVol, virStorageVolFree);
G_DEFINE_AUTOPTR_CLEANUP_FUNC(virStorageVolPtr,
                              virtDBusUtilVirStorageVolListFree);
