#include "connect.h"
#include "domain.h"
#include "domainsnapshot.h"
#include "events.h"
#include "interface.h"
#include "network.h"
#include "nodedev.h"
#include "nwfilter.h"
#include "secret.h"
#include "storagepool.h"
#include "storagevol.h"
#include "util.h"

#include <gio/gunixfdlist.h>
#include <glib/gprintf.h>

static gint virtDBusConnectCredType[] = {
    VIR_CRED_AUTHNAME,
    VIR_CRED_ECHOPROMPT,
    VIR_CRED_REALM,
    VIR_CRED_PASSPHRASE,
    VIR_CRED_NOECHOPROMPT,
    VIR_CRED_EXTERNAL,
};

static gint
virtDBusConnectAuthCallback(virConnectCredentialPtr cred G_GNUC_UNUSED,
                            guint ncred G_GNUC_UNUSED,
                            gpointer cbdata)
{
    GError **error = cbdata;
    g_set_error(error, VIRT_DBUS_ERROR, VIRT_DBUS_ERROR_LIBVIRT,
                "Interactive authentication is not supported. "
                "Use client configuration file for libvirt.");
    return -1;
}

static virConnectAuth virtDBusConnectAuth = {
    virtDBusConnectCredType,
    G_N_ELEMENTS(virtDBusConnectCredType),
    virtDBusConnectAuthCallback,
    NULL,
};

static void
virtDBusConnectClose(virtDBusConnect *connect,
                     gboolean deregisterEvents)
{

    for (gint i = 0; i < VIR_DOMAIN_EVENT_ID_LAST; i++) {
        if (connect->domainCallbackIds[i] >= 0) {
            if (deregisterEvents) {
                virConnectDomainEventDeregisterAny(connect->connection,
                                                   connect->domainCallbackIds[i]);
            }
            connect->domainCallbackIds[i] = -1;
        }
    }

    for (gint i = 0; i < VIR_NETWORK_EVENT_ID_LAST; i++) {
        if (connect->networkCallbackIds[i] >= 0) {
            if (deregisterEvents) {
                virConnectNetworkEventDeregisterAny(connect->connection,
                                                    connect->networkCallbackIds[i]);
            }
            connect->networkCallbackIds[i] = -1;
        }
    }

    for (gint i = 0; i < VIR_NODE_DEVICE_EVENT_ID_LAST; i++) {
        if (connect->nodeDevCallbackIds[i] >= 0) {
            if (deregisterEvents) {
                virConnectNodeDeviceEventDeregisterAny(connect->connection,
                                                       connect->nodeDevCallbackIds[i]);
            }
            connect->nodeDevCallbackIds[i] = -1;
        }
    }

    for (gint i = 0; i < VIR_SECRET_EVENT_ID_LAST; i++) {
        if (connect->secretCallbackIds[i] >= 0) {
            if (deregisterEvents) {
                virConnectSecretEventDeregisterAny(connect->connection,
                                                   connect->secretCallbackIds[i]);
            }
            connect->secretCallbackIds[i] = -1;
        }
    }

    for (gint i = 0; i < VIR_STORAGE_POOL_EVENT_ID_LAST; i++) {
        if (connect->storagePoolCallbackIds[i] >= 0) {
            if (deregisterEvents) {
                virConnectStoragePoolEventDeregisterAny(connect->connection,
                                                        connect->storagePoolCallbackIds[i]);
            }
            connect->storagePoolCallbackIds[i] = -1;
        }
    }

    virConnectClose(connect->connection);
    connect->connection = NULL;
}

gboolean
virtDBusConnectOpen(virtDBusConnect *connect,
                    GError **error)
{
    virtDBusUtilAutoLock lock = g_mutex_locker_new(&connect->lock);

    if (connect->connection) {
        if (virConnectIsAlive(connect->connection))
            return TRUE;
        else
            virtDBusConnectClose(connect, FALSE);
    }

    virtDBusConnectAuth.cbdata = error;

    connect->connection = virConnectOpenAuth(connect->uri,
                                             &virtDBusConnectAuth, 0);
    if (!connect->connection) {
        if (error && !*error)
            virtDBusUtilSetLastVirtError(error);
        return FALSE;
    }

    virtDBusEventsRegister(connect);

    return TRUE;
}

static void
virtDBusConnectGetEncrypted(const gchar *objectPath G_GNUC_UNUSED,
                            gpointer userData,
                            GVariant **value,
                            GError **error)
{
    virtDBusConnect *connect = userData;
    gint encrypted;

    if (!virtDBusConnectOpen(connect, error))
        return;

    encrypted = virConnectIsEncrypted(connect->connection);
    if (encrypted < 0)
        return virtDBusUtilSetLastVirtError(error);

    *value = g_variant_new("b", !!encrypted);
}

static void
virtDBusConnectGetHostname(const gchar *objectPath G_GNUC_UNUSED,
                           gpointer userData,
                           GVariant **value,
                           GError **error)
{
    virtDBusConnect *connect = userData;
    g_autofree gchar * hostname = NULL;

    if (!virtDBusConnectOpen(connect, error))
        return;

    hostname = virConnectGetHostname(connect->connection);
    if (!hostname)
        return virtDBusUtilSetLastVirtError(error);

    *value = g_variant_new("s", hostname);
}

static void
virtDBusConnectGetLibVersion(const gchar *objectPath G_GNUC_UNUSED,
                             gpointer userData,
                             GVariant **value,
                             GError **error)
{
    virtDBusConnect *connect = userData;
    gulong libVer;

    if (!virtDBusConnectOpen(connect, error))
        return;

    if (virConnectGetLibVersion(connect->connection, &libVer) < 0)
        return virtDBusUtilSetLastVirtError(error);

    *value = g_variant_new("t", libVer);
}

static void
virtDBusConnectGetSecure(const gchar *objectPath G_GNUC_UNUSED,
                         gpointer userData,
                         GVariant **value,
                         GError **error)
{
    virtDBusConnect *connect = userData;
    gint secure;

    if (!virtDBusConnectOpen(connect, error))
        return;

    secure = virConnectIsEncrypted(connect->connection);
    if (secure < 0)
        return virtDBusUtilSetLastVirtError(error);

    *value = g_variant_new("b", !!secure);
}

static void
virtDBusConnectGetVersion(const gchar *objectPath G_GNUC_UNUSED,
                          gpointer userData,
                          GVariant **value,
                          GError **error)
{
    virtDBusConnect *connect = userData;
    gulong hvVer;

    if (!virtDBusConnectOpen(connect, error))
        return;

    if (virConnectGetVersion(connect->connection, &hvVer) < 0)
        return virtDBusUtilSetLastVirtError(error);

    *value = g_variant_new("t", hvVer);
}

static void
virtDBusConnectGetCapabilities(GVariant *inArgs G_GNUC_UNUSED,
                               GUnixFDList *inFDs G_GNUC_UNUSED,
                               const gchar *objectPath G_GNUC_UNUSED,
                               gpointer userData,
                               GVariant **outArgs,
                               GUnixFDList **outFDs G_GNUC_UNUSED,
                               GError **error)
{
    virtDBusConnect *connect = userData;
    g_autofree gchar *capabilities = NULL;

    if (!virtDBusConnectOpen(connect, error))
        return;

    capabilities = virConnectGetCapabilities(connect->connection);
    if (!capabilities)
        return virtDBusUtilSetLastVirtError(error);

    *outArgs = g_variant_new("(s)", capabilities);
}

static void
virtDBusConnectBaselineCPU(GVariant *inArgs,
                           GUnixFDList *inFDs G_GNUC_UNUSED,
                           const gchar *objectPath G_GNUC_UNUSED,
                           gpointer userData,
                           GVariant **outArgs,
                           GUnixFDList **outFDs G_GNUC_UNUSED,
                           GError **error)
{
    virtDBusConnect *connect = userData;
    g_autofree const gchar **xmlCPUs = NULL;
    g_autoptr(GVariantIter) iter = NULL;
    const gchar **tmp;
    gsize ncpus;
    g_autofree gchar *cpu = NULL;
    guint flags;

    g_variant_get(inArgs, "(asu)", &iter, &flags);

    ncpus = g_variant_iter_n_children(iter);
    if (ncpus > 0) {
        xmlCPUs = g_new0(const gchar *, ncpus);
        tmp = xmlCPUs;
        while (g_variant_iter_next(iter, "&s", tmp))
            tmp++;
    }

    if (!virtDBusConnectOpen(connect, error))
        return;

    cpu = virConnectBaselineCPU(connect->connection, xmlCPUs, ncpus, flags);
    if (!cpu)
        return virtDBusUtilSetLastVirtError(error);

    *outArgs = g_variant_new("(s)", cpu);
}

static void
virtDBusConnectCompareCPU(GVariant *inArgs,
                          GUnixFDList *inFDs G_GNUC_UNUSED,
                          const gchar *objectPath G_GNUC_UNUSED,
                          gpointer userData,
                          GVariant **outArgs,
                          GUnixFDList **outFDs G_GNUC_UNUSED,
                          GError **error)
{
    virtDBusConnect *connect = userData;
    const gchar *xmlDesc;
    guint flags;
    gint compareResult;

    g_variant_get(inArgs, "(&su)", &xmlDesc, &flags);

    if (!virtDBusConnectOpen(connect, error))
        return;

    compareResult = virConnectCompareCPU(connect->connection, xmlDesc, flags);
    if (compareResult < 0)
        return virtDBusUtilSetLastVirtError(error);

    *outArgs = g_variant_new("(i)", compareResult);
}

static void
virtDBusConnectDomainCreateXML(GVariant *inArgs,
                               GUnixFDList *inFDs G_GNUC_UNUSED,
                               const gchar *objectPath G_GNUC_UNUSED,
                               gpointer userData,
                               GVariant **outArgs,
                               GUnixFDList **outFDs G_GNUC_UNUSED,
                               GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virDomain) domain = NULL;
    g_autofree gchar *path = NULL;
    gchar *xml;
    guint flags;

    g_variant_get(inArgs, "(&su)", &xml, &flags);

    if (!virtDBusConnectOpen(connect, error))
        return;

    domain = virDomainCreateXML(connect->connection, xml, flags);
    if (!domain)
        return virtDBusUtilSetLastVirtError(error);

    path = virtDBusUtilBusPathForVirDomain(domain, connect->domainPath);

    *outArgs = g_variant_new("(o)", path);
}

static void
virtDBusConnectDomainCreateXMLWithFiles(GVariant *inArgs,
                                        GUnixFDList *inFDs,
                                        const gchar *objectPath G_GNUC_UNUSED,
                                        gpointer userData,
                                        GVariant **outArgs,
                                        GUnixFDList **outFDs G_GNUC_UNUSED,
                                        GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virDomain) domain = NULL;
    g_autofree gchar *path = NULL;
    gchar *xml;
    const gint *files = NULL;
    guint nfiles = 0;
    guint flags;

    g_variant_get(inArgs, "(&sahu)", &xml, NULL, &flags);

    if (inFDs) {
        nfiles = g_unix_fd_list_get_length(inFDs);
        if (nfiles > 0)
            files = g_unix_fd_list_peek_fds(inFDs, NULL);
    }

    if (!virtDBusConnectOpen(connect, error))
        return;

    domain = virDomainCreateXMLWithFiles(connect->connection, xml, nfiles,
                                         (gint *)files, flags);
    if (!domain)
        return virtDBusUtilSetLastVirtError(error);

    path = virtDBusUtilBusPathForVirDomain(domain, connect->domainPath);

    *outArgs = g_variant_new("(o)", path);
}

static void
virtDBusConnectDomainDefineXML(GVariant *inArgs,
                               GUnixFDList *inFDs G_GNUC_UNUSED,
                               const gchar *objectPath G_GNUC_UNUSED,
                               gpointer userData,
                               GVariant **outArgs,
                               GUnixFDList **outFDs G_GNUC_UNUSED,
                               GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virDomain) domain = NULL;
    g_autofree gchar *path = NULL;
    gchar *xml;

    g_variant_get(inArgs, "(&s)", &xml);

    if (!virtDBusConnectOpen(connect, error))
        return;

    domain = virDomainDefineXML(connect->connection, xml);
    if (!domain)
        return virtDBusUtilSetLastVirtError(error);

    path = virtDBusUtilBusPathForVirDomain(domain, connect->domainPath);

    *outArgs = g_variant_new("(o)", path);
}

static void
virtDBusConnectDomainLookupByID(GVariant *inArgs,
                                GUnixFDList *inFDs G_GNUC_UNUSED,
                                const gchar *objectPath G_GNUC_UNUSED,
                                gpointer userData,
                                GVariant **outArgs,
                                GUnixFDList **outFDs G_GNUC_UNUSED,
                                GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virDomain) domain = NULL;
    g_autofree gchar *path = NULL;
    gint id;

    g_variant_get(inArgs, "(i)", &id);

    if (!virtDBusConnectOpen(connect, error))
        return;

    domain = virDomainLookupByID(connect->connection, id);
    if (!domain)
        return virtDBusUtilSetLastVirtError(error);

    path = virtDBusUtilBusPathForVirDomain(domain, connect->domainPath);

    *outArgs = g_variant_new("(o)", path);
}

static void
virtDBusConnectDomainLookupByName(GVariant *inArgs,
                                  GUnixFDList *inFDs G_GNUC_UNUSED,
                                  const gchar *objectPath G_GNUC_UNUSED,
                                  gpointer userData,
                                  GVariant **outArgs,
                                  GUnixFDList **outFDs G_GNUC_UNUSED,
                                  GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virDomain) domain = NULL;
    g_autofree gchar *path = NULL;
    const gchar *name;

    g_variant_get(inArgs, "(s)", &name);

    if (!virtDBusConnectOpen(connect, error))
        return;

    domain = virDomainLookupByName(connect->connection, name);
    if (!domain)
        return virtDBusUtilSetLastVirtError(error);

    path = virtDBusUtilBusPathForVirDomain(domain, connect->domainPath);

    *outArgs = g_variant_new("(o)", path);
}

static void
virtDBusConnectDomainLookupByUUID(GVariant *inArgs,
                                  GUnixFDList *inFDs G_GNUC_UNUSED,
                                  const gchar *objectPath G_GNUC_UNUSED,
                                  gpointer userData,
                                  GVariant **outArgs,
                                  GUnixFDList **outFDs G_GNUC_UNUSED,
                                  GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virDomain) domain = NULL;
    g_autofree gchar *path = NULL;
    const gchar *uuidstr;

    g_variant_get(inArgs, "(s)", &uuidstr);

    if (!virtDBusConnectOpen(connect, error))
        return;

    domain = virDomainLookupByUUIDString(connect->connection, uuidstr);
    if (!domain)
        return virtDBusUtilSetLastVirtError(error);

    path = virtDBusUtilBusPathForVirDomain(domain, connect->domainPath);

    *outArgs = g_variant_new("(o)", path);
}

static void
virtDBusConnectDomainRestoreFlags(GVariant *inArgs,
                                  GUnixFDList *inFDs G_GNUC_UNUSED,
                                  const gchar *objectPath G_GNUC_UNUSED,
                                  gpointer userData,
                                  GVariant **outArgs G_GNUC_UNUSED,
                                  GUnixFDList **outFDs G_GNUC_UNUSED,
                                  GError **error)
{
    virtDBusConnect *connect = userData;
    const gchar *from;
    const gchar *xml;
    guint flags;

    g_variant_get(inArgs, "(&s&su)", &from, &xml, &flags);
    if (g_str_equal(xml, ""))
        xml = NULL;

    if (!virtDBusConnectOpen(connect, error))
        return;

    if (virDomainRestoreFlags(connect->connection, from, xml, flags) < 0)
        virtDBusUtilSetLastVirtError(error);
}

static void
virtDBusConnectDomainSaveImageDefineXML(GVariant *inArgs,
                                        GUnixFDList *inFDs G_GNUC_UNUSED,
                                        const gchar *objectPath G_GNUC_UNUSED,
                                        gpointer userData,
                                        GVariant **outArgs G_GNUC_UNUSED,
                                        GUnixFDList **outFDs G_GNUC_UNUSED,
                                        GError **error)
{
    virtDBusConnect *connect = userData;
    const gchar *file;
    const gchar *xml;
    guint flags;

    g_variant_get(inArgs, "(&s&su)", &file, &xml, &flags);

    if (!virtDBusConnectOpen(connect, error))
        return;

    if (virDomainSaveImageDefineXML(connect->connection, file, xml, flags) < 0)
        virtDBusUtilSetLastVirtError(error);
}

static void
virtDBusConnectDomainSaveImageGetXMLDesc(GVariant *inArgs,
                                         GUnixFDList *inFDs G_GNUC_UNUSED,
                                         const gchar *objectPath G_GNUC_UNUSED,
                                         gpointer userData,
                                         GVariant **outArgs,
                                         GUnixFDList **outFDs G_GNUC_UNUSED,
                                         GError **error)
{
    virtDBusConnect *connect = userData;
    const gchar *file;
    guint flags;
    g_autofree gchar *xml = NULL;

    g_variant_get(inArgs, "(&su)", &file, &flags);

    if (!virtDBusConnectOpen(connect, error))
        return;

    xml = virDomainSaveImageGetXMLDesc(connect->connection, file, flags);
    if (!xml)
        return virtDBusUtilSetLastVirtError(error);

    *outArgs = g_variant_new("(s)", xml);
}

static void
virtDBusConnectFindStoragePoolSources(GVariant *inArgs,
                                      GUnixFDList *inFDs G_GNUC_UNUSED,
                                      const gchar *objectPath G_GNUC_UNUSED,
                                      gpointer userData,
                                      GVariant **outArgs,
                                      GUnixFDList **outFDs G_GNUC_UNUSED,
                                      GError **error)
{
    virtDBusConnect *connect = userData;
    const gchar *type;
    const gchar *srcSpec;
    guint flags;
    g_autofree gchar *ret = NULL;

    g_variant_get(inArgs, "(&s&su)", &type, &srcSpec, &flags);
    if (g_str_equal(srcSpec, ""))
        srcSpec = NULL;

    if (!virtDBusConnectOpen(connect, error))
        return;

    ret = virConnectFindStoragePoolSources(connect->connection, type, srcSpec, flags);
    if (!ret)
        return virtDBusUtilSetLastVirtError(error);

    *outArgs = g_variant_new("(s)", ret);
}

static void
virtDBusConnectGetAllDomainStats(GVariant *inArgs,
                                 GUnixFDList *inFDs G_GNUC_UNUSED,
                                 const gchar *objectPath G_GNUC_UNUSED,
                                 gpointer userData,
                                 GVariant **outArgs,
                                 GUnixFDList **outFDs G_GNUC_UNUSED,
                                 GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virDomainStatsRecordPtr) records = NULL;
    guint stats;
    gint nstats;
    guint flags;
    GVariant *gret;
    GVariantBuilder builder;

    g_variant_get(inArgs, "(uu)", &stats, &flags);

    if (!virtDBusConnectOpen(connect, error))
        return;

    nstats = virConnectGetAllDomainStats(connect->connection,
                                         stats, &records, flags);
    if (nstats < 0)
        return virtDBusUtilSetLastVirtError(error);

    g_variant_builder_init(&builder, G_VARIANT_TYPE("a(sa{sv})"));

    for (gint i = 0; i < nstats; i++) {
        const gchar *name;
        GVariant *grecords;

        g_variant_builder_open(&builder, G_VARIANT_TYPE("(sa{sv})"));
        name = virDomainGetName(records[i]->dom);
        grecords = virtDBusUtilTypedParamsToGVariant(records[i]->params,
                                                     records[i]->nparams);
        g_variant_builder_add(&builder, "s", name);
        g_variant_builder_add_value(&builder, grecords);
        g_variant_builder_close(&builder);
    }
    gret = g_variant_builder_end(&builder);

    *outArgs = g_variant_new_tuple(&gret, 1);
}

static void
virtDBusConnectGetCPUModelNames(GVariant *inArgs,
                                GUnixFDList *inFDs G_GNUC_UNUSED,
                                const gchar *objectPath G_GNUC_UNUSED,
                                gpointer userData,
                                GVariant **outArgs,
                                GUnixFDList **outFDs G_GNUC_UNUSED,
                                GError **error)
{
    virtDBusConnect *connect = userData;
    const gchar *arch;
    guint flags;
    g_autoptr(virtDBusCharArray) models = NULL;
    gint nmodels;
    GVariant *gret;
    GVariantBuilder builder;

    g_variant_get(inArgs, "(&su)", &arch, &flags);

    if (!virtDBusConnectOpen(connect, error))
        return;

    nmodels = virConnectGetCPUModelNames(connect->connection, arch, &models, flags);
    if (nmodels < 0)
        return virtDBusUtilSetLastVirtError(error);

    g_variant_builder_init(&builder, G_VARIANT_TYPE("as"));
    for (gint i = 0; i < nmodels; i++)
        g_variant_builder_add(&builder, "s", models[i]);
    gret = g_variant_builder_end(&builder);

    *outArgs = g_variant_new_tuple(&gret, 1);
}

static void
virtDBusConnectGetDomainCapabilities(GVariant *inArgs,
                                     GUnixFDList *inFDs G_GNUC_UNUSED,
                                     const gchar *objectPath G_GNUC_UNUSED,
                                     gpointer userData,
                                     GVariant **outArgs,
                                     GUnixFDList **outFDs G_GNUC_UNUSED,
                                     GError **error)
{
    virtDBusConnect *connect = userData;
    const gchar *emulatorbin;
    const gchar *arch;
    const gchar *machine;
    const gchar *virttype;
    guint flags;
    g_autofree gchar *domCapabilities = NULL;

    g_variant_get(inArgs, "(&s&s&s&su)", &emulatorbin, &arch, &machine,
                  &virttype, &flags);
    if (g_str_equal(emulatorbin, ""))
        emulatorbin = NULL;
    if (g_str_equal(arch, ""))
        arch = NULL;
    if (g_str_equal(machine, ""))
        machine = NULL;
    if (g_str_equal(virttype, ""))
        virttype = NULL;

    if (!virtDBusConnectOpen(connect, error))
        return;

    domCapabilities = virConnectGetDomainCapabilities(connect->connection,
                                                      emulatorbin, arch,
                                                      machine, virttype,
                                                      flags);
    if (!domCapabilities)
        return virtDBusUtilSetLastVirtError(error);

    *outArgs = g_variant_new("(s)", domCapabilities);
}

static void
virtDBusConnectGetSysinfo(GVariant *inArgs,
                          GUnixFDList *inFDs G_GNUC_UNUSED,
                          const gchar *objectPath G_GNUC_UNUSED,
                          gpointer userData,
                          GVariant **outArgs,
                          GUnixFDList **outFDs G_GNUC_UNUSED,
                          GError **error)

{
    virtDBusConnect *connect = userData;
    guint flags;
    g_autofree gchar *sysinfo = NULL;

    g_variant_get(inArgs, "(u)", &flags);

    if (!virtDBusConnectOpen(connect, error))
        return;

    sysinfo = virConnectGetSysinfo(connect->connection, flags);
    if (!sysinfo)
        return virtDBusUtilSetLastVirtError(error);

    *outArgs = g_variant_new("(s)", sysinfo);
}

static void
virtDBusConnectInterfaceChangeBegin(GVariant *inArgs,
                                    GUnixFDList *inFDs G_GNUC_UNUSED,
                                    const gchar *objectPath G_GNUC_UNUSED,
                                    gpointer userData,
                                    GVariant **outArgs G_GNUC_UNUSED,
                                    GUnixFDList **outFDs G_GNUC_UNUSED,
                                    GError **error)
{
    virtDBusConnect *connect = userData;
    guint flags;

    g_variant_get(inArgs, "(u)", &flags);

    if (!virtDBusConnectOpen(connect, error))
        return;

    if (virInterfaceChangeBegin(connect->connection, flags) < 0)
        virtDBusUtilSetLastVirtError(error);
}

static void
virtDBusConnectInterfaceChangeCommit(GVariant *inArgs,
                                     GUnixFDList *inFDs G_GNUC_UNUSED,
                                     const gchar *objectPath G_GNUC_UNUSED,
                                     gpointer userData,
                                     GVariant **outArgs G_GNUC_UNUSED,
                                     GUnixFDList **outFDs G_GNUC_UNUSED,
                                     GError **error)
{
    virtDBusConnect *connect = userData;
    guint flags;

    g_variant_get(inArgs, "(u)", &flags);

    if (!virtDBusConnectOpen(connect, error))
        return;

    if (virInterfaceChangeCommit(connect->connection, flags) < 0)
        virtDBusUtilSetLastVirtError(error);
}

static void
virtDBusConnectInterfaceChangeRollback(GVariant *inArgs,
                                       GUnixFDList *inFDs G_GNUC_UNUSED,
                                       const gchar *objectPath G_GNUC_UNUSED,
                                       gpointer userData,
                                       GVariant **outArgs G_GNUC_UNUSED,
                                       GUnixFDList **outFDs G_GNUC_UNUSED,
                                       GError **error)
{
    virtDBusConnect *connect = userData;
    guint flags;

    g_variant_get(inArgs, "(u)", &flags);

    if (!virtDBusConnectOpen(connect, error))
        return;

    if (virInterfaceChangeRollback(connect->connection, flags) < 0)
        virtDBusUtilSetLastVirtError(error);
}

static void
virtDBusConnectInterfaceDefineXML(GVariant *inArgs,
                                  GUnixFDList *inFDs G_GNUC_UNUSED,
                                  const gchar *objectPath G_GNUC_UNUSED,
                                  gpointer userData,
                                  GVariant **outArgs,
                                  GUnixFDList **outFDs G_GNUC_UNUSED,
                                  GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virInterface) interface = NULL;
    g_autofree gchar *path = NULL;
    const gchar *xml;
    guint flags;

    g_variant_get(inArgs, "(&su)", &xml, &flags);

    if (!virtDBusConnectOpen(connect, error))
        return;

    interface = virInterfaceDefineXML(connect->connection, xml, flags);
    if (!interface)
        return virtDBusUtilSetLastVirtError(error);

    path = virtDBusUtilBusPathForVirInterface(interface, connect->interfacePath);

    *outArgs = g_variant_new("(o)", path);
}

static void
virtDBusConnectInterfaceLookupByMAC(GVariant *inArgs,
                                    GUnixFDList *inFDs G_GNUC_UNUSED,
                                    const gchar *objectPath G_GNUC_UNUSED,
                                    gpointer userData,
                                    GVariant **outArgs,
                                    GUnixFDList **outFDs G_GNUC_UNUSED,
                                    GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virInterface) interface = NULL;
    g_autofree gchar *path = NULL;
    const gchar *mac;

    g_variant_get(inArgs, "(&s)", &mac);

    if (!virtDBusConnectOpen(connect, NULL))
        return;

    interface = virInterfaceLookupByMACString(connect->connection, mac);
    if (!interface)
        return virtDBusUtilSetLastVirtError(error);

    path = virtDBusUtilBusPathForVirInterface(interface, connect->interfacePath);

    *outArgs = g_variant_new("(o)", path);
}

static void
virtDBusConnectInterfaceLookupByName(GVariant *inArgs,
                                     GUnixFDList *inFDs G_GNUC_UNUSED,
                                     const gchar *objectPath G_GNUC_UNUSED,
                                     gpointer userData,
                                     GVariant **outArgs,
                                     GUnixFDList **outFDs G_GNUC_UNUSED,
                                     GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virInterface) interface = NULL;
    g_autofree gchar *path = NULL;
    const gchar *name;

    g_variant_get(inArgs, "(&s)", &name);

    if (!virtDBusConnectOpen(connect, NULL))
        return;

    interface = virInterfaceLookupByName(connect->connection, name);
    if (!interface)
        return virtDBusUtilSetLastVirtError(error);

    path = virtDBusUtilBusPathForVirInterface(interface, connect->interfacePath);

    *outArgs = g_variant_new("(o)", path);
}

static void
virtDBusConnectListDomains(GVariant *inArgs,
                           GUnixFDList *inFDs G_GNUC_UNUSED,
                           const gchar *objectPath G_GNUC_UNUSED,
                           gpointer userData,
                           GVariant **outArgs,
                           GUnixFDList **outFDs G_GNUC_UNUSED,
                           GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virDomainPtr) domains = NULL;
    guint flags;
    GVariantBuilder builder;
    GVariant *gdomains;

    g_variant_get(inArgs, "(u)", &flags);

    if (!virtDBusConnectOpen(connect, error))
        return;

    if (virConnectListAllDomains(connect->connection, &domains, flags) < 0)
        return virtDBusUtilSetLastVirtError(error);

    g_variant_builder_init(&builder, G_VARIANT_TYPE("ao"));

    for (gint i = 0; domains[i]; i++) {
        g_autofree gchar *path = NULL;
        path = virtDBusUtilBusPathForVirDomain(domains[i],
                                               connect->domainPath);

        g_variant_builder_add(&builder, "o", path);
    }

    gdomains = g_variant_builder_end(&builder);
    *outArgs = g_variant_new_tuple(&gdomains, 1);
}

static void
virtDBusConnectListInterfaces(GVariant *inArgs,
                              GUnixFDList *inFDs G_GNUC_UNUSED,
                              const gchar *objectPath G_GNUC_UNUSED,
                              gpointer userData,
                              GVariant **outArgs,
                              GUnixFDList **outFDs G_GNUC_UNUSED,
                              GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virInterfacePtr) interfaces = NULL;
    guint flags;
    GVariantBuilder builder;
    GVariant *ginterfaces;

    g_variant_get(inArgs, "(u)", &flags);

    if (!virtDBusConnectOpen(connect, error))
        return;

    if (virConnectListAllInterfaces(connect->connection, &interfaces, flags) < 0)
        return virtDBusUtilSetLastVirtError(error);

    g_variant_builder_init(&builder, G_VARIANT_TYPE("ao"));

    for (gint i = 0; interfaces[i]; i++) {
        g_autofree gchar *path = NULL;
        path = virtDBusUtilBusPathForVirInterface(interfaces[i],
                                                  connect->interfacePath);

        g_variant_builder_add(&builder, "o", path);
    }

    ginterfaces = g_variant_builder_end(&builder);
    *outArgs = g_variant_new_tuple(&ginterfaces, 1);
}

static void
virtDBusConnectListNetworks(GVariant *inArgs,
                            GUnixFDList *inFDs G_GNUC_UNUSED,
                            const gchar *objectPath G_GNUC_UNUSED,
                            gpointer userData,
                            GVariant **outArgs,
                            GUnixFDList **outFDs G_GNUC_UNUSED,
                            GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virNetworkPtr) networks = NULL;
    guint flags;
    GVariantBuilder builder;
    GVariant *gnetworks;

    g_variant_get(inArgs, "(u)", &flags);

    if (!virtDBusConnectOpen(connect, error))
        return;

    if (virConnectListAllNetworks(connect->connection, &networks, flags) < 0)
        return virtDBusUtilSetLastVirtError(error);

    g_variant_builder_init(&builder, G_VARIANT_TYPE("ao"));

    for (gint i = 0; networks[i]; i++) {
        g_autofree gchar *path = NULL;
        path = virtDBusUtilBusPathForVirNetwork(networks[i],
                                                connect->networkPath);

        g_variant_builder_add(&builder, "o", path);
    }

    gnetworks = g_variant_builder_end(&builder);
    *outArgs = g_variant_new_tuple(&gnetworks, 1);
}

static void
virtDBusConnectListNodeDevices(GVariant *inArgs,
                               GUnixFDList *inFDs G_GNUC_UNUSED,
                               const gchar *objectPath G_GNUC_UNUSED,
                               gpointer userData,
                               GVariant **outArgs,
                               GUnixFDList **outFDs G_GNUC_UNUSED,
                               GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virNodeDevicePtr) devs = NULL;
    guint flags;
    GVariantBuilder builder;
    GVariant *gdevs;

    g_variant_get(inArgs, "(u)", &flags);

    if (!virtDBusConnectOpen(connect, error))
        return;

    if (virConnectListAllNodeDevices(connect->connection, &devs, flags) < 0)
        return virtDBusUtilSetLastVirtError(error);

    g_variant_builder_init(&builder, G_VARIANT_TYPE("ao"));

    for (gint i = 0; devs[i]; i++) {
        g_autofree gchar *path = NULL;
        path = virtDBusUtilBusPathForVirNodeDevice(devs[i],
                                                   connect->nodeDevPath);

        g_variant_builder_add(&builder, "o", path);
    }

    gdevs = g_variant_builder_end(&builder);
    *outArgs = g_variant_new_tuple(&gdevs, 1);
}

static void
virtDBusConnectListNWFilters(GVariant *inArgs,
                             GUnixFDList *inFDs G_GNUC_UNUSED,
                             const gchar *objectPath G_GNUC_UNUSED,
                             gpointer userData,
                             GVariant **outArgs,
                             GUnixFDList **outFDs G_GNUC_UNUSED,
                             GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virNWFilterPtr) nwfilters = NULL;
    guint flags;
    GVariantBuilder builder;
    GVariant *gnwfilters;

    g_variant_get(inArgs, "(u)", &flags);

    if (!virtDBusConnectOpen(connect, error))
        return;

    if (virConnectListAllNWFilters(connect->connection, &nwfilters, flags) < 0)
        return virtDBusUtilSetLastVirtError(error);

    g_variant_builder_init(&builder, G_VARIANT_TYPE("ao"));

    for (gint i = 0; nwfilters[i]; i++) {
        g_autofree gchar *path = NULL;
        path = virtDBusUtilBusPathForVirNWFilter(nwfilters[i],
                                                 connect->nwfilterPath);

        g_variant_builder_add(&builder, "o", path);
    }

    gnwfilters = g_variant_builder_end(&builder);
    *outArgs = g_variant_new_tuple(&gnwfilters, 1);
}

static void
virtDBusConnectListSecrets(GVariant *inArgs,
                           GUnixFDList *inFDs G_GNUC_UNUSED,
                           const gchar *objectPath G_GNUC_UNUSED,
                           gpointer userData,
                           GVariant **outArgs,
                           GUnixFDList **outFDs G_GNUC_UNUSED,
                           GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virSecretPtr) secrets = NULL;
    guint flags;
    GVariantBuilder builder;
    GVariant *gsecrets;

    g_variant_get(inArgs, "(u)", &flags);

    if (!virtDBusConnectOpen(connect, error))
        return;

    if (virConnectListAllSecrets(connect->connection, &secrets, flags) < 0)
        return virtDBusUtilSetLastVirtError(error);

    g_variant_builder_init(&builder, G_VARIANT_TYPE("ao"));

    for (gint i = 0; secrets[i]; i++) {
        g_autofree gchar *path = NULL;
        path = virtDBusUtilBusPathForVirSecret(secrets[i], connect->secretPath);

        g_variant_builder_add(&builder, "o", path);
    }

    gsecrets = g_variant_builder_end(&builder);
    *outArgs = g_variant_new_tuple(&gsecrets, 1);
}

static void
virtDBusConnectListStoragePools(GVariant *inArgs,
                                GUnixFDList *inFDs G_GNUC_UNUSED,
                                const gchar *objectPath G_GNUC_UNUSED,
                                gpointer userData,
                                GVariant **outArgs,
                                GUnixFDList **outFDs G_GNUC_UNUSED,
                                GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virStoragePoolPtr) storagePools = NULL;
    guint flags;
    GVariantBuilder builder;
    GVariant *gstoragePools;

    g_variant_get(inArgs, "(u)", &flags);

    if (!virtDBusConnectOpen(connect, error))
        return;

    if (virConnectListAllStoragePools(connect->connection, &storagePools,
                                      flags) < 0) {
        return virtDBusUtilSetLastVirtError(error);
    }

    g_variant_builder_init(&builder, G_VARIANT_TYPE("ao"));

    for (gint i = 0; storagePools[i]; i++) {
        g_autofree gchar *path = NULL;
        path = virtDBusUtilBusPathForVirStoragePool(storagePools[i],
                                                    connect->storagePoolPath);

        g_variant_builder_add(&builder, "o", path);
    }

    gstoragePools = g_variant_builder_end(&builder);
    *outArgs = g_variant_new_tuple(&gstoragePools, 1);
}

static void
virtDBusConnectNetworkCreateXML(GVariant *inArgs,
                                GUnixFDList *inFDs G_GNUC_UNUSED,
                                const gchar *objectPath G_GNUC_UNUSED,
                                gpointer userData,
                                GVariant **outArgs,
                                GUnixFDList **outFDs G_GNUC_UNUSED,
                                GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virNetwork) network = NULL;
    g_autofree gchar *path = NULL;
    const gchar *xml;

    g_variant_get(inArgs, "(&s)", &xml);

    if (!virtDBusConnectOpen(connect, error))
        return;

    network = virNetworkCreateXML(connect->connection, xml);
    if (!network)
        return virtDBusUtilSetLastVirtError(error);

    path = virtDBusUtilBusPathForVirNetwork(network, connect->domainPath);

    *outArgs = g_variant_new("(o)", path);
}

static void
virtDBusConnectNetworkDefineXML(GVariant *inArgs,
                                GUnixFDList *inFDs G_GNUC_UNUSED,
                                const gchar *objectPath G_GNUC_UNUSED,
                                gpointer userData,
                                GVariant **outArgs,
                                GUnixFDList **outFDs G_GNUC_UNUSED,
                                GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virNetwork) network = NULL;
    g_autofree gchar *path = NULL;
    const gchar *xml;

    g_variant_get(inArgs, "(&s)", &xml);

    if (!virtDBusConnectOpen(connect, error))
        return;

    network = virNetworkDefineXML(connect->connection, xml);
    if (!network)
        return virtDBusUtilSetLastVirtError(error);

    path = virtDBusUtilBusPathForVirNetwork(network, connect->networkPath);

    *outArgs = g_variant_new("(o)", path);
}

static void
virtDBusConnectNetworkLookupByName(GVariant *inArgs,
                                   GUnixFDList *inFDs G_GNUC_UNUSED,
                                   const gchar *objectPath G_GNUC_UNUSED,
                                   gpointer userData,
                                   GVariant **outArgs,
                                   GUnixFDList **outFDs G_GNUC_UNUSED,
                                   GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virNetwork) network = NULL;
    g_autofree gchar *path = NULL;
    const gchar *name;

    g_variant_get(inArgs, "(s)", &name);

    if (!virtDBusConnectOpen(connect, error))
        return;

    network = virNetworkLookupByName(connect->connection, name);
    if (!network)
        return virtDBusUtilSetLastVirtError(error);

    path = virtDBusUtilBusPathForVirNetwork(network, connect->networkPath);

    *outArgs = g_variant_new("(o)", path);
}

static void
virtDBusConnectNetworkLookupByUUID(GVariant *inArgs,
                                   GUnixFDList *inFDs G_GNUC_UNUSED,
                                   const gchar *objectPath G_GNUC_UNUSED,
                                   gpointer userData,
                                   GVariant **outArgs,
                                   GUnixFDList **outFDs G_GNUC_UNUSED,
                                   GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virNetwork) network = NULL;
    g_autofree gchar *path = NULL;
    const gchar *uuidstr;

    g_variant_get(inArgs, "(&s)", &uuidstr);

    if (!virtDBusConnectOpen(connect, error))
        return;

    network = virNetworkLookupByUUIDString(connect->connection, uuidstr);
    if (!network)
        return virtDBusUtilSetLastVirtError(error);

    path = virtDBusUtilBusPathForVirNetwork(network, connect->networkPath);

    *outArgs = g_variant_new("(o)", path);
}

static void
virtDBusConnectNodeDeviceCreateXML(GVariant *inArgs,
                                   GUnixFDList *inFDs G_GNUC_UNUSED,
                                   const gchar *objectPath G_GNUC_UNUSED,
                                   gpointer userData,
                                   GVariant **outArgs,
                                   GUnixFDList **outFDs G_GNUC_UNUSED,
                                   GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virNodeDevice) dev = NULL;
    g_autofree gchar *path = NULL;
    gchar *xml;
    guint flags;

    g_variant_get(inArgs, "(&su)", &xml, &flags);

    if (!virtDBusConnectOpen(connect, error))
        return;

    dev = virNodeDeviceCreateXML(connect->connection, xml, flags);
    if (!dev)
        return virtDBusUtilSetLastVirtError(error);

    path = virtDBusUtilBusPathForVirNodeDevice(dev, connect->nodeDevPath);

    *outArgs = g_variant_new("(o)", path);
}

static void
virtDBusConnectNodeDeviceLookupByName(GVariant *inArgs,
                                      GUnixFDList *inFDs G_GNUC_UNUSED,
                                      const gchar *objectPath G_GNUC_UNUSED,
                                      gpointer userData,
                                      GVariant **outArgs,
                                      GUnixFDList **outFDs G_GNUC_UNUSED,
                                      GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virNodeDevice) dev = NULL;
    g_autofree gchar *path = NULL;
    const gchar *name;

    g_variant_get(inArgs, "(&s)", &name);

    if (!virtDBusConnectOpen(connect, error))
        return;

    dev = virNodeDeviceLookupByName(connect->connection, name);
    if (!dev)
        return virtDBusUtilSetLastVirtError(error);

    path = virtDBusUtilBusPathForVirNodeDevice(dev, connect->nodeDevPath);

    *outArgs = g_variant_new("(o)", path);
}

static void
virtDBusConnectNodeDeviceLookupSCSIHostByWWN(GVariant *inArgs,
                                             GUnixFDList *inFDs G_GNUC_UNUSED,
                                             const gchar *objectPath G_GNUC_UNUSED,
                                             gpointer userData,
                                             GVariant **outArgs,
                                             GUnixFDList **outFDs G_GNUC_UNUSED,
                                             GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virNodeDevice) dev = NULL;
    g_autofree gchar *path = NULL;
    const gchar *wwnn;
    const gchar *wwpn;
    guint flags;

    g_variant_get(inArgs, "(&s&su)", &wwnn, &wwpn, &flags);

    if (!virtDBusConnectOpen(connect, error))
        return;

    dev = virNodeDeviceLookupSCSIHostByWWN(connect->connection, wwnn, wwpn,
                                           flags);
    if (!dev)
        return virtDBusUtilSetLastVirtError(error);

    path = virtDBusUtilBusPathForVirNodeDevice(dev, connect->nodeDevPath);

    *outArgs = g_variant_new("(o)", path);
}

static void
virtDBusConnectNWFilterDefineXML(GVariant *inArgs,
                                 GUnixFDList *inFDs G_GNUC_UNUSED,
                                 const gchar *objectPath G_GNUC_UNUSED,
                                 gpointer userData,
                                 GVariant **outArgs,
                                 GUnixFDList **outFDs G_GNUC_UNUSED,
                                 GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virNWFilter) nwfilter = NULL;
    g_autofree gchar *path = NULL;
    const gchar *xml;

    g_variant_get(inArgs, "(&s)", &xml);

    if (!virtDBusConnectOpen(connect, error))
        return;

    nwfilter = virNWFilterDefineXML(connect->connection, xml);
    if (!nwfilter)
        return virtDBusUtilSetLastVirtError(error);

    path = virtDBusUtilBusPathForVirNWFilter(nwfilter, connect->nwfilterPath);

    *outArgs = g_variant_new("(o)", path);
}

static void
virtDBusConnectNWFilterLookupByName(GVariant *inArgs,
                                    GUnixFDList *inFDs G_GNUC_UNUSED,
                                    const gchar *objectPath G_GNUC_UNUSED,
                                    gpointer userData,
                                    GVariant **outArgs,
                                    GUnixFDList **outFDs G_GNUC_UNUSED,
                                    GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virNWFilter) nwfilter = NULL;
    g_autofree gchar *path = NULL;
    const gchar *name;

    g_variant_get(inArgs, "(s)", &name);

    if (!virtDBusConnectOpen(connect, error))
        return;

    nwfilter = virNWFilterLookupByName(connect->connection, name);
    if (!nwfilter)
        return virtDBusUtilSetLastVirtError(error);

    path = virtDBusUtilBusPathForVirNWFilter(nwfilter, connect->nwfilterPath);

    *outArgs = g_variant_new("(o)", path);
}

static void
virtDBusConnectNWFilterLookupByUUID(GVariant *inArgs,
                                    GUnixFDList *inFDs G_GNUC_UNUSED,
                                    const gchar *objectPath G_GNUC_UNUSED,
                                    gpointer userData,
                                    GVariant **outArgs,
                                    GUnixFDList **outFDs G_GNUC_UNUSED,
                                    GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virNWFilter) nwfilter = NULL;
    g_autofree gchar *path = NULL;
    const gchar *uuidstr;

    g_variant_get(inArgs, "(&s)", &uuidstr);

    if (!virtDBusConnectOpen(connect, error))
        return;

    nwfilter = virNWFilterLookupByUUIDString(connect->connection, uuidstr);
    if (!nwfilter)
        return virtDBusUtilSetLastVirtError(error);

    path = virtDBusUtilBusPathForVirNWFilter(nwfilter, connect->nwfilterPath);

    *outArgs = g_variant_new("(o)", path);
}

static void
virtDBusConnectNodeGetCPUMap(GVariant *inArgs,
                             GUnixFDList *inFDs G_GNUC_UNUSED,
                             const gchar *objectPath G_GNUC_UNUSED,
                             gpointer userData,
                             GVariant **outArgs,
                             GUnixFDList **outFDs G_GNUC_UNUSED,
                             GError **error)
{
    virtDBusConnect *connect = userData;
    g_autofree guchar *cpumap = NULL;
    guint online = 0;
    guint flags;
    gint ret;
    GVariant *gret;
    GVariantBuilder builder;

    g_variant_get(inArgs, "(u)", &flags);

    if (!virtDBusConnectOpen(connect, error))
        return;

    ret = virNodeGetCPUMap(connect->connection, &cpumap, &online, flags);
    if (ret < 0)
        return virtDBusUtilSetLastVirtError(error);

    g_variant_builder_init(&builder, G_VARIANT_TYPE("ab"));
    for (gint i = 0; i < ret; i++)
        g_variant_builder_add(&builder, "b", VIR_CPU_USED(cpumap, i));
    gret = g_variant_builder_end(&builder);

    *outArgs = g_variant_new_tuple(&gret, 1);
}

static void
virtDBusConnectNodeGetCPUStats(GVariant *inArgs,
                               GUnixFDList *inFDs G_GNUC_UNUSED,
                               const gchar *objectPath G_GNUC_UNUSED,
                               gpointer userData,
                               GVariant **outArgs,
                               GUnixFDList **outFDs G_GNUC_UNUSED,
                               GError **error)
{
    virtDBusConnect *connect = userData;
    gint cpuNum;
    guint flags;
    g_autofree virNodeCPUStatsPtr stats = NULL;
    gint count = 0;
    gint ret;
    GVariant *gret;
    GVariantBuilder builder;

    g_variant_get(inArgs, "(iu)", &cpuNum, &flags);

    if (!virtDBusConnectOpen(connect, error))
        return;

    ret = virNodeGetCPUStats(connect->connection, cpuNum, NULL, &count, flags);
    if (ret < 0)
        return virtDBusUtilSetLastVirtError(error);

    if (count != 0) {
        stats = g_new0(virNodeCPUStats, count);
        if (virNodeGetCPUStats(connect->connection, cpuNum, stats,
                               &count, flags) < 0) {
            return virtDBusUtilSetLastVirtError(error);
        }
    }

    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{st}"));
    for (gint i = 0; i < count; i++)
        g_variant_builder_add(&builder, "{st}", stats[i].field, stats[i].value);
    gret = g_variant_builder_end(&builder);

    *outArgs = g_variant_new_tuple(&gret, 1);
}

static void
virtDBusConnectNodeGetFreeMemory(GVariant *inArgs G_GNUC_UNUSED,
                                 GUnixFDList *inFDs G_GNUC_UNUSED,
                                 const gchar *objectPath G_GNUC_UNUSED,
                                 gpointer userData,
                                 GVariant **outArgs,
                                 GUnixFDList **outFDs G_GNUC_UNUSED,
                                 GError **error)

{
    virtDBusConnect *connect = userData;
    guint64 freemem;

    if (!virtDBusConnectOpen(connect, error))
        return;

    freemem = virNodeGetFreeMemory(connect->connection);
    if (freemem == 0)
        return virtDBusUtilSetLastVirtError(error);

    *outArgs = g_variant_new("(t)", freemem);
}

static void
virtDBusConnectNodeGetMemoryParameters(GVariant *inArgs,
                                       GUnixFDList *inFDs G_GNUC_UNUSED,
                                       const gchar *objectPath G_GNUC_UNUSED,
                                       gpointer userData,
                                       GVariant **outArgs,
                                       GUnixFDList **outFDs G_GNUC_UNUSED,
                                       GError **error)
{
    virtDBusConnect *connect = userData;
    g_auto(virtDBusUtilTypedParams) params = { 0 };
    guint flags;
    gint ret;
    GVariant *grecords;

    g_variant_get(inArgs, "(u)", &flags);

    if (!virtDBusConnectOpen(connect, error))
        return;

    ret = virNodeGetMemoryParameters(connect->connection, NULL,
                                     &params.nparams, flags);
    if (ret < 0)
        return virtDBusUtilSetLastVirtError(error);

    if (params.nparams != 0) {
        params.params = g_new0(virTypedParameter, params.nparams);
        if (virNodeGetMemoryParameters(connect->connection, params.params,
                                       &params.nparams, flags) < 0) {
            return virtDBusUtilSetLastVirtError(error);
        }
    }

    grecords = virtDBusUtilTypedParamsToGVariant(params.params, params.nparams);

    *outArgs = g_variant_new_tuple(&grecords, 1);
}

static void
virtDBusConnectNodeGetMemoryStats(GVariant *inArgs,
                                  GUnixFDList *inFDs G_GNUC_UNUSED,
                                  const gchar *objectPath G_GNUC_UNUSED,
                                  gpointer userData,
                                  GVariant **outArgs,
                                  GUnixFDList **outFDs G_GNUC_UNUSED,
                                  GError **error)
{
    virtDBusConnect *connect = userData;
    g_autofree virNodeMemoryStatsPtr params = NULL;
    gint nparams = 0;
    gint cellNum;
    guint flags;
    gint ret;
    GVariantBuilder builder;
    GVariant *res;

    g_variant_get(inArgs, "(iu)", &cellNum, &flags);

    if (!virtDBusConnectOpen(connect, error))
        return;

    ret = virNodeGetMemoryStats(connect->connection, cellNum, NULL,
                                &nparams, flags);
    if (ret < 0)
        return virtDBusUtilSetLastVirtError(error);

    if (nparams != 0) {
        params = g_new0(virNodeMemoryStats, nparams);
        if (virNodeGetMemoryStats(connect->connection, cellNum, params,
                                  &nparams, flags) < 0) {
            return virtDBusUtilSetLastVirtError(error);
        }
    }

    g_variant_builder_init(&builder, G_VARIANT_TYPE("a{st}"));
    for (gint i = 0; i < nparams; i++)
        g_variant_builder_add(&builder, "{st}", params[i].field, params[i].value);
    res = g_variant_builder_end(&builder);

    *outArgs = g_variant_new_tuple(&res, 1);
}

static void
virtDBusConnectNodeGetSecurityModel(GVariant *inArgs G_GNUC_UNUSED,
                                    GUnixFDList *inFDs G_GNUC_UNUSED,
                                    const gchar *objectPath G_GNUC_UNUSED,
                                    gpointer userData,
                                    GVariant **outArgs,
                                    GUnixFDList **outFDs G_GNUC_UNUSED,
                                    GError **error)

{
    virtDBusConnect *connect = userData;
    virSecurityModel secmodel;

    if (!virtDBusConnectOpen(connect, error))
        return;

    if (virNodeGetSecurityModel(connect->connection, &secmodel) < 0)
        return virtDBusUtilSetLastVirtError(error);

    *outArgs = g_variant_new("((ss))", secmodel.model, secmodel.doi);
}

static void
virtDBusConnectNodeSetMemoryParameters(GVariant *inArgs,
                                       GUnixFDList *inFDs G_GNUC_UNUSED,
                                       const gchar *objectPath G_GNUC_UNUSED,
                                       gpointer userData,
                                       GVariant **outArgs G_GNUC_UNUSED,
                                       GUnixFDList **outFDs G_GNUC_UNUSED,
                                       GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(GVariantIter) iter = NULL;
    g_auto(virtDBusUtilTypedParams) params = { 0 };
    guint flags;

    g_variant_get(inArgs, "(a{sv}u)", &iter, &flags);

    if (!virtDBusUtilGVariantToTypedParams(iter, &params.params,
                                           &params.nparams, error)) {
        return;
    }

    if (!virtDBusConnectOpen(connect, error))
        return;

    if (virNodeSetMemoryParameters(connect->connection, params.params,
                                   params.nparams, flags) < 0) {
        virtDBusUtilSetLastVirtError(error);
    }
}

static void
virtDBusConnectSecretDefineXML(GVariant *inArgs,
                               GUnixFDList *inFDs G_GNUC_UNUSED,
                               const gchar *objectPath G_GNUC_UNUSED,
                               gpointer userData,
                               GVariant **outArgs,
                               GUnixFDList **outFDs G_GNUC_UNUSED,
                               GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virSecret) secret = NULL;
    g_autofree gchar *path = NULL;
    const gchar *xml;
    guint flags;

    g_variant_get(inArgs, "(&su)", &xml, &flags);

    if (!virtDBusConnectOpen(connect, error))
        return;

    secret = virSecretDefineXML(connect->connection, xml, flags);
    if (!secret)
        return virtDBusUtilSetLastVirtError(error);

    path = virtDBusUtilBusPathForVirSecret(secret, connect->secretPath);

    *outArgs = g_variant_new("(o)", path);
}

static void
virtDBusConnectSecretLookupByUUID(GVariant *inArgs,
                                  GUnixFDList *inFDs G_GNUC_UNUSED,
                                  const gchar *objectPath G_GNUC_UNUSED,
                                  gpointer userData,
                                  GVariant **outArgs,
                                  GUnixFDList **outFDs G_GNUC_UNUSED,
                                  GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virSecret) secret = NULL;
    g_autofree gchar *path = NULL;
    const gchar *uuidstr;

    g_variant_get(inArgs, "(s)", &uuidstr);

    if (!virtDBusConnectOpen(connect, error))
        return;

    secret = virSecretLookupByUUIDString(connect->connection, uuidstr);
    if (!secret)
        return virtDBusUtilSetLastVirtError(error);

    path = virtDBusUtilBusPathForVirSecret(secret, connect->secretPath);

    *outArgs = g_variant_new("(o)", path);
}

static void
virtDBusConnectSecretLookupByUsage(GVariant *inArgs,
                                   GUnixFDList *inFDs G_GNUC_UNUSED,
                                   const gchar *objectPath G_GNUC_UNUSED,
                                   gpointer userData,
                                   GVariant **outArgs,
                                   GUnixFDList **outFDs G_GNUC_UNUSED,
                                   GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virSecret) secret = NULL;
    g_autofree gchar *path = NULL;
    gint usageType;
    const gchar *usageID;

    g_variant_get(inArgs, "(i&s)", &usageType, &usageID);

    if (!virtDBusConnectOpen(connect, error))
        return;

    secret = virSecretLookupByUsage(connect->connection, usageType, usageID);
    if (!secret)
        return virtDBusUtilSetLastVirtError(error);

    path = virtDBusUtilBusPathForVirSecret(secret, connect->secretPath);

    *outArgs = g_variant_new("(o)", path);
}

static void
virtDBusConnectStoragePoolCreateXML(GVariant *inArgs,
                                    GUnixFDList *inFDs G_GNUC_UNUSED,
                                    const gchar *objectPath G_GNUC_UNUSED,
                                    gpointer userData,
                                    GVariant **outArgs,
                                    GUnixFDList **outFDs G_GNUC_UNUSED,
                                    GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virStoragePool) storagePool = NULL;
    g_autofree gchar *path = NULL;
    gchar *xml;
    guint flags;

    g_variant_get(inArgs, "(&su)", &xml, &flags);

    if (!virtDBusConnectOpen(connect, error))
        return;

    storagePool = virStoragePoolCreateXML(connect->connection, xml, flags);
    if (!storagePool)
        return virtDBusUtilSetLastVirtError(error);

    path = virtDBusUtilBusPathForVirStoragePool(storagePool,
                                                connect->storagePoolPath);

    *outArgs = g_variant_new("(o)", path);
}

static void
virtDBusConnectStoragePoolDefineXML(GVariant *inArgs,
                                    GUnixFDList *inFDs G_GNUC_UNUSED,
                                    const gchar *objectPath G_GNUC_UNUSED,
                                    gpointer userData,
                                    GVariant **outArgs,
                                    GUnixFDList **outFDs G_GNUC_UNUSED,
                                    GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virStoragePool) storagePool = NULL;
    g_autofree gchar *path = NULL;
    gchar *xml;
    guint flags;

    g_variant_get(inArgs, "(&su)", &xml, &flags);

    if (!virtDBusConnectOpen(connect, error))
        return;

    storagePool = virStoragePoolDefineXML(connect->connection, xml, flags);
    if (!storagePool)
        return virtDBusUtilSetLastVirtError(error);

    path = virtDBusUtilBusPathForVirStoragePool(storagePool,
                                                connect->storagePoolPath);

    *outArgs = g_variant_new("(o)", path);
}

static void
virtDBusConnectStoragePoolLookupByName(GVariant *inArgs,
                                       GUnixFDList *inFDs G_GNUC_UNUSED,
                                       const gchar *objectPath G_GNUC_UNUSED,
                                       gpointer userData,
                                       GVariant **outArgs,
                                       GUnixFDList **outFDs G_GNUC_UNUSED,
                                       GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virStoragePool) storagePool = NULL;
    g_autofree gchar *path = NULL;
    const gchar *name;

    g_variant_get(inArgs, "(s)", &name);

    if (!virtDBusConnectOpen(connect, error))
        return;

    storagePool = virStoragePoolLookupByName(connect->connection, name);
    if (!storagePool)
        return virtDBusUtilSetLastVirtError(error);

    path = virtDBusUtilBusPathForVirStoragePool(storagePool,
                                                connect->storagePoolPath);

    *outArgs = g_variant_new("(o)", path);
}

static void
virtDBusConnectStoragePoolLookupByUUID(GVariant *inArgs,
                                       GUnixFDList *inFDs G_GNUC_UNUSED,
                                       const gchar *objectPath G_GNUC_UNUSED,
                                       gpointer userData,
                                       GVariant **outArgs,
                                       GUnixFDList **outFDs G_GNUC_UNUSED,
                                       GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virStoragePool) storagePool = NULL;
    g_autofree gchar *path = NULL;
    const gchar *uuidstr;

    g_variant_get(inArgs, "(s)", &uuidstr);

    if (!virtDBusConnectOpen(connect, error))
        return;

    storagePool = virStoragePoolLookupByUUIDString(connect->connection,
                                                   uuidstr);
    if (!storagePool)
        return virtDBusUtilSetLastVirtError(error);

    path = virtDBusUtilBusPathForVirStoragePool(storagePool,
                                                connect->storagePoolPath);

    *outArgs = g_variant_new("(o)", path);
}

static void
virtDBusConnectStorageVolLookupByKey(GVariant *inArgs,
                                     GUnixFDList *inFDs G_GNUC_UNUSED,
                                     const gchar *objectPath G_GNUC_UNUSED,
                                     gpointer userData,
                                     GVariant **outArgs,
                                     GUnixFDList **outFDs G_GNUC_UNUSED,
                                     GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virStorageVol) storageVol = NULL;
    g_autofree gchar *path = NULL;
    const gchar *key;

    g_variant_get(inArgs, "(&s)", &key);

    if (!virtDBusConnectOpen(connect, error))
        return;

    storageVol = virStorageVolLookupByKey(connect->connection, key);
    if (!storageVol)
        return virtDBusUtilSetLastVirtError(error);

    path = virtDBusUtilBusPathForVirStorageVol(storageVol,
                                               connect->storageVolPath);

    *outArgs = g_variant_new("(o)", path);
}

static void
virtDBusConnectStorageVolLookupByPath(GVariant *inArgs,
                                      GUnixFDList *inFDs G_GNUC_UNUSED,
                                      const gchar *objectPath G_GNUC_UNUSED,
                                      gpointer userData,
                                      GVariant **outArgs,
                                      GUnixFDList **outFDs G_GNUC_UNUSED,
                                      GError **error)
{
    virtDBusConnect *connect = userData;
    g_autoptr(virStorageVol) storageVol = NULL;
    const gchar *inPath;
    g_autofree gchar *path = NULL;

    g_variant_get(inArgs, "(&s)", &inPath);

    if (!virtDBusConnectOpen(connect, error))
        return;

    storageVol = virStorageVolLookupByPath(connect->connection, inPath);
    if (!storageVol)
        return virtDBusUtilSetLastVirtError(error);

    path = virtDBusUtilBusPathForVirStorageVol(storageVol,
                                               connect->storageVolPath);

    *outArgs = g_variant_new("(o)", path);
}

static virtDBusGDBusPropertyTable virtDBusConnectPropertyTable[] = {
    { "Encrypted", virtDBusConnectGetEncrypted, NULL },
    { "Hostname", virtDBusConnectGetHostname, NULL },
    { "LibVersion", virtDBusConnectGetLibVersion, NULL },
    { "Secure", virtDBusConnectGetSecure, NULL },
    { "Version", virtDBusConnectGetVersion, NULL },
    { 0 }
};

static virtDBusGDBusMethodTable virtDBusConnectMethodTable[] = {
    { "BaselineCPU", virtDBusConnectBaselineCPU },
    { "CompareCPU", virtDBusConnectCompareCPU },
    { "DomainCreateXML", virtDBusConnectDomainCreateXML },
    { "DomainCreateXMLWithFiles", virtDBusConnectDomainCreateXMLWithFiles },
    { "DomainDefineXML", virtDBusConnectDomainDefineXML },
    { "DomainLookupByID", virtDBusConnectDomainLookupByID },
    { "DomainLookupByName", virtDBusConnectDomainLookupByName },
    { "DomainLookupByUUID", virtDBusConnectDomainLookupByUUID },
    { "DomainRestore", virtDBusConnectDomainRestoreFlags },
    { "DomainSaveImageDefineXML", virtDBusConnectDomainSaveImageDefineXML },
    { "DomainSaveImageGetXMLDesc", virtDBusConnectDomainSaveImageGetXMLDesc },
    { "FindStoragePoolSources", virtDBusConnectFindStoragePoolSources },
    { "GetAllDomainStats", virtDBusConnectGetAllDomainStats },
    { "GetCapabilities", virtDBusConnectGetCapabilities },
    { "GetCPUModelNames", virtDBusConnectGetCPUModelNames },
    { "GetDomainCapabilities", virtDBusConnectGetDomainCapabilities },
    { "GetSysinfo", virtDBusConnectGetSysinfo },
    { "InterfaceChangeBegin", virtDBusConnectInterfaceChangeBegin },
    { "InterfaceChangeCommit", virtDBusConnectInterfaceChangeCommit },
    { "InterfaceChangeRollback", virtDBusConnectInterfaceChangeRollback },
    { "InterfaceDefineXML", virtDBusConnectInterfaceDefineXML },
    { "InterfaceLookupByMAC", virtDBusConnectInterfaceLookupByMAC },
    { "InterfaceLookupByName", virtDBusConnectInterfaceLookupByName },
    { "ListDomains", virtDBusConnectListDomains },
    { "ListInterfaces", virtDBusConnectListInterfaces },
    { "ListNetworks", virtDBusConnectListNetworks },
    { "ListNodeDevices", virtDBusConnectListNodeDevices },
    { "ListNWFilters", virtDBusConnectListNWFilters },
    { "ListSecrets", virtDBusConnectListSecrets },
    { "ListStoragePools", virtDBusConnectListStoragePools },
    { "NetworkCreateXML", virtDBusConnectNetworkCreateXML },
    { "NetworkDefineXML", virtDBusConnectNetworkDefineXML },
    { "NetworkLookupByName", virtDBusConnectNetworkLookupByName },
    { "NetworkLookupByUUID", virtDBusConnectNetworkLookupByUUID },
    { "NodeDeviceCreateXML", virtDBusConnectNodeDeviceCreateXML },
    { "NodeDeviceLookupByName", virtDBusConnectNodeDeviceLookupByName },
    { "NodeDeviceLookupSCSIHostByWWN", virtDBusConnectNodeDeviceLookupSCSIHostByWWN },
    { "NWFilterDefineXML", virtDBusConnectNWFilterDefineXML },
    { "NWFilterLookupByName", virtDBusConnectNWFilterLookupByName },
    { "NWFilterLookupByUUID", virtDBusConnectNWFilterLookupByUUID },
    { "NodeGetCPUMap", virtDBusConnectNodeGetCPUMap },
    { "NodeGetCPUStats", virtDBusConnectNodeGetCPUStats },
    { "NodeGetFreeMemory", virtDBusConnectNodeGetFreeMemory },
    { "NodeGetMemoryParameters", virtDBusConnectNodeGetMemoryParameters },
    { "NodeGetMemoryStats", virtDBusConnectNodeGetMemoryStats },
    { "NodeGetSecurityModel", virtDBusConnectNodeGetSecurityModel },
    { "NodeSetMemoryParameters", virtDBusConnectNodeSetMemoryParameters },
    { "SecretDefineXML", virtDBusConnectSecretDefineXML },
    { "SecretLookupByUUID", virtDBusConnectSecretLookupByUUID },
    { "SecretLookupByUsage", virtDBusConnectSecretLookupByUsage },
    { "StoragePoolCreateXML", virtDBusConnectStoragePoolCreateXML },
    { "StoragePoolDefineXML", virtDBusConnectStoragePoolDefineXML },
    { "StoragePoolLookupByName", virtDBusConnectStoragePoolLookupByName },
    { "StoragePoolLookupByUUID", virtDBusConnectStoragePoolLookupByUUID },
    { "StorageVolLookupByKey", virtDBusConnectStorageVolLookupByKey },
    { "StorageVolLookupByPath", virtDBusConnectStorageVolLookupByPath },
    { 0 }
};

static GDBusInterfaceInfo *interfaceInfo = NULL;

void
virtDBusConnectFree(virtDBusConnect *connect)
{
    if (connect->connection)
        virtDBusConnectClose(connect, TRUE);

    g_free(connect->domainPath);
    g_free(connect->domainSnapshotPath);
    g_free(connect->interfacePath);
    g_free(connect->networkPath);
    g_free(connect->nodeDevPath);
    g_free(connect->nwfilterPath);
    g_free(connect->secretPath);
    g_free(connect->storagePoolPath);
    g_free(connect->storageVolPath);
    g_free(connect);
}

void
virtDBusConnectNew(virtDBusConnect **connectp,
                   GDBusConnection *bus,
                   const gchar *uri,
                   const gchar *connectPath,
                   GError **error)
{
    g_autoptr(virtDBusConnect) connect = NULL;

    if (!interfaceInfo) {
        interfaceInfo = virtDBusGDBusLoadIntrospectData(VIRT_DBUS_CONNECT_INTERFACE,
                                                        error);
        if (!interfaceInfo)
            return;
    }

    connect = g_new0(virtDBusConnect, 1);

    g_mutex_init(&connect->lock);

    for (gint i = 0; i < VIR_DOMAIN_EVENT_ID_LAST; i++)
        connect->domainCallbackIds[i] = -1;

    for (gint i = 0; i < VIR_NETWORK_EVENT_ID_LAST; i++)
        connect->networkCallbackIds[i] = -1;

    for (gint i = 0; i < VIR_NODE_DEVICE_EVENT_ID_LAST; i++)
        connect->nodeDevCallbackIds[i] = -1;

    for (gint i = 0; i < VIR_SECRET_EVENT_ID_LAST; i++)
        connect->secretCallbackIds[i] = -1;

    for (gint i = 0; i < VIR_STORAGE_POOL_EVENT_ID_LAST; i++)
        connect->storagePoolCallbackIds[i] = -1;

    connect->bus = bus;
    connect->uri = uri;
    connect->connectPath = connectPath;

    virtDBusGDBusRegisterObject(bus,
                                connect->connectPath,
                                interfaceInfo,
                                virtDBusConnectMethodTable,
                                virtDBusConnectPropertyTable,
                                connect);

    virtDBusDomainRegister(connect, error);
    if (error && *error)
        return;

    virtDBusDomainSnapshotRegister(connect, error);
    if (error && *error)
        return;

    virtDBusInterfaceRegister(connect, error);
    if (error && *error)
        return;

    virtDBusNetworkRegister(connect, error);
    if (error && *error)
        return;

    virtDBusNodeDeviceRegister(connect, error);
    if (error && *error)
        return;

    virtDBusNWFilterRegister(connect, error);
    if (error && *error)
        return;

    virtDBusSecretRegister(connect, error);
    if (error && *error)
        return;

    virtDBusStoragePoolRegister(connect, error);
    if (error && *error)
        return;

    virtDBusStorageVolRegister(connect, error);
    if (error && *error)
        return;

    *connectp = connect;
    connect = NULL;
}

void
virtDBusConnectListFree(virtDBusConnect **connectList)
{
    if (!connectList)
        return;

    for (gint i = 0; connectList[i]; i++)
        virtDBusConnectFree(connectList[i]);

    g_free(connectList);
}
