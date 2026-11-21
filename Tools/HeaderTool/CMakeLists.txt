# WhiteWolfHeaderTool: generates the code that registers reflected classes, from the WCLASS(),
# WPROPERTY() and WFUNCTION() declarations in a module's headers. The build runs it for every
# module (see ww_generate_reflection in CMake/WhiteWolfModule.cmake).
#
# The tool cannot link to the engine, because the engine is built with it. It uses the engine's
# fixed-width integer types (Core/CoreTypes.h, a header without dependencies) and nothing else.

file(GLOB_RECURSE header_tool_sources CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Private/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Private/*.h")

# Everything but main() is a library, so that the unit tests can link to it.
add_library(HeaderToolLibrary STATIC ${header_tool_sources})
ww_apply_compiler_settings(HeaderToolLibrary)
target_include_directories(HeaderToolLibrary PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/Private"
    "${PROJECT_SOURCE_DIR}/Engine/Public")
set_target_properties(HeaderToolLibrary PROPERTIES FOLDER "Tools")
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${header_tool_sources})

add_executable(WhiteWolfHeaderTool Main.cpp)
ww_apply_compiler_settings(WhiteWolfHeaderTool)
target_link_libraries(WhiteWolfHeaderTool PRIVATE HeaderToolLibrary)
set_target_properties(WhiteWolfHeaderTool PROPERTIES FOLDER "Tools")

# The tests link to the engine only for FProcessDiagnostics, which every test executable calls.
# They also borrow the engine tests' FScopedTempDirectory, a header without dependencies.
ww_add_test_executable(HeaderToolTests DEPENDENCIES HeaderToolLibrary Engine)
if(TARGET HeaderToolTests)
    target_include_directories(HeaderToolTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/Tests")
endif()
