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
