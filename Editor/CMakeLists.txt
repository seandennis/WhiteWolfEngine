# The editor module: WhiteWolfEditorCore.dll.
#
# Everything the editor can do lives in this module, as commands that need no window (see
# docs/DECISIONS.md, D-040), so that the unit tests and the agent interface drive the same code
# as the editor's user interface.

ww_add_module(Editor
    OUTPUT_NAME WhiteWolfEditorCore
    PUBLIC_DEPENDENCIES Engine)

ww_add_module_tests(Editor)
if(TARGET EditorTests)
    # The tests borrow the engine tests' FScopedLogCapture and FScopedTempDirectory, which are
    # headers without dependencies.
    target_include_directories(EditorTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/Tests")
endif()
