/* -*- Mode:C; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#pragma once
#include <stdio.h>
#include <stdlib.h>

#define MAX_ID 16384
#define Packed __attribute__((__packed__))
#define Assertf(A, M, ...)             \
    if (!(A))                          \
    {                                  \
        printf(M "\n", ##__VA_ARGS__); \
        exit(1);                       \
    }
enum CtrlInfo
{
    LastCtrlBlock = 0,
    FollowCtrlBlock = 0x3
};

typedef unsigned char uint8_t;
typedef unsigned short int uint16_t;
typedef unsigned int uint32_t;

typedef struct
{
    uint16_t ctrlInfo : 2;
    uint16_t ctrlInfoVersion : 14;
    uint16_t ctrlInfoLock;
    uint32_t freeMemOffset;
} Packed CtrlInfoBlock;

typedef struct
{
    uint16_t ctrlInfo : 2;
    uint16_t id : 14;
    uint32_t size;
    uint32_t offset;
} Packed SharedMemoryCtrl;

typedef struct
{
    uint8_t version;
    uint8_t preVersion;
    uint8_t mem[0];
} Packed SharedMemoryLockable;

void Init(uint32_t key, uint32_t poolSize);
void FreeMemory(void);
void *GetMemory(uint16_t id, uint32_t size);
void *RegisterMemory(uint16_t id, uint32_t size);
void *AcquireMemory(uint16_t id);
void ReleaseMemory(uint16_t id);
uint8_t GetMemoryVersion(uint8_t id);