/* -*- Mode:C; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/ipc.h>
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

void CtrlInfoLock(void)
{
    while (!__sync_bool_compare_and_swap(&gCtrlInfo->ctrlInfoLock, 0x0, 0xffff))
        ;
}
void CtrlInfoUnlock(void)
{
    Assertf(__sync_bool_compare_and_swap(&gCtrlInfo->ctrlInfoLock, 0xffff, 0x0), "Lock status error");
}

void Init(uint32_t key, uint32_t poolSize)
{
    memset(gMemoryCtrlInfo, 0, sizeof(gMemoryCtrlInfo));
    memset(gMemoryLocker, 0, sizeof(gMemoryLocker));
    gCtrlBlockSize = sizeof(SharedMemoryCtrl);
    gConfigLen = sizeof(CtrlInfoBlock);

    gMemoryPoolSize = poolSize;
    gMemoryKey = key;
    // printf("Key %u  Size %u\n", gMemoryKey, gMemoryPoolSize);

    gShmid = shmget(gMemoryKey, gMemoryPoolSize, 0666 | IPC_CREAT);
    Assertf(gShmid >= 0, "Cannot alloc shared memory(shmid<0)");
    gMemoryPoolPtr = (uint8_t *)shmat(gShmid, NULL, 0);
    Assertf(gMemoryPoolPtr > 0, "Cannot alloc shared memory(ptr error)");

    shmctl(gShmid, IPC_STAT, &gShmds);
    gIsCreator = getpid() == gShmds.shm_cpid;
    if (gIsCreator)
    {
        // puts("Creator");
        memset(gMemoryPoolPtr, 0, poolSize);
    }
    else
    {
        // puts("User");
        sleep(1);
    }

    gCtrlInfo = (CtrlInfoBlock *)(gMemoryPoolPtr + gMemoryPoolSize - gConfigLen);
    gCurCtrlInfo = (SharedMemoryCtrl *)gCtrlInfo;
    gCurrentVersion = 0;
    CtrlInfoLock();
    if (gCtrlInfo->ctrlInfoVersion == gCurrentVersion)
    {
        CtrlInfoUnlock();
        return;
    }
    while (gCurCtrlInfo->ctrlInfo == FollowCtrlBlock)
    {
        --gCurCtrlInfo;
        Assertf(gMemoryCtrlInfo[gCurCtrlInfo->id] == 0, "Id has been used");
        gMemoryCtrlInfo[gCurCtrlInfo->id] = gCurCtrlInfo;
        // printf("Load %u\n", gCurCtrlInfo->id);
    }
    gCurrentVersion = gCtrlInfo->ctrlInfoVersion;
    CtrlInfoUnlock();
}

void FreeMemory(void)
{
    if (!gIsCreator)
        return;
    shmctl(gShmid, IPC_STAT, &gShmds);
    while (gShmds.shm_nattch != 1)
        shmctl(gShmid, IPC_STAT, &gShmds);
    shmctl(gShmid, IPC_RMID, 0);
}

void *GetMemory(uint16_t id, uint32_t size)
{
    Assertf(id < MAX_ID, "Id out of range");
    CtrlInfoLock();
    if (gCtrlInfo->ctrlInfoVersion != gCurrentVersion)
    {
        while (gCurCtrlInfo->ctrlInfo == FollowCtrlBlock)
        {
            --gCurCtrlInfo;
            Assertf(gMemoryCtrlInfo[gCurCtrlInfo->id] == 0, "Id %u has been used", gCurCtrlInfo->id);
            gMemoryCtrlInfo[gCurCtrlInfo->id] = gCurCtrlInfo;
            // printf("Load %u\n", gCurCtrlInfo->id);
        }
        gCurrentVersion = gCtrlInfo->ctrlInfoVersion;
    }
    if (gMemoryCtrlInfo[id] != 0)
    {
        Assertf(size == gMemoryCtrlInfo[id]->size, "Size of memory error");
        CtrlInfoUnlock();
        return gMemoryPoolPtr + gMemoryCtrlInfo[id]->offset;
    };
    // printf("Alloc id %u\n", id);

    Assertf(gCtrlInfo->freeMemOffset + size < gMemoryPoolSize - gConfigLen - (gCtrlInfo->ctrlInfoVersion + 1) * gCtrlBlockSize, "Memory pool full");
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
    CtrlInfoUnlock();
    return ret;
}

void *RegisterMemory(uint16_t id, uint32_t size)
{
    gMemoryLocker[id] = (SharedMemoryLockable *)GetMemory(id, size + sizeof(SharedMemoryLockable));
    return gMemoryLocker[id]->mem;
}

void *AcquireMemory(uint16_t id) //Should register first
{
    SharedMemoryLockable *info = gMemoryLocker[id];
    while (!__sync_bool_compare_and_swap(&info->preVersion, info->version, info->version + (uint8_t)1))
        ;
    return info->mem;
}

void *AcquireMemoryCond(uint16_t id, uint8_t mod, uint8_t res)
{
    SharedMemoryLockable *info = gMemoryLocker[id];
    while (info->version % mod != res)
        ;
    while (!__sync_bool_compare_and_swap(&info->preVersion, info->version, info->version + (uint8_t)1))
        ;
    return info->mem;
}

void *AcquireMemoryTarget(uint16_t id, uint8_t tar)
{
    SharedMemoryLockable *info = gMemoryLocker[id];
    while (info->version != tar)
        ;
    while (!__sync_bool_compare_and_swap(&info->preVersion, info->version, info->version + (uint8_t)1))
        ;
    return info->mem;
}

void *AcquireMemoryCondFunc(uint16_t id, bool (*cond)(uint8_t version))
{
    SharedMemoryLockable *info = gMemoryLocker[id];
    while (!cond(info->version))
        ;
    while (!__sync_bool_compare_and_swap(&info->preVersion, info->version, info->version + (uint8_t)1))
        ;
    return info->mem;
}

void ReleaseMemory(uint16_t id) //Should register first
{
    SharedMemoryLockable *info = gMemoryLocker[id];
    Assertf(__sync_bool_compare_and_swap(&info->version, info->preVersion - (uint8_t)1, info->preVersion), "Lock %u status error", id);
}

void ReleaseMemoryAndRollback(uint16_t id)
{
    SharedMemoryLockable *info = gMemoryLocker[id];
    Assertf(__sync_bool_compare_and_swap(&info->preVersion, info->version + (uint8_t)1, info->version), "Lock %u status error", id);
}

uint8_t GetMemoryVersion(uint16_t id)
{
    return gMemoryLocker[id]->version;
}

void IncMemoryVersion(uint16_t id)
{
    SharedMemoryLockable *info = gMemoryLocker[id];
    while (!__sync_bool_compare_and_swap(&info->preVersion, info->version, info->version + (uint8_t)1))
        ;
    Assertf(__sync_bool_compare_and_swap(&info->preVersion, info->version + (uint8_t)1, info->version), "Lock %u status error", id);
}