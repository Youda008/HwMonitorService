# CppClient

A minimalistic C++ client that connects to the TCP socket of the HwMonitorService, and requests the value of a sensor.
Use this if you want to avoid fiddling with the low-level system socket API or adding a networking library to your project.

## Usage

1. Build the library as you would build any other CMake project.
   for example
```
mkdir build-release
cd build-release
cmake -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release ../
mingw32-make
```

2. Add the "include" directory to the include directories of your project.

3. Add the build directory you created to the library directories of your project.

4. Add `hwmoncl` to the libraries of your project.
