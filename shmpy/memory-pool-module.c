#define PY_SSIZE_T_CLEAN
#include <Python.h>
// #include "structmember.h"
#include"memory-pool.h"


/* Module method table */
static PyMethodDef MemoryPool_methods[] = {
    {"Init",
     (PyCFunction)py_init,
     METH_VARARGS,
     "Init shared memory pool"},
    {"FreeMemory",
     (PyCFunction)py_freeMemory,
     METH_NOARGS,
     "Free shared memory pool"},
    {"GetMemory",
     (PyCFunction)py_getMemory,
     METH_VARARGS,
     "Get memory of id"},
    {"RegisterMemory",
     (PyCFunction)py_regMemory,
     METH_VARARGS,
     "Register memory of id"},
    {"AcquireMemory",
     (PyCFunction)py_acquireMemory,
     METH_VARARGS,
     "Acquire memory of id"},
    {"ReleaseMemory",
     (PyCFunction)py_releaseMemory,
     METH_VARARGS,
     "Release memory of id"},
    {"GetMemoryVersion",
     (PyCFunction)py_getMemoryVersion,
     METH_VARARGS,
     "Get memory version of id"},
    {NULL, NULL, 0, NULL} /* Sentinel */
};

/* Module structure */
static struct PyModuleDef shmModule = {
    PyModuleDef_HEAD_INIT,
    "shm_pool",           /* name of module */
    "Shared memory pool", /* Doc string (may be NULL) */
    -1,                   /* Size of per-interpreter state or -1 */
    MemoryPool_methods    /* Method table */
};

/* Module initialization function */
PyMODINIT_FUNC
PyInit_shm_pool(void)
{
    return PyModule_Create(&shmModule);
}