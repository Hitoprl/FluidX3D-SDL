# FluidX3D

This is a fork of [FluidX3D](https://github.com/ProjectPhysX/FluidX3D) with the aim of providing real-time graphics support on Linux, as well as other enhacements.

## Build on Linux

To build this fork on Linux, you need some additional dependencies:
- cmake 3.18 or later.
- A C++ compiler with C++20 support.
- SDL2 development libraries (on Debian/Ubuntu: `apt install libsdl2-dev`)
- SDL2 TTF development libraries (on Debian/Ubuntu: `apt install libsdl2-ttf-dev`)
- Zlib developtment libraries (on Debian/Ubuntu: `apt install zlib1g-dev`)

Run the following commands:
```bash
cmake -B build
cmake --build build -j
```
And to run the resulting executable:
```bash
build/src/fluidx3d
```

## Changes in this fork

This fork adds several changes:
- Graphical backend based on SDL, tested on Linux. To enable this backend, uncomment the macro `SDL_GRAPHICS` in `defines.hpp`.
- Build system using cmake.
- Disk cache for voxelized meshes so that it is not necessary to re-voxelize a model on every run.
- Bumped C++ version to C++20 - this was actually my mistake, I wanted to keep it in C++17. There are very few actual C++20 features in use, it would be simple to bring it back to C++17.

Since I've developed this fork on a Linux machine, I haven't been able to test it on Windows, so there might be things broken.
