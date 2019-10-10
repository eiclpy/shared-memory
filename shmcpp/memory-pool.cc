/* -*- Mode:C; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#include <cstring>
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
SharedMemoryCtrl *curCtrlInfo;

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
    gCtrlBlockSize = sizeof(SharedMemoryCtrl);
    gConfigLen = sizeof(CtrlInfoBlock);

    gMemoryPoolSize = poolSize;
    gMemoryKey = key;
    printf("Key %u  Size %u\n", gMemoryKey, gMemoryPoolSize);

    gShmid = shmget(gMemoryKey, gMemoryPoolSize, 0666 | IPC_CREAT);
    Assertf(gShmid >= 0, "Cannot alloc shared memory(shmid<0)");
    gMemoryPoolPtr = (uint8_t *)shmat(gShmid, NULL, 0);
    Assertf(gMemoryPoolPtr > 0, "Cannot alloc shared memory(ptr error)");

    shmctl(gShmid, IPC_STAT, &gShmds);
    gIsCreator = getpid() == gShmds.shm_cpid;
    if (gIsCreator)
    {
        puts("Creator");
        memset(gMemoryPoolPtr, 0, poolSize);
    }
    else
    {
        puts("User");
        sleep(1);
    }

    gCtrlInfo = (CtrlInfoBlock *)(gMemoryPoolPtr + gMemoryPoolSize - gConfigLen);
    curCtrlInfo = (SharedMemoryCtrl *)gCtrlInfo;
    gCurrentVersion = 0;
    CtrlInfoLock();
    if (gCtrlInfo->ctrlInfoVersion == gCurrentVersion)
    {
        CtrlInfoUnlock();
        return;
    }
    while (curCtrlInfo->ctrlInfo == FollowCtrlBlock)
    {
        --curCtrlInfo;
        Assertf(gMemoryCtrlInfo[curCtrlInfo->id] == 0, "Id has been used");
        gMemoryCtrlInfo[curCtrlInfo->id] = curCtrlInfo;
        printf("Load %u\n", curCtrlInfo->id);
    }
    gCurrentVersion = gCtrlInfo->ctrlInfoVersion;
    CtrlInfoUnlock();
}

void FreeMemory(void)
{
    shmctl(gShmid, IPC_RMID, 0);
}

void *GetMemory(uint16_t id, uint32_t size)
{
    Assertf(id < MAX_ID, "Id out of range");
    CtrlInfoLock();
    if (gCtrlInfo->ctrlInfoVersion != gCurrentVersion)
    {
        while (curCtrlInfo->ctrlInfo == FollowCtrlBlock)
        {
            --curCtrlInfo;
            Assertf(gMemoryCtrlInfo[curCtrlInfo->id] == 0, "Id has been used");
            gMemoryCtrlInfo[curCtrlInfo->id] = curCtrlInfo;
            printf("Load %u\n", curCtrlInfo->id);
        }
        gCurrentVersion = gCtrlInfo->ctrlInfoVersion;
    }
    if (gMemoryCtrlInfo[id] != 0)
    {
        Assertf(size == gMemoryCtrlInfo[id]->size, "Size of memory error");
        CtrlInfoUnlock();
        return gMemoryPoolPtr + gMemoryCtrlInfo[id]->offset;
    };
    printf("Alloc id %u\n", id);

    Assertf(gCtrlInfo->freeMemOffset + size < gMemoryPoolSize - gConfigLen - (gCtrlInfo->curMemNum + 1) * gCtrlBlockSize, "Memory pool full");
    SharedMemoryCtrl *lastCtrlBlock = (SharedMemoryCtrl *)(gMemoryPoolPtr + gMemoryPoolSize - gConfigLen - (gCtrlInfo->curMemNum * gCtrlBlockSize));
    lastCtrlBlock->ctrlInfo = FollowCtrlBlock;
    SharedMemoryCtrl *newCtrlBlock = lastCtrlBlock - 1;
    newCtrlBlock->ctrlInfo = LastCtrlBlock;
    newCtrlBlock->id = id;
    newCtrlBlock->size = size;
    newCtrlBlock->offset = gCtrlInfo->freeMemOffset;
    gMemoryCtrlInfo[id] = newCtrlBlock;

    ++gCtrlInfo->ctrlInfoVersion;
    gCurrentVersion = gCtrlInfo->ctrlInfoVersion;

    ++gCtrlInfo->curMemNum;
    void *ret = gMemoryPoolPtr + gCtrlInfo->freeMemOffset;
    gCtrlInfo->freeMemOffset += size;
    CtrlInfoUnlock();
    return ret;
}

void RegisterMemory(uint16_t id, uint32_t size)
{
    gMemoryLocker[id] = (SharedMemoryLockable *)GetMemory(id, size + sizeof(SharedMemoryLockable));
}

void *AcquireMemory(uint16_t id) //Should register first
{
    SharedMemoryLockable *info = gMemoryLocker[id];
    while (!__sync_bool_compare_and_swap(&info->preVersion, info->version, info->version + (uint8_t)1))
        ;
    return info->mem;
}

void ReleaseMemory(uint16_t id) //Should register first
{
    SharedMemoryLockable *info = gMemoryLocker[id];
    Assertf(__sync_bool_compare_and_swap(&info->version, info->preVersion - (uint8_t)1, info->preVersion), "Lock %u status error", id);
}

uint8_t GetMemoryVersion(uint8_t id)
{
    return gMemoryLocker[id]->version;
}