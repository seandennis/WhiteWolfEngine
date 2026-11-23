# The core engine module: WhiteWolfEngine.dll.

find_package(Threads REQUIRED)

ww_add_module(Engine
    OUTPUT_NAME WhiteWolfEngine
    PUBLIC_DEPENDENCIES Threads::Threads glm::glm-header-only
    PRIVATE_DEPENDENCIES glfw nvrhi nvrhi_vk Vulkan::Headers stb cgltf ${CMAKE_DL_LIBS})

# Where a user's own directories are is asked of the shell of Windows (FUserDirectories): the function is in
# shell32, what frees its answer in ole32, and the identifiers of the directories in uuid.
if(WIN32)
    target_link_libraries(Engine PRIVATE shell32 ole32 uuid)
endif()

# The engine's shaders are compiled while it is built and become part of the DLL.
ww_add_shaders(Engine ROOT "${CMAKE_CURRENT_SOURCE_DIR}/Shaders")

# The engine version comes from the top-level project() call and nowhere else.
set_source_files_properties(Private/Core/EngineVersion.cpp PROPERTIES
    COMPILE_DEFINITIONS
        "WW_ENGINE_VERSION_MAJOR=${PROJECT_VERSION_MAJOR};WW_ENGINE_VERSION_MINOR=${PROJECT_VERSION_MINOR};WW_ENGINE_VERSION_PATCH=${PROJECT_VERSION_PATCH}")

ww_add_module_tests(Engine)
if(TARGET EngineTests)
    set_source_files_properties(Tests/Core/EngineVersionTests.cpp PROPERTIES
        COMPILE_DEFINITIONS
            "WW_EXPECTED_VERSION_MAJOR=${PROJECT_VERSION_MAJOR};WW_EXPECTED_VERSION_MINOR=${PROJECT_VERSION_MINOR};WW_EXPECTED_VERSION_PATCH=${PROJECT_VERSION_PATCH}")

    # The renderer's test scene is compared with reference images kept beside the tests. A picture
    # that does not match is written to the build directory, never into the repository.
    set_source_files_properties(Tests/Rendering/RendererTestSceneTests.cpp PROPERTIES
        COMPILE_DEFINITIONS
            "WW_REFERENCE_IMAGE_DIRECTORY=\"${CMAKE_CURRENT_SOURCE_DIR}/Tests/Rendering/ReferenceImages\";WW_TEST_OUTPUT_DIRECTORY=\"${CMAKE_BINARY_DIR}/TestOutput\"")

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
