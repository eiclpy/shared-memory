/* -*- Mode:C; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/ipc.h>
// #include "structmember.h"
#include "memory-pool.h"

uint8_t *gMemoryPoolPtr;
CtrlInfoBlock *gCtrlInfo;
bool gIsCreator;
key_t gShmid;
struct shmid_ds gShmds;
uint32_t gMemoryPoolSize;
uint32_t gMemoryKey;
uint32_t gConfigLen;
uint32_t gCtrlBlockSize;
uint16_t gCurrentVersion;
SharedMemoryCtrl *gCurCtrlInfo;

SharedMemoryCtrl *gMemoryCtrlInfo[MAX_ID];
SharedMemoryLockable *gMemoryLocker[MAX_ID];

#define Assertf(A, M, ...)                                  \
    if (!(A))                                               \
    {                                                       \
        PyErr_Format(PyExc_RuntimeError, M, ##__VA_ARGS__); \
    }

void CtrlInfoLock(void)
{
    while (!__sync_bool_compare_and_swap(&gCtrlInfo->ctrlInfoLock, 0x0, 0xffff))
        ;
}
bool CtrlInfoUnlock(void)
{
    return __sync_bool_compare_and_swap(&gCtrlInfo->ctrlInfoLock, 0xffff, 0x0);
}

static void *GetMemory(uint16_t id, uint32_t size)
{
    if (id >= MAX_ID)
    {
        PyErr_SetString(PyExc_RuntimeError, "Id out of range");
        return NULL;
    }
    CtrlInfoLock();
    if (gCtrlInfo->ctrlInfoVersion != gCurrentVersion)
    {
        while (gCurCtrlInfo->ctrlInfo == FollowCtrlBlock)
        {
            --gCurCtrlInfo;
            if (gMemoryCtrlInfo[gCurCtrlInfo->id] != 0)
            {
                CtrlInfoUnlock();
                PyErr_Format(PyExc_RuntimeError, "Id %u has been used", gCurCtrlInfo->id);
                return NULL;
            }
            gMemoryCtrlInfo[gCurCtrlInfo->id] = gCurCtrlInfo;
            // printf("Load %u\n", gCurCtrlInfo->id);
        }
        gCurrentVersion = gCtrlInfo->ctrlInfoVersion;
    }
    if (gMemoryCtrlInfo[id] != 0)
    {
        if (size != gMemoryCtrlInfo[id]->size)
        {
            CtrlInfoUnlock();
            PyErr_SetString(PyExc_RuntimeError, "Size of memory error");
            return NULL;
        }
        CtrlInfoUnlock();
        return gMemoryPoolPtr + gMemoryCtrlInfo[id]->offset;
    };
    // printf("Alloc id %u\n", id);

    // Assertf(gCtrlInfo->freeMemOffset + size < gMemoryPoolSize - gConfigLen - (gCtrlInfo->ctrlInfoVersion + 1) * gCtrlBlockSize || (CtrlInfoUnlock(), 0), "Memory pool full");
    if (gCtrlInfo->freeMemOffset + size >= gMemoryPoolSize - gConfigLen - (gCtrlInfo->ctrlInfoVersion + 1) * gCtrlBlockSize)
    {
        CtrlInfoUnlock();
        PyErr_SetString(PyExc_RuntimeError, "Memory pool full");
        return NULL;
    }
    gCurCtrlInfo->ctrlInfo = FollowCtrlBlock;
    --gCurCtrlInfo;
    gCurCtrlInfo->ctrlInfo = LastCtrlBlock;
    gCurCtrlInfo->id = id;
    gCurCtrlInfo->size = size;
    gCurCtrlInfo->offset = gCtrlInfo->freeMemOffset;
    gMemoryCtrlInfo[id] = gCurCtrlInfo;

    ++gCtrlInfo->ctrlInfoVersion;
    gCurrentVersion = gCtrlInfo->ctrlInfoVersion;

    void *ret = gMemoryPoolPtr + gCtrlInfo->freeMemOffset;
    gCtrlInfo->freeMemOffset += size;
    if (!CtrlInfoUnlock())
    {
        PyErr_SetString(PyExc_RuntimeError, "Lock status error");
        return NULL;
    }
    return ret;
}

// void Init(uint32_t key, uint32_t poolSize)
PyObject *py_init(PyObject *self, PyObject *args)
{
    uint32_t key, poolSize;
    if (!PyArg_ParseTuple(args, "II", &key, &poolSize))
    {
        return NULL;
    }
    memset(gMemoryCtrlInfo, 0, sizeof(gMemoryCtrlInfo));
    memset(gMemoryLocker, 0, sizeof(gMemoryLocker));
    gCtrlBlockSize = sizeof(SharedMemoryCtrl);
    gConfigLen = sizeof(CtrlInfoBlock);

    gMemoryPoolSize = poolSize;
    gMemoryKey = key;
    // printf("Key %u  Size %u\n", gMemoryKey, gMemoryPoolSize);

    gShmid = shmget(gMemoryKey, gMemoryPoolSize, 0666 | IPC_CREAT);
    if (gShmid < 0)
    {
        switch (errno)
        {
        case EACCES:
            PyErr_SetString(PyExc_RuntimeError, "Permission error");
            break;
        case EINVAL:
            PyErr_SetString(PyExc_ValueError, "Invalid size");
            break;
        case ENOMEM:
            PyErr_SetString(PyExc_MemoryError, "No enough memory to allocate");
            break;
        case ENOSPC:
            PyErr_SetString(PyExc_OSError, "Too many shared memory identifiers");
            break;
        default:
            PyErr_SetString(PyExc_Exception, "Unknow error");
            break;
        }
        return NULL;
    }
    gMemoryPoolPtr = (uint8_t *)shmat(gShmid, NULL, 0);
    if (gMemoryPoolPtr <= 0)
    {
        gMemoryPoolPtr = NULL;
        switch (errno)
        {
        case EACCES:
            PyErr_SetString(PyExc_RuntimeError, "Permission error");
            break;
        case ENOMEM:
            PyErr_SetString(PyExc_MemoryError, "No enough memory");
            break;
        case EINVAL:
            PyErr_SetString(PyExc_ValueError, "Invalid params");
            break;
        default:
            PyErr_SetString(PyExc_Exception, "Unknow error");
            break;
        }
        return NULL;
    }
    if (shmctl(gShmid, IPC_STAT, &gShmds) < 0)
    {
        switch (errno)
        {
        case EIDRM:
        case EINVAL:
            PyErr_Format(PyExc_RuntimeError, "No shared memory with id %u", gShmid);
            break;

        case EACCES:
            PyErr_SetString(PyExc_RuntimeError, "Permission error");
            break;

        default:
            PyErr_SetString(PyExc_Exception, "Unknow error");
            break;
        }
        return NULL;
    }
    gIsCreator = getpid() == gShmds.shm_cpid;
    if (gIsCreator)
        memset(gMemoryPoolPtr, 0, poolSize);
    else
        sleep(1);

    gCtrlInfo = (CtrlInfoBlock *)(gMemoryPoolPtr + gMemoryPoolSize - gConfigLen);
    gCurCtrlInfo = (SharedMemoryCtrl *)gCtrlInfo;
    gCurrentVersion = 0;
    CtrlInfoLock();
    if (gCtrlInfo->ctrlInfoVersion == gCurrentVersion)
    {
        if (!CtrlInfoUnlock())
        {
            PyErr_SetString(PyExc_RuntimeError, "Lock status error");
            return NULL;
        }
        Py_RETURN_NONE;
    }
    while (gCurCtrlInfo->ctrlInfo == FollowCtrlBlock)
    {
        --gCurCtrlInfo;
        if (gMemoryCtrlInfo[gCurCtrlInfo->id] != 0)
        {
            CtrlInfoUnlock();
            PyErr_Format(PyExc_RuntimeError, "Id %u has been used", gCurCtrlInfo->id);
            return NULL;
        }
        gMemoryCtrlInfo[gCurCtrlInfo->id] = gCurCtrlInfo;
    }
    gCurrentVersion = gCtrlInfo->ctrlInfoVersion;
    if (!CtrlInfoUnlock())
    {
        PyErr_SetString(PyExc_RuntimeError, "Lock status error");
        return NULL;
    }
    Py_RETURN_NONE;
}

// void FreeMemory(void)
PyObject *py_freeMemory(PyObject *self)
{
    if (!gIsCreator)
        Py_RETURN_NONE;
    shmctl(gShmid, IPC_STAT, &gShmds);
    while (gShmds.shm_nattch != 1)
        shmctl(gShmid, IPC_STAT, &gShmds);
    shmctl(gShmid, IPC_RMID, 0);
    Py_RETURN_NONE;
}

// void *GetMemory(uint16_t id, uint32_t size)
PyObject *py_getMemory(PyObject *self, PyObject *args)
{
    uint16_t id;
    uint32_t size;
    if (!PyArg_ParseTuple(args, "HI", &id, &size))
        return NULL;
    void *ret = GetMemory(id, size);
    if (NULL == ret)
        return NULL;
    return Py_BuildValue("K", (unsigned long long)ret);
}

// void *RegisterMemory(uint16_t id, uint32_t size)
PyObject *py_regMemory(PyObject *self, PyObject *args)
{
    uint16_t id;
    uint32_t size;
    if (!PyArg_ParseTuple(args, "HI", &id, &size))
        return NULL;

    void *ret = GetMemory(id, size + sizeof(SharedMemoryLockable));
    if (NULL == ret)
        return NULL;
    gMemoryLocker[id] = (SharedMemoryLockable *)ret;

    return Py_BuildValue("K", (unsigned long long)gMemoryLocker[id]->mem);
}

// void *AcquireMemory(uint16_t id) //Should register first
PyObject *py_acquireMemory(PyObject *self, PyObject *args)
{
    uint16_t id;
    if (!PyArg_ParseTuple(args, "H", &id))
    {
        return NULL;
    }
    SharedMemoryLockable *info = gMemoryLocker[id];
    while (!__sync_bool_compare_and_swap(&info->preVersion, info->version, info->version + (uint8_t)1))
        ;
    return Py_BuildValue("K", (unsigned long long)info->mem);
}

// void ReleaseMemory(uint16_t id) //Should register first
PyObject *py_releaseMemory(PyObject *self, PyObject *args)
{
    uint16_t id;
    if (!PyArg_ParseTuple(args, "H", &id))
    {
        return NULL;
    }
    SharedMemoryLockable *info = gMemoryLocker[id];
    // Assertf(__sync_bool_compare_and_swap(&info->version, info->preVersion - (uint8_t)1, info->preVersion), "Lock %u status error", id);
    if (!__sync_bool_compare_and_swap(&info->version, info->preVersion - (uint8_t)1, info->preVersion))
    {
        PyErr_Format(PyExc_RuntimeError, "Lock %u status error", id);
        return NULL;
    }
    Py_RETURN_NONE;
}

// void ReleaseMemoryAndRollback(uint16_t id)
PyObject *py_releaseMemoryAndRollback(PyObject *self, PyObject *args)
{
    uint16_t id;
    if (!PyArg_ParseTuple(args, "H", &id))
        return NULL;

    SharedMemoryLockable *info = gMemoryLocker[id];
    // Assertf(__sync_bool_compare_and_swap(&info->preVersion, info->version + (uint8_t)1, info->version), "Lock %u status error", id);
    if (!__sync_bool_compare_and_swap(&info->preVersion, info->version + (uint8_t)1, info->version))
    {
        PyErr_Format(PyExc_RuntimeError, "Lock %u status error", id);
        return NULL;
    }
    Py_RETURN_NONE;
}

// uint8_t GetMemoryVersion(uint8_t id)
PyObject *py_getMemoryVersion(PyObject *self, PyObject *args)
{
    uint16_t id;
    if (!PyArg_ParseTuple(args, "H", &id))
    {
        return NULL;
    }
    return Py_BuildValue("B", gMemoryLocker[id]->version);
}
