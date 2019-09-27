/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#pragma once
#include <cstdio>
#include <cassert>
#include <cstring>
#include <iostream>
#include <unordered_map>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/ipc.h>

#define DEFAULT_SEM_KEY 963258741
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

struct CtrlInfoBlock
{
	uint16_t ctrlInfo : 2;
	uint16_t ctrlInfoVersion : 14;
	uint32_t freeMemOffset;
	uint32_t curMemNum;
} __attribute__((__packed__));

struct SharedMemoryCtrl
{
	uint16_t ctrlInfo : 2;
	uint16_t id : 14;
	uint32_t size;
	uint32_t offset;
} __attribute__((__packed__));

struct SharedMemoryLockable
{
	uint8_t version;
	uint8_t preVersion;
	uint8_t mem[0];
} __attribute__((__packed__));

extern "C" void Init(uint32_t key, uint32_t poolSize);
extern "C" void FreeMemory(void);
extern "C" void *GetMemory(uint16_t id, uint32_t size);
extern "C" void RegisterMemory(uint16_t id, uint32_t size);
extern "C" void *UseMemory(uint16_t id);
extern "C" void ReleaseMemory(uint16_t id);
extern "C" uint8_t GetMemoryVersion(uint8_t id);