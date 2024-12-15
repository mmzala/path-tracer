# Vulkan Memory allocator

FetchContent_Declare(
        VMA
        GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
        GIT_TAG v3.1.0
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(VMA)

# SpdLog

FetchContent_Declare(
        SpdLog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.14.1
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(SpdLog)

# SDL

FetchContent_Declare(
        SDL
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG preview-3.1.3
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)

set(SDL_SHARED OFF)
set(SDL_STATIC ON)
FetchContent_MakeAvailable(SDL)

target_compile_options(SDL3-static PRIVATE -Wno-deprecated-declarations)

# GLM

FetchContent_Declare(
        GLM
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.1
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(GLM)

# Guarantees GLM follows the same perspective convention as Vulkan
target_compile_definitions(glm PUBLIC GLM_FORCE_DEPTH_ZERO_TO_ONE PUBLIC GLM_ENABLE_EXPERIMENTAL)

# FastGLTF

FetchContent_Declare(
        FastGLTF
        GIT_REPOSITORY https://github.com/spnda/fastgltf.git
        GIT_TAG main
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
)

FetchContent_MakeAvailable(FastGLTF)
