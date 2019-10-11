from py_interface import *
from ctypes import *
Init(1234, 4096)
# id=1 type is c_int
var = ShmVar(1, c_int)
for i in range(100000):
    v = var.Get()
    assert v == i*2
    var.Set(v+1)


class BigVar(Structure):
    _pack_ = 1
    _fields_ = [
        ('a', c_int*100),
    ]

# id=2 type is BigVar
bigvar = ShmBigVar(2, BigVar)
for i in range(1000):
    while bigvar.GetVersion() % 2 != 1:
        pass
    with bigvar as data:
        for j in range(100):
            assert int(data.a[j]) == (i*1000+j)
            data.a[j] *= 2

FreeMemory()

print('OK')
