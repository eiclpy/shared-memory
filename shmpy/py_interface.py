from ctypes import *
import os

shm = CDLL(os.path.abspath('./memory-pool.so'))

# extern "C" void Init(uint32_t key, uint32_t poolSize);
Init = shm.Init
Init.argtypes = [c_uint32, c_uint32]
# extern "C" void FreeMemory(void);
FreeMemory = shm.FreeMemory
# extern "C" void *GetMemory(uint16_t id, uint32_t size);
GetMemory = shm.GetMemory
GetMemory.argtypes = [c_uint16, c_uint32]
GetMemory.restype = c_void_p
# extern "C" void RegisterMemory(uint16_t id, uint32_t size);
RegisterMemory = shm.RegisterMemory
RegisterMemory.argtypes = [c_uint16, c_uint32]
# extern "C" void *AcquireMemory(uint16_t id);
AcquireMemory = shm.AcquireMemory
AcquireMemory.argtypes = [c_uint16]
AcquireMemory.restype = c_void_p
# extern "C" void ReleaseMemory(uint16_t id);
ReleaseMemory = shm.ReleaseMemory
ReleaseMemory.argtypes = [c_uint16]
# extern "C" uint8_t GetMemoryVersion(uint8_t id);
GetMemoryVersion = shm.GetMemoryVersion
GetMemoryVersion.argtypes = [c_uint16]
GetMemoryVersion.restype = c_uint8

READABLE = 0xff
SETABLE = 0


class ShmVar:
    def __init__(self, uid, structType):
        class sType(Structure):
            _pack_ = 1
            _fields_ = [
                ('tagWt', c_ubyte),
                ('tagRd', c_ubyte),
                ('data', structType),
            ]
        self.m_id = uid
        self.m_dataType = sType
        self.m_addr = GetMemory(self.m_id, sizeof(self.m_dataType))
        print(self.m_addr)
        self.m_obj = self.m_dataType.from_address(self.m_addr)

    def Set(self, data):
        while self.m_obj.tagWt != SETABLE:
            pass
        self.m_obj.data = data
        self.m_obj.tagWt = READABLE

    def Get(self):
        while self.m_obj.tagRd != READABLE:
            pass
        ret = self.m_obj.data
        self.m_obj.tagRd = SETABLE
        return ret

    def GetObj(self):
        return self.m_obj


class ShmBigVar:
    def __init__(self, uid, structType):
        self.m_id = uid
        self.m_dataType = structType
        RegisterMemory(self.m_id, sizeof(self.m_dataType))
        self.m_obj = self.m_dataType.from_address(AcquireMemory(self.m_id))
        ReleaseMemory(self.m_id)

    def __enter__(self):
        print('enter')
        AcquireMemory(self.m_id)
        return self.m_obj

    def __exit__(self, Type, value, traceback):
        print('exit')
        ReleaseMemory(self.m_id)


__all__ = ['Init', 'FreeMemory', 'ShmVar', 'ShmBigVar']

if __name__ == "__main__":
    Init(2333, 4096)
    v = ShmBigVar(233, c_int*10)
    with v as o:
        for i in range(10):
            o[i] = c_int(i)
        print(*o)
    FreeMemory()
