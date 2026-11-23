---
name: build-and-test
description: Build White Wolf Engine and run its tests. Use when building, running the whole test suite or one test, checking a change on every supported preset, or diagnosing a build or test failure.
---

# Build and test White Wolf Engine

All commands run from the repository root. On Windows use a **Developer PowerShell for VS** (2022 or 2026) with `git` on the `PATH`; the developer shell does not add it (see "Toolchain" in `AGENTS.md`).

## 1. Pick the presets for this machine

| Machine | Workflow presets to run |
| --- | --- |
| Windows with Visual Studio 2022 or 2026 | `windows-debug`, `windows-release` |
| Linux (development cross-check) | `linux-gcc-debug`, `linux-gcc-release`, `linux-clang-debug`, `linux-sanitize` |

Windows with MSVC is the product toolchain and the result that counts. The Linux presets only cross-check the code; they never replace a Windows run.

## 2. Build and test everything

Each command configures, builds and runs every test, and stops at the first failing step:

```
cmake --workflow --preset windows-debug
cmake --workflow --preset windows-release
```

On Linux, run all four:

```
cmake --workflow --preset linux-gcc-debug
cmake --workflow --preset linux-gcc-release
cmake --workflow --preset linux-clang-debug
cmake --workflow --preset linux-sanitize
```

Success is `100% tests passed` from every preset with no compiler warnings (warnings are errors). A test reported as `Skipped` is not a pass: read why and fix the cause. `CodeStyle.ClangFormat` is skipped only when clang-format 18 is missing.

## 3. Iterate faster

```
cmake --preset windows                    # configure once
cmake --build --preset windows-debug      # rebuild after edits
ctest --preset windows-debug              # run all tests
ctest --preset windows-debug -R EngineTests --output-on-failure
```

Run part of a test executable directly (doctest options):

```
Build\windows\Binaries\Debug\EngineTests.exe --list-test-suites
Build\windows\Binaries\Debug\EngineTests.exe --test-suite="FVersion"
Build\windows\Binaries\Debug\EngineTests.exe --test-case="*Parse*"
Build\windows\Binaries\Debug\EngineTests.exe --success          # also print passing assertions
```

On Linux the executables are at `Build/<configure preset>/Binaries/<Config>/EngineTests`.

New source files need no CMake edit. On Windows, the build that first notices a new file writes the Visual Studio projects again but may not compile the file in that same run: build a second time, or run `cmake --preset windows` before building.

## 4. Diagnose failures

| Symptom | Cause and fix |
| --- | --- |
| `unresolved external symbol` (MSVC) or `undefined reference` when linking a test or another module | The symbol is not exported. Add the module API macro (`ENGINE_API`) to its declaration. |
| A warning stops the build | Fix the code. Do not disable the warning; see "Warnings" in `AGENTS.md`. |
| `No such preset` | The preset is for the other operating system; see the table in step 1. |
| Configure fails with `could not find git` | `git` is not on the `PATH`. Install Git for Windows or add the copy bundled with Visual Studio; see "Toolchain" in `AGENTS.md`. |
| Configure fails while fetching a dependency | The first configure needs internet access to GitHub. Retry when the network is available; never vendor a copy by hand. |
| doctest fails to compile with `use of undefined type 'std::basic_ostream'` (MSVC) | A `CHECK` compares a type that doctest prints with `operator<<`, such as `std::string_view`, and the test file does not include `<ostream>`. Include it. |
| An error of the form `Header.h(12): error: ...` from the step "Generating reflection code" | `WhiteWolfHeaderTool` rejected a `WCLASS()`, `WPROPERTY()` or `WFUNCTION()` declaration. The message says what to change; the rules are in "Objects and garbage collection" in `AGENTS.md`. |
| A compile error inside `<Module>.generated.cpp` | A reflected member has a type that cannot be reflected, or the header of a class from another module is not included. Fix the declaration in the header; never edit the generated file. |
| Renderer tests fail with "Vulkan is not installed" or "no graphics card on this machine can be used" | The machine has no Vulkan 1.3. Install a graphics driver, or a software implementation: on Linux `sudo apt-get install libvulkan1 mesa-vulkan-drivers` (Lavapipe). |
| `Detected memory leaks!` only when a test uses the render device | A `thread_local` object with a destructor exists in the test executable or the engine. Graphics drivers start threads that never exit cleanly, and each gets its own copy that is never destroyed. Remove the `thread_local`; see D-026. |
| `CodeStyle.ClangFormat` fails | Run `cmake -P Tools/Format.cmake`, review the changes, rebuild. |
| A sanitizer report under `linux-sanitize` | A real memory, leak or undefined-behaviour bug. Fix it; never suppress it. |
| A LeakSanitizer report whose stack frames are in `(<unknown module>)` | The memory was allocated by a shared library that had been unloaded by the time the report was written, which hides the library's name and makes its memory look leaked. Do not unload such a library while the process goes on; this is why Vulkan stays loaded (D-044). The annotations of the failed CI job show the first frames of each stack. |
| `RendererTestSceneTests.cpp` fails with a difference from the reference image | Open `Build/<preset>/TestOutput/RendererTestScene.png` and compare it with `Engine/Tests/Rendering/ReferenceImages/RendererTestScene.png`. Fix the renderer if the new picture is wrong. Only if the change was meant to alter the picture, copy the new one over the reference and commit both together. |
| `EditorUiTests.cpp` fails with a difference from the reference image | The same for the editor: `Build/<preset>/TestOutput/EditorDefaultLayout.png` against `Editor/Tests/Ui/ReferenceImages/EditorDefaultLayout.png`. A panel that was added, moved or restyled is meant to alter the picture; anything else is a regression of the layout. |
| A fatal error that begins `An assertion of Dear ImGui failed` | The editor's user interface used the library wrongly: a window, menu or tree row that was begun and not ended, or the other way round. The message names the library's file and line; the mistake is in `Editor/Private/Ui`. |
| Stale or confusing build state | Delete `Build/<configure preset>/` and run the workflow preset again. |

## 5. Report honestly

State exactly which presets you ran and their results. If MSVC was not available, say that the change is unverified on the product toolchain until the Windows CI job passes.
