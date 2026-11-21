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
