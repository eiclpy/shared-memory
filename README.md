# Sys V Shared memory between C/C++ and Python

## Compile C Version
```
gcc -fPIC -shared shmc/memory-pool.c -o memory-pool.so
```

## Compile C++ Version
```
g++ -fPIC -shared shmcpp/memory-pool.cc -o memory-pool.so
```

## Use Shared Memory in NS-3
### 1. Copy module to src/
```
cp -r shared-memory/ /path/to/ns3/src/
```
### 2. Rebuild NS-3
```
./waf configure
./waf
```

## Run Example C Code
```
gcc shmc/memory-pool.c shmc/test.c -o test
./test
```

## Run Example C++ Code
```
g++ shmcpp/memory-pool.cc shmcpp/test.cc -o test
./test
```

## Run Example NS-3 Code
```
copy src/shared-memory/example/test-shm.cc scratch/
./waf --run scratch/test-shm
```

## Run Python Code
```python
python test.py
```

## NOTICE: C/C++ code and Python code need to run simultaneously