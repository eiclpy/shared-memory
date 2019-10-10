#include "memory-pool.h"
#ifndef READABLE
#define READABLE 0xff
#endif
#ifndef SETABLE
#define SETABLE 0x0
#endif
/*
适用于小型数据
*/
#define Typedef(Type)                                                                              \
    typedef struct                                                                                 \
    {                                                                                              \
        uint8_t tagRd;                                                                             \
        uint8_t tagWt;                                                                             \
        Type data;                                                                                 \
    } __attribute__((__packed__)) ShmVar##Type;                                                    \
    ShmVar##Type *CreateShmVar##Type(uint16_t id)                                                  \
    {                                                                                              \
        ShmVar##Type *ret = (ShmVar##Type *)GetMemory(id, sizeof(ShmVar##Type));                   \
        Assertf(ret > 0, "memory pool full");                                                      \
        return ret;                                                                                \
    }                                                                                              \
    Type GetShmVar##Type(ShmVar##Type *ptr)                                                        \
    {                                                                                              \
        while (ptr->tagRd != READABLE)                                                             \
            ;                                                                                      \
        Type ret = ptr->data;                                                                      \
        Assertf(__sync_bool_compare_and_swap(&ptr->tagRd, READABLE, SETABLE), "Tag status error"); \
        return ret;                                                                                \
    }                                                                                              \
    void SetShmVar##Type(ShmVar##Type *ptr, Type data)                                             \
    {                                                                                              \
        while (ptr->tagWt != SETABLE)                                                              \
            ;                                                                                      \
        ptr->data = data;                                                                          \
        Assertf(__sync_bool_compare_and_swap(&ptr->tagWt, SETABLE, READABLE), "Tag status error"); \
    }
