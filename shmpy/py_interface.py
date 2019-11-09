import shm_pool
from shm_pool import Init, FreeMemory, GetMemory, RegisterMemory, AcquireMemory, ReleaseMemory, ReleaseMemoryRB, GetMemoryVersion
from ctypes import *

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
        self.m_obj = self.m_dataType.from_address(
            RegisterMemory(self.m_id, sizeof(self.m_dataType)))

    def GetVersion(self):
        return int(GetMemoryVersion(self.m_id))

    def __enter__(self):
        # print('enter')
        AcquireMemory(self.m_id)
        return self.m_obj

    def __exit__(self, Type, value, traceback):
        # print('exit')
        ReleaseMemory(self.m_id)


__all__ = ['Init', 'FreeMemory', 'GetMemory', 'RegisterMemory', 'AcquireMemory',
           'ReleaseMemory', 'ReleaseMemoryRB', 'GetMemoryVersion', 'ShmVar', 'ShmBigVar']

if __name__ == "__main__":
    Init(2333, 4096)
    v = ShmBigVar(233, c_int*10)
    with v as o:
        for i in range(10):
            o[i] = c_int(i)
        print(*o)
    FreeMemory()
