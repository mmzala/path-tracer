# Vulkan Path Tracer

![image](https://github.com/user-attachments/assets/9dbfdd36-3042-4731-adb6-0e0d1db1f401)

A real-time path tracer built using Vulkan with the `VK_KHR_ray_tracing_pipeline` extension. 
The renderer supports a wide range of 3D models via [Assimp](https://github.com/assimp/assimp), enabling the import of virtually any format supported by the library. 
An accumulation buffer increases sample count over successive frames when the camera is stationary, enhancing image quality while maintaining interactive performance.

## Build Instructions

The project currently only supports Windows. First you'll have to install the [VulkanSDK](https://vulkan.lunarg.com/sdk/home).

To setup the project use the following cmake command:
```
cmake CMakeLists.txt
```

To build the project it is recommended to use the [Clang](https://clang.llvm.org/get_started.html) compiler.
This is the only compiler the project is tested with.

All of the build files can be found in the root directory inside the `build` folder.

## Planned Features

- Physically Based Rendering
- Translucency

## Libraries

- [Vulkan](https://vulkan.lunarg.com/sdk/home)
- [Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers)
- [Vulkan Memory Allocator](https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator)
- [SDL](https://github.com/libsdl-org/SDL)
- [glm](https://github.com/g-truc/glm)
- [assimp](https://github.com/assimp/assimp)
- [stb](https://github.com/nothings/stb)
- [spdlog](https://github.com/gabime/spdlog)
