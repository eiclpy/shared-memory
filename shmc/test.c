#include "memory-pool.h"
#include "shm-var.h"
//Typedef(Type) will define the type ShmVar##Type
//and the functions CreateShmVar##Type SetShmVar##Type GetShmVar##Type
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
    for (int i = 0; i < 100000; ++i)
    {
        //Set the variable to 2*i
        //The Python program will wait for the set value of the code
        SetShmVarint(var, 2 * i);
        //Get the variable returned by the python program
        //The code will wait for the Python program return value
        int ret = GetShmVarint(var);
        Assertf(ret == 2 * i + 1, "Error at %d", i);
    }

    //For complex variables (Reduce one memory copy)
    //Register the variable in memory pool with id = 2
    RegisterMemory(2, sizeof(BigVar));
    for (int i = 0; i < 1000; ++i)
    {
        //Verify that the operation should be performed
        //If the version is odd, the python program operate the memory
        //else the C program operate the memory
        while (GetMemoryVersion(2) % 2 != 0)
            ;
        //Acquire the variable
        //The Python program will wait until release the variable
        BigVar *bigvar = (BigVar *)AcquireMemory(2);
        if (i != 0)
            for (int j = 0; j < 100; ++j)
                Assertf(bigvar->a[j] == 2 * ((i - 1) * 1000 + j), "Error at %d %d", i, j);
        for (int j = 0; j < 100; ++j)
            bigvar->a[j] = i * 1000 + j;
        //Release memory's operation right
        ReleaseMemory(2);
    }

    //Free memory pool
    FreeMemory();

    puts("OK");
}