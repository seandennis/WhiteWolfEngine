# The editor module: WhiteWolfEditorCore.dll.
#
# Everything the editor can do lives in this module, as commands that need no window (see
# docs/DECISIONS.md, D-040), so that the unit tests and the agent interface drive the same code
# as the editor's user interface.

ww_add_module(Editor
    OUTPUT_NAME WhiteWolfEditorCore
    PUBLIC_DEPENDENCIES Engine
    PRIVATE_DEPENDENCIES imgui)

# The editor's executable is an entry point and nothing more.
add_executable(WhiteWolfEditor Main.cpp)
ww_apply_compiler_settings(WhiteWolfEditor)
target_link_libraries(WhiteWolfEditor PRIVATE Editor)
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES Main.cpp)

ww_add_module_tests(Editor)
if(TARGET EditorTests)
    # The tests borrow the engine tests' FScopedLogCapture and FScopedTempDirectory, which are
    # headers without dependencies.
    target_include_directories(EditorTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/Tests")

    # Pictures of the user interface are compared with reference images kept beside the tests. A
    # picture that does not match is written to the build directory, never into the repository.
    set_source_files_properties(Tests/Ui/EditorUiTests.cpp PROPERTIES
        COMPILE_DEFINITIONS
            "WW_REFERENCE_IMAGE_DIRECTORY=\"${CMAKE_CURRENT_SOURCE_DIR}/Tests/Ui/ReferenceImages\";WW_TEST_OUTPUT_DIRECTORY=\"${CMAKE_BINARY_DIR}/TestOutput\"")

    # Starts the executable as an agent would, with messages on its standard input, and checks
    # what it writes to its standard output. The unit tests cover what the server answers; this
    # covers the wiring of the process: the command line, the streams and a clean exit.
    add_test(NAME WhiteWolfEditor.ServesAnAgent
        COMMAND "${CMAKE_COMMAND}"
            "-DEDITOR=$<TARGET_FILE:WhiteWolfEditor>"
            "-DMESSAGES=${CMAKE_CURRENT_SOURCE_DIR}/Tests/Agent/AgentSession.jsonl"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/Tests/Agent/RunAgentSession.cmake")
    set_tests_properties(WhiteWolfEditor.ServesAnAgent PROPERTIES
        FAIL_REGULAR_EXPRESSION "Detected memory leaks!")
endif()
