/*
 * Copyright (c) 2020 Huawei Device Co., Ltd.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#include "default_client.h"

#include <log.h>
#include <ohos_errno.h>
#include <pthread.h>
#include <string.h>
#include <service_registry.h>
#include "client_factory.h"
#include "dbinder_service.h"
#include "endpoint.h"
#include "ipc_skeleton.h"
#include "iproxy_client.h"
#include "memory_adapter.h"
#include "samgr_server.h"
#include "thread_adapter.h"

#undef LOG_TAG
#undef LOG_DOMAIN
#define LOG_TAG "Samgr"
#define LOG_DOMAIN 0xD001800
typedef struct IClientHeader IClientHeader;
typedef struct IDefaultClient IDefaultClient;
typedef struct IClientEntry IClientEntry;
struct IClientHeader {
    SaName key;
    SvcIdentity target;
    uint32 deadId;
    uintptr_t saId;
};

struct IClientEntry {
    INHERIT_IUNKNOWNENTRY(IClientProxy);
};

#pragma pack(1)
struct IDefaultClient {
    IClientHeader header;
    IClientEntry entry;
};
#pragma pack()

static int AddRef(IUnknown *iUnknown);
static int Release(IUnknown *proxy);
static int ProxyInvoke(IClientProxy *proxy, int funcId, IpcIo *request, IOwner owner, INotify notify);
static int OnServiceExit(void *ipcMsg, IpcIo *data, void *argv);
static SvcIdentity QueryIdentity(const char *service, const char *feature);
static SvcIdentity QueryRemoteIdentity(const char *deviceId, const char *service, const char *feature);
static const IClientEntry DEFAULT_ENTRY = {CLIENT_IPROXY_BEGIN, .Invoke = ProxyInvoke, IPROXY_END};
static MutexId g_mutex = NULL;
static pthread_mutex_t g_handleMutex = PTHREAD_MUTEX_INITIALIZER;
static int32_t g_handle = 0;

static int32_t GetNextHandle(void)
{
    pthread_mutex_lock(&g_handleMutex);
    int32_t handle = ++g_handle;
    pthread_mutex_unlock(&g_handleMutex);
    return handle;
}

IUnknown *SAMGR_CreateIProxy(const char *service, const char *feature)
{
    SvcIdentity identity = QueryIdentity(service, feature);
    if (identity.handle == INVALID_INDEX) {
        return NULL;
    }

    IDefaultClient *client = SAMGR_CreateIClient(service, feature, sizeof(IClientHeader));
    if (client == NULL) {
        client = SAMGR_Malloc(sizeof(IDefaultClient));
        if (client == NULL) {
            return NULL;
        }
        client->entry = DEFAULT_ENTRY;
    }

    IClientHeader *header = &client->header;
    header->target = identity;
    header->key.service = service;
    header->key.feature = feature;
    (void)AddDeathRecipient(identity, OnServiceExit, client, &header->deadId);

    IClientEntry *entry = &client->entry;
    entry->iUnknown.Invoke = ProxyInvoke;
    entry->iUnknown.AddRef = AddRef;
    entry->iUnknown.Release = Release;
    return GET_IUNKNOWN(*entry);
}

IUnknown *SAMGR_CreateIRemoteProxy(const char* deviceId, const char *service, const char *feature)
{
    SvcIdentity identity = QueryRemoteIdentity(deviceId, service, feature);

    IDefaultClient *client = SAMGR_CreateIClient(service, feature, sizeof(IClientHeader));
    if (client == NULL) {
        client = SAMGR_Malloc(sizeof(IDefaultClient));
        if (client == NULL) {
            return NULL;
        }
        client->entry = DEFAULT_ENTRY;
    }

    IClientHeader *header = &client->header;
    header->target = identity;
    header->key.service = service;
    header->key.feature = feature;
    SaNode *saNode = GetSaNodeBySaName(service, feature);
    if (saNode != NULL) {
        header->saId = saNode->saId;
    }

    IClientEntry *entry = &client->entry;
    entry->iUnknown.Invoke = ProxyInvoke;
    entry->iUnknown.AddRef = AddRef;
    entry->iUnknown.Release = Release;
    return GET_IUNKNOWN(*entry);
}

SvcIdentity SAMGR_GetRemoteIdentity(const char *service, const char *feature)
{
    SvcIdentity identity = {INVALID_INDEX, INVALID_INDEX, INVALID_INDEX};
    IUnknown *iUnknown = SAMGR_FindServiceApi(service, feature);
    if (iUnknown == NULL) {
        return identity;
    }
    IClientProxy *proxy = NULL;
    if (iUnknown->QueryInterface(iUnknown, CLIENT_PROXY_VER, (void **)&proxy) != EC_SUCCESS || proxy == NULL) {
        return identity;
    }
    struct IDefaultClient *client = GET_OBJECT(proxy, struct IDefaultClient, entry.iUnknown);
    identity = client->header.target;
    proxy->Release((IUnknown *)proxy);
    return identity;
}

SaName *SAMGR_GetSAName(const IUnknown *proxy)
{
    IDefaultClient *client = GET_OBJECT(proxy, IDefaultClient, entry.iUnknown);
    return &(client->header.key);
}

int SAMGR_CompareSAName(const SaName *key1, const SaName *key2)
{
    if (key1 == key2) {
        return 0;
    }

    if (key1->service != key2->service) {
        int ret = strcmp(key1->service, key2->service);
        if (ret != 0) {
            return ret;
        }
    }

    if (key1->feature == key2->feature) {
        return 0;
    }

    if (key1->feature == NULL) {
        return -1;
    }

    if (key2->feature == NULL) {
        return 1;
    }

    return strcmp(key1->feature, key2->feature);
}

static int AddRef(IUnknown *iUnknown)
{
    MUTEX_Lock(g_mutex);
    int ref = IUNKNOWN_AddRef(iUnknown);
    MUTEX_Unlock(g_mutex);
    return ref;
}

static int Release(IUnknown *proxy)
{
    MUTEX_Lock(g_mutex);
    int ref = IUNKNOWN_Release(proxy);
    MUTEX_Unlock(g_mutex);
    if (ref != 0) {
        return ref;
    }
    IDefaultClient *client = GET_OBJECT(proxy, IDefaultClient, entry.iUnknown);
    int ret = SAMGR_ReleaseIClient(client->header.key.service, client->header.key.feature, client);
    if (ret == EC_NOHANDLER) {
        SAMGR_Free(client);
        return EC_SUCCESS;
    }
    return ret;
}

static int ProxyInvoke(IClientProxy *proxy, int funcId, IpcIo *request, IOwner owner, INotify notify)
{
    if (proxy == NULL) {
        return EC_INVALID;
    }

    IDefaultClient *client = GET_OBJECT(proxy, IDefaultClient, entry.iUnknown);
    IClientHeader *header = &client->header;

    IpcIo reply;
    void *replyBuf = NULL;
    MessageOption flag;
    MessageOptionInit(&flag);
    flag.flags = (notify == NULL) ? TF_OP_ASYNC : TF_OP_SYNC;
    HILOG_DEBUG(HILOG_MODULE_SAMGR, "%d %lu, %lu, saId: %d\n", header->target.handle, header->target.token,
           header->target.cookie, header->saId);
    IpcIo requestWrapper;
    uint8_t *tmpData2 = (uint8_t *) malloc(MAX_IO_SIZE);
    if (tmpData2 == NULL) {
        HILOG_ERROR(HILOG_MODULE_SAMGR, "malloc data for ipc io failed\n");
        return EC_INVALID;
    } else {
        IpcIoInit(&requestWrapper, tmpData2, MAX_IO_SIZE, MAX_OBJ_NUM);
    }
    WriteInt32(&requestWrapper, (int32_t)header->saId);
    if (!IpcIoAppend(&requestWrapper, request)) {
        HILOG_ERROR(HILOG_MODULE_SAMGR, "ipc io append fail\n");
        FreeBuffer(tmpData2);
        return EC_INVALID;
    }
    int ret = SendRequest(header->target, funcId, &requestWrapper, &reply, flag, (uintptr_t *)&replyBuf);
    FreeBuffer(tmpData2);

    if (notify != NULL) {
        notify(owner, ret, &reply);
    }

    if (replyBuf != NULL) {
        FreeBuffer(replyBuf);
    }
    return ret;
}

static int OnServiceExit(void *ipcMsg, IpcIo *data, void *argv)
{
    (void)data;
    IClientHeader *header = (IClientHeader *)argv;
    (void)RemoveDeathRecipient(header->target, header->deadId);
    header->deadId = INVALID_INDEX;
    header->target.handle = INVALID_INDEX;
    header->target.token = INVALID_INDEX;
    header->target.cookie = INVALID_INDEX;
    if (ipcMsg != NULL) {
        FreeBuffer(ipcMsg);
    }
    HILOG_ERROR(HILOG_MODULE_SAMGR, "Miss the remote service<%u, %u>!", header->target.handle, header->target.token);
    return EC_SUCCESS;
}

static SvcIdentity QueryIdentity(const char *service, const char *feature)
{
    IpcIo req;
    uint8 data[MAX_DATA_LEN];
    IpcIoInit(&req, data, MAX_DATA_LEN, 0);
    WriteUint32(&req, RES_FEATURE);
    WriteUint32(&req, OP_GET);
    WriteString(&req, service);
    WriteBool(&req, feature == NULL);
    if (feature != NULL) {
        WriteBool(&req, feature);
    }
    IpcIo reply;
    void *replyBuf = NULL;
    SvcIdentity samgr = {SAMGR_HANDLE, SAMGR_TOKEN, SAMGR_COOKIE};
    MessageOption flag;
    MessageOptionInit(&flag);
    int ret = SendRequest(samgr, INVALID_INDEX, &req, &reply, flag, (uintptr_t *)&replyBuf);
    int32_t sa_ret = EC_FAILURE;
    ret = (ret != EC_SUCCESS) ? EC_FAILURE : ReadInt32(&reply, &sa_ret);
    SvcIdentity target = {INVALID_INDEX, INVALID_INDEX, INVALID_INDEX};
    if (sa_ret == EC_SUCCESS) {
        ReadRemoteObject(&reply, &target);
    }
    if (ret == EC_PERMISSION) {
        HILOG_INFO(HILOG_MODULE_SAMGR, "Cannot Access<%s, %s> No Permission!", service, feature);
    }
    if (replyBuf != NULL) {
        FreeBuffer(replyBuf);
    }
    return target;
}

static SvcIdentity QueryRemoteIdentity(const char *deviceId, const char *service, const char *feature)
{
    char saName[2 * MAX_NAME_LEN + 2];
    (void)sprintf_s(saName, 2 * MAX_NAME_LEN + 2, "%s#%s", service?service:"", feature?feature:"");
    HILOG_INFO(HILOG_MODULE_SAMGR, "saName %s, make remote binder start", saName);

    SvcIdentity target = {INVALID_INDEX, INVALID_INDEX, INVALID_INDEX};;
    SaNode *saNode = GetSaNodeBySaName(service, feature);
    if (saNode == NULL) {
        HILOG_ERROR(HILOG_MODULE_SAMGR, "service: %s feature: %s have no saId in sa map", service, feature);
        return target;
    }
    int32_t ret = MakeRemoteBinder(saName, strlen(saName), deviceId, strlen(deviceId), saNode->saId, 0,
                                   &target);
    if (ret != EC_SUCCESS) {
        HILOG_ERROR(HILOG_MODULE_SAMGR, "MakeRemoteBinder failed");
        return target;
    }
    target.handle = GetNextHandle();
    extern void WaitForProxyInit(SvcIdentity *svc);
    WaitForProxyInit(&target);
    HILOG_ERROR(HILOG_MODULE_SAMGR, "MakeRemoteBinder sid handle=%d", target.handle);
    return target;
}
