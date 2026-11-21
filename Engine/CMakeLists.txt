# The core engine module: WhiteWolfEngine.dll.

find_package(Threads REQUIRED)

ww_add_module(Engine
    OUTPUT_NAME WhiteWolfEngine
    PUBLIC_DEPENDENCIES Threads::Threads glm::glm-header-only)

# The engine version comes from the top-level project() call and nowhere else.
set_source_files_properties(Private/Core/EngineVersion.cpp PROPERTIES
    COMPILE_DEFINITIONS
        "WW_ENGINE_VERSION_MAJOR=${PROJECT_VERSION_MAJOR};WW_ENGINE_VERSION_MINOR=${PROJECT_VERSION_MINOR};WW_ENGINE_VERSION_PATCH=${PROJECT_VERSION_PATCH}")

ww_add_module_tests(Engine)
if(TARGET EngineTests)
    set_source_files_properties(Tests/Core/EngineVersionTests.cpp PROPERTIES
        COMPILE_DEFINITIONS
            "WW_EXPECTED_VERSION_MAJOR=${PROJECT_VERSION_MAJOR};WW_EXPECTED_VERSION_MINOR=${PROJECT_VERSION_MINOR};WW_EXPECTED_VERSION_PATCH=${PROJECT_VERSION_PATCH}")

    # Proves that leak detection is switched on, by leaking on purpose and requiring the report.
    # Registered only where a leak can be detected: MSVC Debug builds (the debug heap of the C
    # runtime) and GCC or Clang builds with AddressSanitizer (LeakSanitizer).
    set(leak_probe_command EngineTests "--test-case=Deliberately leaks memory" --no-skip)
    set(leak_report_pattern "Detected memory leaks!|LeakSanitizer: detected memory leaks")
    if(MSVC)
        add_test(NAME EngineTests.DetectsMemoryLeaks COMMAND ${leak_probe_command} CONFIGURATIONS Debug)
        set_tests_properties(EngineTests.DetectsMemoryLeaks PROPERTIES
            PASS_REGULAR_EXPRESSION "${leak_report_pattern}")
    elseif("address" IN_LIST WW_SANITIZERS)
        add_test(NAME EngineTests.DetectsMemoryLeaks COMMAND ${leak_probe_command})
        set_tests_properties(EngineTests.DetectsMemoryLeaks PROPERTIES
            PASS_REGULAR_EXPRESSION "${leak_report_pattern}")
    endif()
endif()
