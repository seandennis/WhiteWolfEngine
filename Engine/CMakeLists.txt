# The core engine module: WhiteWolfEngine.dll.

find_package(Threads REQUIRED)

ww_add_module(Engine
    OUTPUT_NAME WhiteWolfEngine
    PUBLIC_DEPENDENCIES Threads::Threads)

# The engine version comes from the top-level project() call and nowhere else.
set_source_files_properties(Private/Core/EngineVersion.cpp PROPERTIES
    COMPILE_DEFINITIONS
        "WW_ENGINE_VERSION_MAJOR=${PROJECT_VERSION_MAJOR};WW_ENGINE_VERSION_MINOR=${PROJECT_VERSION_MINOR};WW_ENGINE_VERSION_PATCH=${PROJECT_VERSION_PATCH}")

ww_add_module_tests(Engine)
if(TARGET EngineTests)
    set_source_files_properties(Tests/Core/EngineVersionTests.cpp PROPERTIES
        COMPILE_DEFINITIONS
            "WW_EXPECTED_VERSION_MAJOR=${PROJECT_VERSION_MAJOR};WW_EXPECTED_VERSION_MINOR=${PROJECT_VERSION_MINOR};WW_EXPECTED_VERSION_PATCH=${PROJECT_VERSION_PATCH}")
endif()
