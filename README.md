# OpenCL Path Tracer

Developed during Advanced Graphics...

## Install using vcpkg
Make sure that your GPU manufacturers OpenCL SDK is installed:

[CUDA Toolkit (Nvidia)]: https://developer.nvidia.com/cuda-downloads



Download and install [vcpkg](https://github.com/Microsoft/vcpkg) and use it to install the dependencies:

```bash
vcpkg install glew
vcpkg install sdl2
vcpkg install freeimage
vcpkg install assimp
vcpkg install glm
```


When configuring CMake pass the location of your vcpkg toolchain file as CMAKE_TOOLCHAIN_FILE:

```bash
cmake -DCMAKE_TOOLCHAIN_FILE="[PATH_TO_VCPKG]\\scripts\\buildsystems\\vcpkg.cmake" (...)
```

## Dependencies
- OpenCL
- OpenGL
- GLEW
- SDL2
- FreeImage
- Assimp
- glm