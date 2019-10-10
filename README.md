# shared-memory
Sys V Shared memory between C/C++ and Python

# Compile C Version
```
cd shmc
gcc -fPIC -shared memory-pool.c -o memory-pool.so
```

# Compile C++ Version
```
cd shmcpp
g++ -fPIC -shared memory-pool.c -o memory-pool.so
```

# Example C Code
```c
#include "memory-pool.h"
#include "shm-var.h"
Typedef(int);

typedef struct
{
    int a[100];
} Packed BigVar;

main()
{
    //Init memory pool
    Init(1234, 4096);

    //Use simple variables
    //Create a variable in shared memory pool with id = 1
    ShmVarint *var = CreateShmVarint(1);
    //Set the variable to 233
    //The Python program will wait for the set value of the code
    SetShmVarint(var, 233);
    //Get the variable returned by the python program
    //The code will wait for the Python program return value
    int ret = GetShmVarint(var);
    printf("%d", ret);

    //For complex variables (Reduce one memory copy)
    //Register the variable in memory pool with id = 2
    RegisterMemory(2, sizeof(BigVar));
    //Verify that the operation should be performed
    //If the version is odd, the python program operate the memory
    //else the C program operate the memory
    while (GetMemoryVersion(2) % 2 != 0)
        ;
    //Acquire the variable
    //The Python program will wait until release the variable
    BigVar *bigvar = (BigVar *)AcquireMemory(2);

    for (int i = 0; i < 100; ++i)
        bigvar->a[i] = i;
    //Release memory's operation right
    ReleaseMemory(2);

    //Free memory pool
    FreeMemory();
}
```