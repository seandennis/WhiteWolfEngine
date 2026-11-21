# ToolSupportLibrary: file operations shared by the build tools (WhiteWolfHeaderTool and
# WhiteWolfShaderCompiler). The tools run while the engine is being built, so they cannot link to
# it. Its behaviour is tested through the tools that use it.

file(GLOB_RECURSE tool_support_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Private/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Private/*.h")

add_library(ToolSupportLibrary STATIC ${tool_support_sources})
ww_apply_compiler_settings(ToolSupportLibrary)
target_include_directories(ToolSupportLibrary PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/Private")
set_target_properties(ToolSupportLibrary PROPERTIES FOLDER "Tools")
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${tool_support_sources})
