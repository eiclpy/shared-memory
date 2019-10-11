# Sys V Shared memory between C/C++ and Python

## Compile C Version
```
gcc -fPIC -shared shmc/memory-pool.c -o memory-pool.so
```

## Compile C++ Version
```
g++ -fPIC -shared shmcpp/memory-pool.c -o memory-pool.so
```

## Run Example C Code
```
gcc shmc/memory-pool.c shmc/test.c -o test
./test
```

## Run Example C++ Code
```
g++ shmc/memory-pool.cc shmc/test.cc -o test
./test
```

## Run Python Code
```python
python test.py
```

## NOTICE: C/C++ code and Python code need to run simultaneously