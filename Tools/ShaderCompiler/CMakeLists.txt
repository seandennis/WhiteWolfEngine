# WhiteWolfShaderCompiler: compiles a module's GLSL shaders to SPIR-V with the glslang library and
# writes a source file that embeds them in the module (see ww_add_shaders in
# CMake/WhiteWolfModule.cmake, and D-027 in docs/DECISIONS.md).
#
# Like the header tool it cannot link to the engine, because the engine is built with it. It uses
# the engine's fixed-width integer types (Core/CoreTypes.h, a header without dependencies) and
# nothing else.

file(GLOB_RECURSE shader_compiler_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Private/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Private/*.h")

# Everything but main() is a library, so that the unit tests can link to it.
add_library(ShaderCompilerLibrary STATIC ${shader_compiler_sources})
ww_apply_compiler_settings(ShaderCompilerLibrary)
target_include_directories(ShaderCompilerLibrary PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Private"
    "${PROJECT_SOURCE_DIR}/Engine/Public")
target_link_libraries(ShaderCompilerLibrary
    PUBLIC ToolSupportLibrary
    PRIVATE glslang::glslang glslang::glslang-default-resource-limits)
set_target_properties(ShaderCompilerLibrary PROPERTIES FOLDER "Tools")
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${shader_compiler_sources})

add_executable(WhiteWolfShaderCompiler Main.cpp)
ww_apply_compiler_settings(WhiteWolfShaderCompiler)
target_link_libraries(WhiteWolfShaderCompiler PRIVATE ShaderCompilerLibrary)
set_target_properties(WhiteWolfShaderCompiler PROPERTIES FOLDER "Tools")

# The tests link to the engine only for FProcessDiagnostics, which every test executable calls.
# They also borrow the engine tests' FScopedTempDirectory, a header without dependencies.
ww_add_test_executable(ShaderCompilerTests DEPENDENCIES ShaderCompilerLibrary Engine)
if(TARGET ShaderCompilerTests)
    target_include_directories(ShaderCompilerTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/Tests")
endif()
