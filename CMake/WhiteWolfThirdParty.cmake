# Third-party dependencies of the engine.
#
# Every dependency is pinned to an exact commit; the release tag it corresponds to is noted
# beside it. Record each dependency and the reason it was chosen in docs/DECISIONS.md.

include(FetchContent)

# glm: vector, quaternion and matrix math (MIT). Release 1.0.3.
# Header-only use; glm types are part of the engine's public API.
set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 8d1fd52e5ab5590e2c81768ace50c72bae28f2ed
    SYSTEM)
FetchContent_MakeAvailable(glm)

# GLFW: windows and input (zlib licence). Release 3.4.
# Linked statically into the engine DLL; no GLFW type appears in an engine header.
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
if(NOT WIN32)
    # The Linux builds only cross-check the code and run headless, on GLFW's null platform,
    # which is always compiled in. Leaving out X11 and Wayland means that no window system
    # packages have to be installed to build.
    set(GLFW_BUILD_X11 OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_WAYLAND OFF CACHE BOOL "" FORCE)
endif()
FetchContent_Declare(glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 7b6aead9fb88b3623e3b3725ebb42670cbe4c579
    SYSTEM)
FetchContent_MakeAvailable(glfw)
set_target_properties(glfw PROPERTIES FOLDER "ThirdParty")

# Vulkan-Headers: the declarations of the Vulkan API (Apache-2.0 OR MIT). Release v1.4.352,
# the version NVRHI is developed against. Declared here so that it is pinned to a commit;
# NVRHI would otherwise fetch it by tag.
set(VULKAN_HEADERS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
set(VULKAN_HEADERS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(VULKAN_HEADERS_ENABLE_MODULE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(vulkan_headers
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
    GIT_TAG 015e25c3c91b70eb1a754d36fb14c4ba6ad9b0b9
    SYSTEM)
FetchContent_MakeAvailable(vulkan_headers)

# NVRHI: the rendering hardware interface, used with its Vulkan backend (MIT).
# NVRHI publishes no release tags; this is its main branch as of 2026-11-21.
set(NVRHI_WITH_VULKAN ON CACHE BOOL "" FORCE)
set(NVRHI_FETCH_VULKAN_HEADERS OFF CACHE BOOL "" FORCE)
set(NVRHI_WITH_VALIDATION ON CACHE BOOL "" FORCE)
set(NVRHI_WITH_DX11 OFF CACHE BOOL "" FORCE)
set(NVRHI_WITH_DX12 OFF CACHE BOOL "" FORCE)
set(NVRHI_WITH_NVAPI OFF CACHE BOOL "" FORCE)
set(NVRHI_WITH_RTXMU OFF CACHE BOOL "" FORCE)
set(NVRHI_WITH_AFTERMATH OFF CACHE BOOL "" FORCE)
set(NVRHI_BUILD_SHARED OFF CACHE BOOL "" FORCE)
set(NVRHI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NVRHI_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(nvrhi
    GIT_REPOSITORY https://github.com/NVIDIA-RTX/NVRHI.git
    GIT_TAG bf0bbd9b666a7297492beb1999a4ddc0dfded143
    SYSTEM)
FetchContent_MakeAvailable(nvrhi)
# NVRHI's Vulkan backend calls into its common library but does not say so. Linkers that resolve
# static libraries in command-line order (GNU ld) need to be told, or the symbols are not found.
target_link_libraries(nvrhi_vk PRIVATE nvrhi)

# glslang: compiles the engine's GLSL shaders to SPIR-V while the engine is built
# (BSD-3-Clause, with parts under Apache-2.0 and MIT). Release 16.6.0.
# It is linked into WhiteWolfShaderCompiler, a build tool, and not into the engine. glslang's own
# command-line compiler is not built, because building it needs Python.
# It is built with run-time type information, which it leaves out by default: the shader compiler
# derives from glslang's classes, and the undefined behaviour sanitizer checks such objects through
# their type information, which the library must then contain.
set(ENABLE_RTTI ON CACHE BOOL "" FORCE)
set(ENABLE_OPT OFF CACHE BOOL "" FORCE)
set(ENABLE_HLSL OFF CACHE BOOL "" FORCE)
set(ENABLE_GLSLANG_BINARIES OFF CACHE BOOL "" FORCE)
set(ENABLE_SPVREMAPPER OFF CACHE BOOL "" FORCE)
set(GLSLANG_TESTS OFF CACHE BOOL "" FORCE)
set(GLSLANG_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_EXTERNAL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glslang
    GIT_REPOSITORY https://github.com/KhronosGroup/glslang.git
    GIT_TAG e1b562a8bed273a02f30b59b66a5d499793cede5
    SYSTEM)
FetchContent_MakeAvailable(glslang)

# stb: reads PNG and JPEG images (stb_image.h) and writes PNG images (stb_image_write.h). Public
# domain, or MIT at the user's choice. stb publishes no releases; this is its master branch on
# 2026-11-21.
# Both are single-header libraries, which one source file of a program has to turn into code. That
# file is written here and compiled as a library of its own, so that stb is built as third-party
# code and not with the engine's warnings. The engine reads and writes files itself, so stb's own
# file functions are left out, as are the image formats the engine does not support.
FetchContent_Declare(stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG 6b5799bae8a1fe3792691ef4b72ae26434c3e799
    SYSTEM)
FetchContent_MakeAvailable(stb)

set(stb_implementation "${CMAKE_BINARY_DIR}/ThirdParty/StbImplementation.cpp")
file(CONFIGURE OUTPUT "${stb_implementation}" CONTENT
"// Written by CMake/WhiteWolfThirdParty.cmake.
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
" @ONLY)
add_library(stb STATIC "${stb_implementation}")
target_include_directories(stb SYSTEM PUBLIC "${stb_SOURCE_DIR}")
target_compile_definitions(stb PUBLIC STBI_NO_STDIO STBI_WRITE_NO_STDIO STBI_ONLY_PNG STBI_ONLY_JPEG)
set_target_properties(stb PROPERTIES POSITION_INDEPENDENT_CODE ON FOLDER "ThirdParty")