include(FetchContent)

# Fetch GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
)
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(glfw)

# Fetch WebGPU-distribution
FetchContent_Declare(
    webgpu
    GIT_REPOSITORY https://github.com/eliemichel/WebGPU-distribution
    GIT_TAG        wgpu-v24.0.0.2
)
FetchContent_MakeAvailable(webgpu)

add_executable(sandbox
    src/main.cpp
    src/motion_math.cpp
    src/movement.cpp
    src/renderer.cpp
    src/assets.cpp
    src/character.cpp
    src/map.cpp
    src/minimap.cpp
    src/labels.cpp
    src/audio.cpp
    src/validation.cpp
)

target_include_directories(sandbox PUBLIC include)
target_link_libraries(sandbox PUBLIC sandbox-data glfw webgpu)

target_copy_webgpu_binaries(sandbox)
