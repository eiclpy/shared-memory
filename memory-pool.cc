#include "memory-pool.h"

uint8_t *gMemoryPoolPtr;
CtrlInfoBlock *ctrlInfo;
key_t gShmid;
key_t gSmeid;
uint32_t gMemoryPoolSize;
uint32_t gMemoryKey;
uint32_t gConfigLen;
uint32_t gCtrlBlockSize;
uint16_t gCurrentVersion;
SharedMemoryCtrl *curCtrlInfo;

std::unordered_map<uint16_t, SharedMemoryCtrl *> info;
std::unordered_map<uint16_t, SharedMemoryLockable *> locker;

void CtrlInfoLock(void)
{
    static sembuf lock = {0, -1, SEM_UNDO};
    Assertf(semop(gSmeid, &lock, 1) != -1, "Cannot lock");
}
void CtrlInfoUnlock(void)
{
    static sembuf unlock = {0, 1, SEM_UNDO | IPC_NOWAIT};
    Assertf(semop(gSmeid, &unlock, 1) != -1, "Cannot unlock");
}

void Init(uint32_t key, uint32_t poolSize)
{
    bool create = false;
    gCtrlBlockSize = sizeof(SharedMemoryCtrl);
    gConfigLen = sizeof(CtrlInfoBlock);

    gMemoryPoolSize = poolSize;
    gMemoryKey = key;
    std::cout << " Key " << gMemoryKey << " Size " << gMemoryPoolSize << std::endl;

    gSmeid = semget(DEFAULT_SEM_KEY, 1, IPC_CREAT | 0666);
    Assertf(gSmeid >= 0, "Cannot alloc sme");
    Assertf(semctl(gSmeid, 0, SETVAL, 1) != -1, "Cannot init sme");

    CtrlInfoLock();

    gShmid = shmget(gMemoryKey, gMemoryPoolSize, 0666);
    if (gShmid < 0)
    {
        gShmid = shmget(gMemoryKey, gMemoryPoolSize, 0666 | IPC_CREAT);
        Assertf(gShmid >= 0, "Cannot alloc shared memory(shmid<0)");
        create = true;
    }
    gMemoryPoolPtr = (uint8_t *)shmat(gShmid, NULL, 0);
    Assertf(gMemoryPoolPtr > 0, "Cannot alloc shared memory(ptr error)");
    ctrlInfo = (CtrlInfoBlock *)(gMemoryPoolPtr + gMemoryPoolSize - gConfigLen);
    curCtrlInfo = (SharedMemoryCtrl *)ctrlInfo;
    gCurrentVersion = 0;
    if (create)
        memset(gMemoryPoolPtr, 0, poolSize);
    else
    {
        if (ctrlInfo->ctrlInfoVersion == gCurrentVersion)
        {
            CtrlInfoUnlock();
            return;
        }
        while (curCtrlInfo->ctrlInfo == FollowCtrlBlock)
        {
            --curCtrlInfo;
            auto iter = info.find(curCtrlInfo->id);
            Assertf(iter == info.end(), "Id has been used");
            info[curCtrlInfo->id] = curCtrlInfo;
            std::cout << "Load " << curCtrlInfo->id << std::endl;
        }
        gCurrentVersion = ctrlInfo->ctrlInfoVersion;
    }
    CtrlInfoUnlock();
}

void FreeMemory(void)
{
    shmctl(gShmid, IPC_RMID, 0);
}

void *GetMemory(uint16_t id, uint32_t size)
{
    Assertf(id < 16384, "Id out of range");
    CtrlInfoLock();
    if (ctrlInfo->ctrlInfoVersion != gCurrentVersion)
    {
        while (curCtrlInfo->ctrlInfo == FollowCtrlBlock)
        {
            --curCtrlInfo;
            auto iter = info.find(curCtrlInfo->id);
            // if (iter != info.end())
            // {
            //     std::cout << "Id has been used" << std::endl;
            //     exit(1);
            // }
            Assertf(iter == info.end(), "Id has been used");
            info[curCtrlInfo->id] = curCtrlInfo;
            std::cout << "Load " << curCtrlInfo->id << std::endl;
        }
        gCurrentVersion = ctrlInfo->ctrlInfoVersion;
    }
    auto iter = info.find(id);
    if (iter != info.end())
    {
        Assertf(size == iter->second->size, "Size of memory error");
        CtrlInfoUnlock();
        return gMemoryPoolPtr + iter->second->offset;
    }
    std::cout << "Alloc id " << id << std::endl;
    std::cout << "Free Offset " << ctrlInfo->freeMemOffset << "  Alloc Size  " << size << "  Current Allocated  " << ctrlInfo->curMemNum << std::endl;
    // if (ctrlInfo->freeMemOffset + size >= gMemoryPoolSize - gConfigLen - (ctrlInfo->curMemNum + 1) * gCtrlBlockSize)
    // {
    //     std::cout << "Memory pool full" << std::endl;
    //     exit(1);
    // }
    Assertf(ctrlInfo->freeMemOffset + size < gMemoryPoolSize - gConfigLen - (ctrlInfo->curMemNum + 1) * gCtrlBlockSize, "Memory pool full");
    SharedMemoryCtrl *lastCtrlBlock = (SharedMemoryCtrl *)(gMemoryPoolPtr + gMemoryPoolSize - gConfigLen - (ctrlInfo->curMemNum * gCtrlBlockSize));
    lastCtrlBlock->ctrlInfo = FollowCtrlBlock;
    SharedMemoryCtrl *newCtrlBlock = lastCtrlBlock - 1;
    newCtrlBlock->ctrlInfo = LastCtrlBlock;
    newCtrlBlock->id = id;
    newCtrlBlock->size = size;
    newCtrlBlock->offset = ctrlInfo->freeMemOffset;
    info[id] = newCtrlBlock;

    ++ctrlInfo->ctrlInfoVersion;
    gCurrentVersion = ctrlInfo->ctrlInfoVersion;

    ++ctrlInfo->curMemNum;
    void *ret = gMemoryPoolPtr + ctrlInfo->freeMemOffset;
    ctrlInfo->freeMemOffset += size;
    CtrlInfoUnlock();
    return ret;
}

void RegisterMemory(uint16_t id, uint32_t size)
{
    locker[id] = (SharedMemoryLockable *)GetMemory(id, size + sizeof(SharedMemoryLockable));
}

void *UseMemory(uint16_t id) //Should register first
{
    SharedMemoryLockable *info = locker[id];
    while (!__sync_bool_compare_and_swap(&info->preVersion, info->version, info->version + (uint8_t)1))
        ;
    return info->mem;
}

void ReleaseMemory(uint16_t id) //Should register first
{
    SharedMemoryLockable *info = locker[id];
    Assertf(__sync_bool_compare_and_swap(&info->version, info->preVersion - (uint8_t)1, info->preVersion), "Lock %u status error", id);
}

uint8_t GetMemoryVersion(uint8_t id)
{
    return locker[id]->version;
}