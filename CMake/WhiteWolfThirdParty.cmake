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

# stb: reads PNG, JPEG and Radiance HDR images (stb_image.h) and writes PNG and Radiance HDR
# images (stb_image_write.h). Public
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
target_compile_definitions(stb PUBLIC
    STBI_NO_STDIO STBI_WRITE_NO_STDIO STBI_ONLY_PNG STBI_ONLY_JPEG STBI_ONLY_HDR)
set_target_properties(stb PROPERTIES POSITION_INDEPENDENT_CODE ON FOLDER "ThirdParty")

# cgltf: reads glTF 2.0 files, both .gltf and .glb (MIT). Release v1.15.
# A single-header library, built as a library of its own like stb above.
FetchContent_Declare(cgltf
    GIT_REPOSITORY https://github.com/jkuhlmann/cgltf.git
    GIT_TAG 360db1a95480fe102ae9c69b27c5d101167ff5ba
    SYSTEM)
FetchContent_MakeAvailable(cgltf)

set(cgltf_implementation "${CMAKE_BINARY_DIR}/ThirdParty/CgltfImplementation.cpp")
file(CONFIGURE OUTPUT "${cgltf_implementation}" CONTENT
"// Written by CMake/WhiteWolfThirdParty.cmake.
#define CGLTF_IMPLEMENTATION
#include <cgltf.h>
" @ONLY)
add_library(cgltf STATIC "${cgltf_implementation}")
target_include_directories(cgltf SYSTEM PUBLIC "${cgltf_SOURCE_DIR}")
set_target_properties(cgltf PROPERTIES POSITION_INDEPENDENT_CODE ON FOLDER "ThirdParty")

# Dear ImGui: lays out the editor's user interface (MIT). Release v1.93.0-docking, from the
# library's docking branch, which has the dockable windows the editor is made of.
# The library comes without a build script. Its sources are compiled here as a library of its own,
# so that they are built as third-party code and not with the engine's warnings. It is linked into
# the editor module only, and no Dear ImGui type appears in a header of that module.
# The header written here configures the library; Dear ImGui includes it wherever it is used:
#  - A failed assertion of the library is reported like a failed check of the engine, in every
#    build configuration. The editor module defines the function.
#  - The library reaches the clipboard, files and other programs only through what the editor
#    gives it, so that the editor behaves the same with a window and without one, and a test
#    cannot touch the clipboard or the files of the machine it runs on.
#  - Functions that the library has replaced with newer ones are left out, so that the editor
#    cannot come to depend on them, and so are the demonstration windows.
#  - Characters beyond U+FFFF are text like any other, and indices are 32 bits wide, as in the
#    engine's FUiDrawList.
# misc/cpp/imgui_stdlib.cpp is the library's own text fields for std::string, which grow with what
# is typed into them.
FetchContent_Declare(imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG b916186e66aa3c02b72c6ca17455516512ed8049
    SYSTEM)
FetchContent_MakeAvailable(imgui)

set(imgui_configuration "${CMAKE_BINARY_DIR}/ThirdParty/WhiteWolfImGuiConfiguration.h")
file(CONFIGURE OUTPUT "${imgui_configuration}" CONTENT
"// Written by CMake/WhiteWolfThirdParty.cmake.
#pragma once

[[noreturn]] void ReportImGuiAssertionFailure(const char* Expression, const char* File, int Line);
#define IM_ASSERT(Expression) \\
	((Expression) ? static_cast<void>(0) : ReportImGuiAssertionFailure(#Expression, __FILE__, __LINE__))

#define IMGUI_DISABLE_WIN32_FUNCTIONS
#define IMGUI_DISABLE_FILE_FUNCTIONS
#define IMGUI_DISABLE_DEFAULT_SHELL_FUNCTIONS
#define IMGUI_DISABLE_OBSOLETE_FUNCTIONS
#define IMGUI_DISABLE_DEMO_WINDOWS
#define IMGUI_USE_WCHAR32
#define ImDrawIdx unsigned int
" @ONLY)
add_library(imgui STATIC
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_demo.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/misc/cpp/imgui_stdlib.cpp")
target_include_directories(imgui SYSTEM PUBLIC "${imgui_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/ThirdParty")
target_compile_definitions(imgui PUBLIC "IMGUI_USER_CONFIG=\"WhiteWolfImGuiConfiguration.h\"")
set_target_properties(imgui PROPERTIES POSITION_INDEPENDENT_CODE ON FOLDER "ThirdParty")