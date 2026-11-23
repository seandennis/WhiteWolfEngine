# White Wolf Engine: guide for agents

White Wolf Engine is a simple, production-grade 3D game engine for Windows, written in C++20.
This file is the working agreement for every agent (and human) who changes this repository.

- **What to build** is defined by [`WhiteWolfEngine.md`](WhiteWolfEngine.md), the project specification. It wins over everything else.
- **Decisions already made** are recorded in [`docs/DECISIONS.md`](docs/DECISIONS.md). Read it before changing architecture, tooling or dependencies.
- **Where the project stands** is the milestone table in [`README.md`](README.md).

## Ground rules

1. Work autonomously. Ask the owner only when a decision is irreversible or blocks all progress; otherwise make the call and record it in `docs/DECISIONS.md`.
2. Stay inside this repository. Do not read, search or copy code from anywhere else on the machine.
3. Work in milestones, in the order given by the specification. A milestone must build, pass every test and be committed before the next one starts.
4. Production-grade only: no hacks, no shortcuts, no `TODO` placeholders, no dead code. If simplicity and feature count conflict, cut the feature, never the quality.
5. Every feature ships with unit tests. All tests run with `ctest` and must pass before any commit.
6. Never commit a broken build. Run the [pre-commit checklist](#pre-commit-review-checklist) before every commit.
7. Keep `AGENTS.md`, `README.md`, `docs/DECISIONS.md` and the skills in `.claude/skills/` accurate in the same commit that changes what they describe.

## Toolchain

| Purpose | Tool |
| --- | --- |
| Product compiler | MSVC, Visual Studio 2022 (17.6 or later) or Visual Studio 2026 ("Desktop development with C++" workload), x64. See D-019 |
| Build system | CMake 3.25 or later (bundled with Visual Studio), driven through `CMakePresets.json` |
| Source control | Git, on the `PATH`. CMake also uses it to fetch the pinned third-party sources |
| Graphics | Vulkan 1.3, from the driver of a graphics card or from a software implementation such as Lavapipe. Needed to run the renderer and its tests, not to build. No Vulkan SDK is required |
| Formatter | clang-format 18 (exactly major version 18) |
| Development cross-checks | GCC 13+, Clang 18+ and Ninja on Linux. Not a shipping platform; see D-004 |

On Windows, run every command below from a **Developer PowerShell for VS** (2022 or 2026) so that `cmake` and `ctest` are on the `PATH`. The developer shell does not add `git`: install Git for Windows, or add the copy bundled with Visual Studio (`Common7\IDE\CommonExtensions\Microsoft\TeamFoundation\Team Explorer\Git\cmd` under the Visual Studio installation) to the `PATH`. The first configure needs internet access to fetch the pinned third-party sources.

The `windows` preset names no generator, so CMake uses the newest Visual Studio installed on the machine.

## Build commands

One command configures, builds and runs every test:

```
cmake --workflow --preset windows-debug
cmake --workflow --preset windows-release
```

The same steps individually, for incremental work:

```
cmake --preset windows                    # configure (generates the Visual Studio solution in Build/windows/)
cmake --build --preset windows-debug      # build Debug   (or windows-release)
ctest --preset windows-debug              # run all tests (or windows-release)
```

Development cross-checks on Linux use the same pattern with these presets:

| Workflow preset | What it checks |
| --- | --- |
| `linux-gcc-debug`, `linux-gcc-release` | GCC with warnings as errors |
| `linux-clang-debug` | Clang with warnings as errors |
| `linux-sanitize` | GCC with AddressSanitizer, LeakSanitizer and UBSan; any report fails the test run |

Build output goes to `Build/<configure preset>/`. Executables and DLLs are in `Build/<configure preset>/Binaries/<Config>/`. Nothing under `Build/` is committed.

The editor is `Build\windows\Binaries\Debug\WhiteWolfEditor.exe` (or `Release`). Started without an option it opens its window and asks which project to work on. `WhiteWolfEditor --screenshot Editor.png` draws the same window without opening one, on a machine with or without a display, and saves the picture: use it to look at what the editor shows. The picture is of the editor before a project is opened, without the window that asks for one.

CMake options:

| Option | Default | Meaning |
| --- | --- | --- |
| `WW_BUILD_TESTS` | `ON` | Build the unit tests and register them with CTest |
| `WW_SANITIZERS` | empty | Sanitizers for GCC/Clang development builds, for example `address;undefined` |

## Test commands

```
ctest --preset windows-debug                                  # everything, the only gate that counts
ctest --preset windows-debug -R EngineTests                   # one CTest entry
Build\windows\Binaries\Debug\EngineTests.exe --list-test-cases
Build\windows\Binaries\Debug\EngineTests.exe --test-suite="FVersion"
Build\windows\Binaries\Debug\EngineTests.exe --test-case="*Parse*"
```

- Tests use [doctest](https://github.com/doctest/doctest). Each module has one test executable, `<Module>Tests`, built from every file under `<Module>/Tests/`, and one CTest entry of the same name: `EngineTests` and `EditorTests`. The build tools have their own: `HeaderToolTests` and `ShaderCompilerTests`.
- Tests are headless: they never open a window, wait for input or need a display. The renderer tests do need Vulkan 1.3: they create an `FRenderDevice`, draw to an `FRenderTarget` and read the pixels back, on whatever graphics card or software driver the machine has. A machine without Vulkan fails them; it does not skip them.
- Test executables are built with `DOCTEST_CONFIG_NO_MULTITHREADING`: call doctest assertions (`CHECK`, `REQUIRE`) on the main thread only. A test may start threads that run engine code. A test that needs a window creates an `FScopedHeadlessPlatform` (`Tests/Platform/ScopedHeadlessPlatform.h`), under which `FWindow::Create` makes windows that exist in memory only, and feeds input through `Window->GetInput().HandleKeyEvent(...)`.
- `WhiteWolfEditor.ServesAnAgent` is a CTest entry that starts the editor's executable as an agent does, gives it the messages of `Editor/Tests/Agent/AgentSession.jsonl` on its standard input and checks its answers (`RunAgentSession.cmake`).
- `EngineTests.DetectsMemoryLeaks` is a CTest entry that leaks memory on purpose and passes only if the leak is reported. It exists in MSVC Debug builds and under `linux-sanitize`, the builds that can detect leaked allocations.
- A test executable's `main` starts with `FProcessDiagnostics::DisableErrorDialogs()` and `FProcessDiagnostics::EnableMemoryLeakReport()`, so a crash cannot hang the run in a dialog and leaks are reported at exit.
- `CodeStyle.ClangFormat` is a CTest entry that runs the format check. It reports **Skipped** (not Passed) when clang-format 18 is not installed; install it rather than committing unchecked code.
- Test files mirror the source tree and are named `<Subject>Tests.cpp`, for example `Engine/Tests/Core/VersionTests.cpp` tests `Engine/Public/Core/Version.h`.
- Group the cases for a type in `TEST_SUITE("<TypeName>")`. Name each `TEST_CASE` as a sentence that states the expected behaviour.
- Test the contract, including every failure path: malformed input, boundaries, overflow, empty input.
- To test logging or a fatal error, create an `FScopedLogCapture` (`TestSupport/ScopedLogCapture.h`). While it is alive, log messages are recorded and a Fatal message or failed `WW_CHECK` throws `FFatalErrorException` instead of terminating: `CHECK_THROWS_AS(DoBadThing(), FFatalErrorException);`.
- A test that loads assets creates an `FScopedContentDirectory` (`Tests/Assets/TestAssets.h`), a temporary content directory with functions that write a picture, an environment or a glTF file into it. Count warnings with `CountLoggedProblems`, not all log messages: loading a file logs a line.
- Tests that write files use an `FScopedTempDirectory` (`TestSupport/ScopedTempDirectory.h`) and never write into the repository. Nor do they read or write what belongs to whoever runs them: an `FEditor` keeps what it remembers in a file only if it is given one (`SetSettingsFile`), and a test gives it none, or one in a temporary directory.
- **Reference images.** `Tests/Rendering/RendererTestSceneTests.cpp` draws the renderer's test scene (`Tests/Rendering/RendererTestScene.h`) and compares it with `Engine/Tests/Rendering/ReferenceImages/RendererTestScene.png`, allowing the small differences between graphics cards (D-034). When the picture does not match, the test fails and saves what it drew as `Build/<preset>/TestOutput/RendererTestScene.png`. Look at that picture. If the renderer is wrong, fix the renderer. If the picture is right, because a change was meant to alter how the scene looks, copy it over the reference image and commit it with the change, and say in the commit message why the picture changed. The same file proves that the picture depends on the sky light, on shadows and on ambient occlusion, by drawing it without each (D-039): when the renderer learns something that the scene should show, add it to the scene and to that test.

## Directory layout

```
WhiteWolfEngine.md        Project specification (source of truth)
AGENTS.md                 This guide
CLAUDE.md                 Imports AGENTS.md for Claude Code
README.md                 Overview, build instructions, milestone status
CMakeLists.txt            Top-level build script; the engine version is set here
CMakePresets.json         Configure, build, test and workflow presets
.clang-format             Formatting rules (clang-format 18)
.editorconfig             Editor whitespace settings
.github/workflows/ci.yml  Continuous integration: Windows MSVC, Linux cross-checks, formatting
.claude/skills/           Step-by-step procedures for common development tasks
CMake/                    Build helpers
  WhiteWolfCompilerSettings.cmake   Warning levels, conformance flags, sanitizers
  WhiteWolfModule.cmake             ww_add_module(), ww_add_module_tests(), ww_generate_reflection(), ww_add_shaders()
  WhiteWolfThirdParty.cmake         Pinned engine and editor dependencies (glm, GLFW, Vulkan-Headers, NVRHI, glslang,
                                    stb, cgltf, Dear ImGui)
  WhiteWolfThirdPartyTesting.cmake  Pinned test-only dependencies (doctest)
Engine/                   The core engine module (WhiteWolfEngine.dll)
  CMakeLists.txt
  Public/                 Headers other modules may include
    CoreMinimal.h         Included first by every engine header: core types, assertions, logging
    Core/                 Fundamental types and assertions: CoreTypes.h, Assert.h, Version.h, EngineVersion.h,
                          EnumClassFlags.h (WW_ENUM_CLASS_FLAGS), FileHelper.h (FFileHelper: whole text files),
                          ProcessDiagnostics.h (error dialogs, memory leak report), Utf8.h (FUtf8: characters as UTF-8)
    Assets/               Assets and their names: Asset.h (WAsset), AssetPath.h (FAssetPath), AssetPackage.h (WAssetPackage),
                          AssetManager.h (FAssetManager: loads the files of the content directory)
    Input/                Keyboard and mouse: InputState.h (FInputState), Key.h (EKey), MouseButton.h (EMouseButton)
    Json/                 JSON text: JsonValue.h (FJsonValue), JsonReader.h (FJsonReader), JsonWriter.h (FJsonWriter)
    Logging/              The engine log: Log.h (FLog, WW_LOG), categories, messages and sinks
    Math/                 Math types: Vector.h (FVector, FVector3f, FVector2f, FVector2D), Quat.h (FQuat), Transform.h (FTransform),
                          Rotator.h (FRotator: a rotation as angles), LinearColor.h (FLinearColor), Color.h (FColor),
                          Srgb.h (FSrgb: the encoding of colours), Box.h (FBox, FBox3f: the room that points
                          take up), Matrix.h (FMatrix4f), Ray.h (FRay), Intersection.h (FIntersection: where a ray
                          meets a triangle or a box)
    Object/               The object system: Object.h (WObject), Class.h (WClass), ObjectPtr.h (TObjectPtr),
                          WeakObjectPtr.h (TWeakObjectPtr), ObjectGlobals.h (NewObject, Cast, IsValid,
                          CollectGarbage), GarbageCollector.h (FGarbageCollector), ObjectMacros.h (class
                          registration)
    Platform/             The window system: Platform.h (FPlatform), Window.h (FWindow). The directories that the
                          system keeps for a user, for settings and for documents: UserDirectories.h (FUserDirectories)
    Rendering/            The renderer: RenderDevice.h (FRenderDevice), RenderTarget.h (FRenderTarget), Image.h (FImage),
                          HdrImage.h (FHdrImage), ImageFile.h (FImageFile: PNG, JPEG and Radiance HDR files),
                          ImageDifference.h (FImageDifference),
                          Texture.h (WTexture), MipChain.h (FMipChain), MeshShapes.h (FMeshShapes: box, sphere, plane),
                          EnvironmentMap.h (WEnvironmentMap), SphericalHarmonics.h (FSphericalHarmonics),
                          SkyLightComponent.h (WSkyLightComponent), ShadowCascades.h (FShadowCascades),
                          CameraView.h (FCameraView), StaticMesh.h (WStaticMesh), Material.h (WMaterial),
                          StaticMeshComponent.h (WStaticMeshComponent), DirectionalLightComponent.h,
                          PointLightComponent.h, SceneRenderer.h (FSceneRenderer), SceneViewMode.h,
                          GltfImporter.h (FGltfImporter: .gltf and .glb files),
                          SceneRayCast.h (FSceneRayCast: what a ray through a scene meets first), SceneRayHit.h,
                          UiRenderer.h (FUiRenderer), UiDrawList.h, UiDrawCommand.h, UiVertex.h (user interfaces),
                          SwapChain.h (FSwapChain: shows a render target in a window)
    Reflection/           Reflected properties: Property.h (FProperty), ValueProperty.h and PropertyValueTraits.h
                          (bool, numbers, strings, math types), ObjectProperty.h, WeakObjectProperty.h,
                          ArrayProperty.h, PropertyFactory.h (MakeProperty), PropertyJsonContext.h.
                          Reflected functions: Function.h (FFunction), FunctionFactory.h (MakeFunction)
    Scene/                Scenes: Scene.h (WScene), Entity.h (WEntity), Component.h (WComponent),
                          SceneSerializer.h (FSceneSerializer: scene files), MissingComponent.h
  Private/                Sources and module-internal headers, mirroring Public/
  Shaders/                GLSL shaders (*.vert, *.frag) and the files they include (*.glsl), compiled into the DLL
  Tests/                  Unit tests, mirroring Public/ (EngineTests executable)
    TestSupport/          Helpers shared by tests: FScopedLogCapture, FScopedTempDirectory
    Object/TestObjects.h  WObject classes used by the object system tests, and FScopedObjectTest
    Rendering/TestRendering.h  Helpers for tests that draw: a validated device, reading a target back, checking the log
    Rendering/TestSceneRendering.h  Helpers for tests that draw scenes: FSceneRenderTest, squares, colours
    Rendering/TestReferenceImages.h Compares a picture with a reference image (CheckAgainstReferenceImage)
    Rendering/TestSceneLighting.h   The lighting model written out again in C++, and a lit test scene
    Rendering/TestGltfFiles.h       Builds small .gltf and .glb files for the importer's tests
    Assets/TestAssets.h             A content directory for one test, and files to put into it
    Rendering/RendererTestScene.h   The renderer's test scene, which is compared with a reference image
    Rendering/ReferenceImages/      The reference images
Editor/                   The editor module (WhiteWolfEditorCore.dll): everything the editor does, and its user interface
  CMakeLists.txt
  Main.cpp                Entry point of the editor's executable, WhiteWolfEditor
  Public/
    Editor.h              FEditor: the scene being edited, what is selected in it, from where it is looked at and the
                          commands that change it
    Commands/             The command system: CommandRegistry.h (FCommandRegistry, FCommandFunction),
                          CommandDescription.h, CommandParameter.h, CommandArgumentType.h (ECommandArgumentType),
                          CommandEffect.h (ECommandEffect), CommandArguments.h (FCommandArguments),
                          CommandResult.h (FCommandResult)
    History/              UndoHistory.h (FUndoHistory: the states a scene has been in, for undo and redo),
                          ScopedEditorStep.h (FScopedEditorStep: several commands as one step)
    Project/              Project.h (FProject: a game project, a directory with a project file and a Content directory)
    Settings/             EditorSettings.h (FEditorSettings: what the editor remembers from one run to the next)
    Viewport/             ViewportCamera.h (FViewportCamera: the point of view of the editor's viewport). The gizmo,
                          without a window to be drawn in: GizmoSettings.h (FGizmoSettings, with GizmoMode.h and
                          GizmoSpace.h), TransformGizmo.h (FTransformGizmo: where its handles are in a picture,
                          with GizmoHandle.h and GizmoShape.h), GizmoDrag.h (FGizmoDrag: what dragging a handle
                          does)
    Ui/                   EditorUi.h (FEditorUi: the user interface, laid out for a surface and drawn onto it),
                          UiRectangle.h (FUiRectangle), UiText.h (FUiText: names of the code as words, search,
                          text that is too long for its place)
    Agent/                AgentServer.h (FAgentServer: the commands as tools of a Model Context Protocol server)
    EditorWindow.h        FEditorWindow: the editor's main window, with the user interface drawn into it every frame;
                          EditorWindowDescription.h
    EditorApplication.h   FEditorApplication: what the executable does, given a command line and streams
  Private/
    Commands/             The editor's own commands, one class per area (FSceneCommands, FEntityCommands,
                          FComponentCommands, FPropertyCommands, FEditCommands, FProjectCommands, FAssetCommands,
                          FSelectionCommands, FViewportCommands, FEditorCommands), and what they share
    Ui/                   The user interface on Dear ImGui: ImGuiLibrary.h (the one include of the library),
                          FUiInput, FUiTextures and FUiDrawData (what goes into the library and what comes out),
                          FUiTheme, FUiItems (named items), FUiCommands (the commands widgets give), FUiRequests
                          (what one part asks of another), FUiIcons (buttons with a sign), FMainMenuBar,
                          FMainToolbar (with FUiNewEntity, what its Add menu makes),
                          FPropertyEditor (the rows that show and change reflected properties, with
                          FPropertyChange and FPropertyDefaults)
      Panels/             One class per panel: FViewportPanel, FOutlinerPanel, FDetailsPanel; FUiPanel is what
                          they share
      Dialogs/            One class per window that has to be answered: FNewProjectDialog, FOpenProjectDialog,
                          FSaveSceneDialog, FOpenSceneDialog; FUiDialog is what they share
  Tests/                  Unit tests (EditorTests executable)
    TestEditorObjects.h   Component classes for the tests, FScopedEditorTest, JSON written as text
    Ui/TestEditorUi.h     FEditorUiTest: an editor with its user interface, drawn headless and driven by a test
    Ui/Panels/            The tests of single panels, driven the same way
    Ui/ReferenceImages/   The reference image of the editor's default layout
    Agent/                The agent server's tests, and a session of messages that CTest gives the executable
Tools/
  Format.cmake            Formats or checks all C++ sources
  GitHooks/pre-commit     Optional hook that runs the format check
  HeaderTool/             WhiteWolfHeaderTool: generates reflection code from WCLASS(), WPROPERTY(), WFUNCTION()
    Main.cpp              Entry point
    Private/              Tokenizer, header parser, code generator
    Tests/                Unit tests (HeaderToolTests executable)
  ShaderCompiler/         WhiteWolfShaderCompiler: compiles GLSL shaders to SPIR-V and writes them as C++ source
    Main.cpp              Entry point
    Private/              GLSL compiler (on the glslang library), code generator
    Tests/                Unit tests (ShaderCompilerTests executable)
  ToolSupport/            File helpers shared by the build tools
docs/
  DECISIONS.md            Decision log
```

Reserved by the specification and created by later milestones: `Runtime/` (game runtime executable) and `Plugins/<Name>/` (optional plugin DLLs).

## Adding code

- **A new class or struct**: one type per header/source pair. Put the header in `<Module>/Public/<Area>/` if other modules may use it, otherwise in `<Module>/Private/<Area>/`. Put the source in `<Module>/Private/<Area>/`. The file name is the type name without its prefix: `FVersion` lives in `Version.h` and `Version.cpp`.
- **No CMake edit is needed** for new files: modules glob their `Public/`, `Private/` and `Tests/` directories.
- **Exporting from a module**: mark every function or class that another module (including the tests) must link to with the module's API macro: `ENGINE_API` for the Engine module, `EDITOR_API` for the Editor module. Annotate individual functions on value types and whole classes when they have virtual functions. Symbols without the macro are not visible outside the DLL on any compiler.
- **Include order**: a `.cpp` includes its own header first. Every engine header starts with `#include "CoreMinimal.h"` (the headers that `CoreMinimal.h` itself includes start with `Core/CoreTypes.h` instead). Then project headers, then a blank line, then third-party and standard headers. Include exactly what the file uses; do not rely on indirect includes. Prefer forward declarations in headers.
- **Tests**: add `<Module>/Tests/<Area>/<Subject>Tests.cpp` in the same commit as the code it tests.
- **Test helper types** that exist only to exercise one feature may share a single `Test<Feature>.h`/`.cpp` pair, as `Tests/Object/TestObjects.h` does. This is the only exception to one type per file.
- **A new module**: call `ww_add_module(<Name>)` and `ww_add_module_tests(<Name>)` from the module's `CMakeLists.txt`; see `Engine/CMakeLists.txt`. Both run the header tool for the module, so its `WCLASS()` classes are registered without further CMake.

## Code style

The code follows the [Epic C++ Coding Standard](https://dev.epicgames.com/documentation/en-us/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine) with the adaptations listed here. Layout is enforced by `.clang-format`; naming and design rules are enforced by review.

### Naming

- `PascalCase` for types, functions, variables, parameters, members, constants and files. No underscores, no Hungarian notation other than the prefixes below.
- Type prefixes:

  | Prefix | Used for | Example |
  | --- | --- | --- |
  | `W` | Classes derived from `WObject` (replaces Unreal's `U`) | `WObject`, `WClass` |
  | `F` | Structs and all other classes | `FVersion` |
  | `E` | Enums, always `enum class` | `ELogVerbosity` |
  | `I` | Abstract interfaces | `IPhysicsScene` |
  | `T` | Templates | `TWeakObjectPtr<T>` |

- Boolean variables and members start with `b`: `bIsVisible`. Functions that return `bool` ask a question: `IsValid()`, `ShouldTick()`.
- Functions are verbs; types and variables are nouns. Names are descriptive and unambiguous; do not abbreviate.
- Parameters written by the function are prefixed with `Out` (`OutVersion`, `bOutSucceeded`). Prefix a parameter with `In` when it would otherwise shadow a member (`InName`). Shadowing is not allowed: a parameter or local variable must never have the name of a data member of its class or of a base class, in static member functions too. MSVC rejects it (C4458) even where GCC and Clang do not.
- Macros are `UPPER_SNAKE_CASE` with the `WW_` prefix. The exceptions are the reflection macros named by the specification (`WCLASS()`, `WPROPERTY()`, `WFUNCTION()`), the module API macros (`ENGINE_API`) and `DLLEXPORT`/`DLLIMPORT`.
- CMake functions are `ww_snake_case`, cache options are `WW_UPPER_SNAKE_CASE`, local variables are `snake_case`.

### Formatting

- Tabs for indentation, four columns wide; spaces only to align within a line. Lines are at most 120 columns.
- Braces go on their own line. Every `if`, `else`, `for` and `while` body has braces, even a single statement.
- `switch`: a non-empty case that falls through must say so with `[[fallthrough]];`. Always provide a `default` case.
- Pointers and references bind to the type: `WObject* Object`, `const FVersion& Version`.
- One declaration per line. Declare variables where they are first used and initialise them at declaration.
- Run `cmake -P Tools/Format.cmake` before every commit. Never hand-tune against the formatter.

### Language use

- C++20. Use the standard library (`std::string`, `std::vector`, `std::optional`, `std::format`) directly; this project does not reimplement Unreal's containers (D-009). Strings are UTF-8 `std::string`; there is no `TEXT()` macro.
- Math types are glm types under engine names: `FVector` (`glm::dvec3`) for every world-space position, `FVector3f` (`glm::vec3`) for directions, scales and mesh-local data, `FQuat` (`glm::quat`, constructed as w, x, y, z) for rotations, and `FTransform` for location, rotation and scale. Their components are glm's lowercase `x`, `y`, `z`, `w`. Never store a world-space position in single precision. `FMatrix4f` (`glm::mat4`) is for the renderer only.
- Rotations are stored and combined as `FQuat`. An `FRotator` is a rotation as `Pitch`, `Yaw` and `Roll` in degrees, around +X, +Y and +Z, for showing a rotation to a person and making one from numbers (`ToQuat`, `FromQuat`, D-056); do not store one or calculate with one.
- The world is right-handed with +Y up, and lengths are metres. Objects and cameras face along their own -Z axis with +X to their right. Triangles are front-facing when their corners run counter-clockwise. This is glTF's convention (D-028).
- Integer types: `int8`, `int16`, `int32`, `int64`, `uint8`, `uint16`, `uint32`, `uint64` from `Core/CoreTypes.h`. Use `int32` for counts and indices in engine APIs; use `std::size_t` only where a standard container requires it.
- `auto` only for lambdas, iterators and types that cannot be spelled; otherwise write the type.
- `nullptr`, never `NULL` or `0`. `override` on every override. `enum class` always. `static_assert` for compile-time invariants.
- Const-correctness everywhere: `const` methods, `const` references for read-only parameters, `const` locals that are never reassigned.
- No raw `new` or `delete`. Use values, RAII, `std::unique_ptr` and standard containers.
- No `using namespace` at global scope. File-local helpers go in an anonymous namespace in the `.cpp`.
- Use C++ casts, not C-style casts.
- Avoid `bool` parameters in public APIs; prefer an `enum class`. Name literal arguments with a constant when their meaning is not obvious at the call site.
- Use named intermediate variables to make complicated conditions readable: `const bool bIsLastComponent = ...;`.
- Handle every error. Functions that can fail return `std::optional`, `bool` with an `Out` parameter, or a result type; they never fail silently.
- Engine code does not throw exceptions and does not catch them. Report failures through return values and the log.
- Assert programming errors with `WW_CHECK(Condition)` or `WW_CHECKF(Condition, "format", Args...)`. A failed check logs a Fatal error and terminates. Checks are active in every build configuration. Never use a check for something that can happen in a correct program, such as bad input or a missing file.
- Log with `WW_LOG(LogCategory, Verbosity, "format {}", Args...)`, using `std::format` syntax. Verbosities: `Fatal` (terminates), `Error`, `Warning`, `Display` (also shown on the console), `Log`, `Verbose`, `VeryVerbose`. Each subsystem defines its own category: `extern ENGINE_API FLogCategory LogMySystem;` in a header and `constinit FLogCategory LogMySystem("LogMySystem", ELogVerbosity::Log);` in the source.
- Do not call C runtime functions that MSVC deprecates (`fopen`, `strcpy`, `sprintf`, `localtime`, `getenv`, ...); they are errors under `/WX`. Use the C++ standard library equivalents.

### Comments

- Document every public type and function with a `/** ... */` comment that says what it does, with `@param` and `@return` where they add information. Document the contract, including failure behaviour.
- Use `//` comments inside functions to explain why, not what. Do not leave commented-out code.

### Warnings

First-party code compiles with `/W4 /WX` on MSVC and an equivalent strict set on GCC and Clang (`CMake/WhiteWolfCompilerSettings.cmake`). Fix warnings in the code. A warning may be disabled only project-wide, in that file, with the reason recorded in `docs/DECISIONS.md`.

## Objects and garbage collection

`WObject` is the base of every garbage-collected object. The rules below are not style; breaking them corrupts memory or is a fatal error.

- **Create objects with `NewObject<T>()` only.** Never `new`, `delete`, a stack variable or a by-value member of a `WObject` class. Constructing one any other way is a fatal error.
- **A reference keeps its target alive only if the collector can see it**: it must be a `TObjectPtr<T>` (or a `std::vector` of them) that is registered as a property of its class. A raw pointer or an unregistered `TObjectPtr` does not keep anything alive. Raw `T*` is fine for parameters and locals that do not outlive the current call.
- **`TWeakObjectPtr<T>`** refers to an object without keeping it alive. `Get()` returns null once the object is destroyed or marked for destruction, so it never dangles. Use it for back-pointers, caches and observers.
- **`Destroy()`** marks an object for destruction: `IsValid(Object)` is false and weak references are null from that call on, and the next collection destroys it and sets every remaining `TObjectPtr` to it to null. Check `IsValid()` before using a reference that something else may have destroyed.
- **Roots.** An object survives if it is rooted with `AddToRoot()` or reachable from a rooted object through registered properties. Classes are rooted automatically. Pair every `AddToRoot()` with a `RemoveFromRoot()`.
- **`OnDestroyed()`** runs once, inside the first `Destroy()` call, when the object is already invalid. Override it to destroy what the object owns, as an entity destroys its components and children. Always call `Super::OnDestroyed()`. It does not run for an object that is collected without `Destroy()`, so it is not the place to release resources.
- **`BeginDestroy()`** is where an object releases GPU, audio, physics and other non-memory resources. Always call `Super::BeginDestroy()`. All other objects are still allocated while it runs. **Destructors** run later and must not touch any other object.
- **Collection is incremental.** `FGarbageCollector::Tick()` runs once per frame and does a small, time-budgeted part of a collection, so game code runs between the steps of a collection. Never keep a raw pointer to an object from one frame to the next: only registered `TObjectPtr` properties, weak references and the root set are safe across frames. This applies to engine systems as much as to game code.
- **Storing a reference tells the collector.** Every construction, copy and assignment of a `TObjectPtr` goes through the collector, which is what makes incremental collection correct. Never copy objects that contain `TObjectPtr` members with `memcpy` or similar. Storing a reference to a destroyed object stores null.
- **`CollectGarbage()`** destroys everything unreachable before it returns. It is for tests and tools. Never call it while a function further up the stack holds a raw pointer to an object that nothing else keeps alive, and never from `BeginDestroy()`.
- **Leaks fail the test run.** The test executable ends with `FGarbageCollector::ReportLeakedObjects()`; a test that leaves an object in the root set makes the whole run fail. MSVC Debug builds and the `linux-sanitize` preset additionally fail on any leaked memory allocation (D-018).
- **Main thread only.** Objects are created, used and collected on the main thread.

Declaring a class:

```cpp
// Door.h
#include "CoreMinimal.h"

#include "Object/ObjectMacros.h"
#include "Object/ObjectPtr.h"

WCLASS()
class ENGINE_API WDoor : public WObject
{
	WW_DECLARE_CLASS(WDoor, WObject)

public:
	WPROPERTY(Category = "Door")
	float OpenAngle = 90.0f;

	WPROPERTY()
	TObjectPtr<WHinge> Hinge;

	WPROPERTY(Transient)
	std::vector<TObjectPtr<WPanel>> Panels;

	WFUNCTION(BlueprintCallable)
	void Open(float Speed);
};
```

That is all: nothing about reflection is written in `Door.cpp`. The build runs `WhiteWolfHeaderTool` over the headers of every module, and the tool writes `Build/<preset>/<Module>/Generated/<Module>.generated.cpp`, which registers each `WCLASS()` class with its properties and functions. The rules the tool enforces, each with an error message that names the file and line:

- `WCLASS()` goes on the line before a class at global scope that derives publicly from `WObject` or another reflected class. The class name starts with `W`, and the first line of the body is `WW_DECLARE_CLASS(<Class>, <BaseClass>)`.
- `WPROPERTY()` goes before a non-static member variable. Specifiers: `Transient` (not saved), `BlueprintReadOnly`, `BlueprintReadWrite`, `Category = "Name"`.
- `WFUNCTION()` goes before a member function that is not static, not a template and not overloaded. Every parameter needs a name. Specifiers: `BlueprintCallable`, `BlueprintPure`, `Category = "Name"`.
- Members may be private. Unknown specifiers are errors, so a typing mistake cannot pass silently.

A member can be a property if it is a `bool`, a fixed-width integer, `float`, `double`, `std::string`, `FVector`, `FVector3f`, `FQuat`, `FTransform`, `FLinearColor`, a `TObjectPtr`, a `TWeakObjectPtr` or a `std::vector` of any of these; any other type is a compile error in the generated file. A function's parameters and return value must be property types or pointers to `WObject` classes. Every property converts to and from JSON (`ExportJson`, `ImportJson`), which is what saving, loading and the agent interface use.

The generated file includes every reflected header of its module, so a forward declaration is enough for a `TObjectPtr<WOther>` member when `WOther` is a reflected class of the same module. For a class from another module, include its header.
A class is registered when its module loads. Class names are unique across all modules: `WClass::FindByName` finds one, and `WClass::GetAllClasses` lists them all, which is how to find the classes that derive from one without naming them. A class with a pure virtual function is abstract and cannot be created with `NewObject`.

Tests that create objects start with `const FScopedObjectTest ObjectTest;` (`Tests/Object/TestObjects.h`), which collects garbage before and after the test so that tests cannot affect each other.

## Scenes, entities and components

A `WScene` is a tree of `WEntity` objects; each entity has a name, a transform relative to its parent, and a list of `WComponent` objects that give it data and behaviour.

- **Create through the owner.** Entities come from `Scene->CreateEntity("Name", Parent)` and components from `Entity->AddComponent<WMyComponent>()`. Do not create either with `NewObject`. A scene itself is created with `NewObject<WScene>()` and must be held by a reflected `TObjectPtr` or added to the root set.
- **A new component type is one class.** Derive from `WComponent`, mark it with `WCLASS()` and its data with `WPROPERTY()`. It can then be added to entities, is saved and loaded with the scene, and appears in the editor, with no further code. Override `OnAttached()` and `OnDetached()` to react to joining and leaving an entity; call the `Super` version in both.
- **Transforms.** `GetRelativeTransform()` is relative to the parent; `GetWorldTransform()` and `GetWorldLocation()` are in world space and are calculated from the ancestors on every call. Locations are double precision (`FVector`).
- **Destroying.** `Scene->Destroy()` destroys a scene with everything in it and makes the collector start on its next tick. `Entity->Destroy()` destroys the entity, its components and all its descendants at once and removes it from its parent. `Component->Destroy()` and `Entity->RemoveComponent()` remove one component. These change the lists returned by `GetChildren()`, `GetComponents()` and `GetRootEntities()`: copy a list before destroying things while going through it.
- **Identifiers.** Every entity and component has an identifier that is unique within its scene and never reused (`GetId()`, `Scene->FindEntityById()`). Scene files and tools refer to objects by it.
- **Saving and loading.** `FSceneSerializer` writes a scene as JSON text (`SaveToFile`, `SaveToText`) and creates a scene from it (`LoadFromFile`, `LoadFromText`). Every `WPROPERTY()` that is not `Transient` is saved; nothing has to be written per component. A reference to an entity or component of the same scene is saved as its identifier, and a reference to an asset as its asset path; loading a scene loads the assets it names. Loading reports problems with single values as warnings and refuses only a file whose structure is wrong; the format is described in `Scene/SceneSerializer.h` and D-024. A loaded scene is returned unreferenced: hold it before the next collection. In a component, do in `OnAttached()` whatever depends on property values; it runs after a loaded scene's properties are all set.
- **Copying.** `FSceneSerializer::DuplicateEntity(Entity)` makes a copy of an entity with its components and all its descendants, in the same scene and under the same parent. What is copied is what is saved: every `WPROPERTY()` that is not `Transient`, by reflection, so a component needs no code of its own to be copied (D-053). A copy that referred to something inside the copied tree refers to that thing's copy; every other reference stays. The editor's command for it is `Entity.Duplicate`.
- **Failures are return values.** `CreateEntity`, `AddComponent` and `SetParent` refuse invalid requests (a parent from another scene, an abstract component class, an entity made its own ancestor) by returning null or false and logging a warning to `LogScene`; they are not assertions, because editor and agent commands can ask for them.

## Windows and input

- `FPlatform::Initialize(EPlatformMode::Windowed or Headless, Error)` starts the window system, `FWindow::Create` makes a window, and `FPlatform::Shutdown()` stops it after the last window is destroyed. `FPlatform::PollEvents()` is called once per frame. Everything runs on the main thread.
- **Headless mode** creates windows that exist in memory only. It needs no display, which is what lets tests, build servers and tools run the whole engine unattended. The Linux development builds support only this mode (D-025).
- **Input is read from an `FInputState`,** one per window: `IsKeyDown` for held keys, `WasKeyPressed` and `WasKeyReleased` for what changed in the current frame, and the mouse position, movement and scroll amount; `HasMousePosition` is false while the mouse is outside the window. Keys are `EKey` values named after their position on a US keyboard. Text is not read from keys: `GetTypedText` is what was typed in the current frame, as the keyboard layout made it.
- **A window can take the mouse.** `FWindow::CaptureMouse` hides the pointer and lets the mouse move without end, which is what turning a camera with it needs; the mouse position of the input state goes on changing, beyond the window. `ReleaseMouse` shows the pointer again where it was. Whatever takes the mouse gives it back when the button or key that it was taken for comes up.
- **A window knows the scale of its display.** `FWindow::GetContentScale()` is what the user has set the display to, 1 for the usual pixel density and 1.25 or 1.5 or 2 for denser ones; a window is given real pixels, so whatever draws a user interface into it multiplies its sizes by that. `FWindowDescription::bMaximized` opens a window as large as its screen allows.
- **Input can be simulated.** The `Handle...` functions of `FInputState` are public: a window calls them with real events, and tests and tools call them to press keys, type text and move the mouse without a person. Never read keys from GLFW directly.
- No GLFW type or header appears outside `Engine/Private/Platform/`. Include `Platform/Glfw.h` there, never `<GLFW/glfw3.h>`. `FWindow::GetNativeHandle()` is the window as the operating system knows it, for handing it to something that is not the engine's; it is null for a window that exists in memory only.

## Rendering

- **Nothing reaches the graphics card in world coordinates.** An `FCameraView` is the point of view of a picture. Its `GetCameraRelativeMatrix` subtracts the camera's location from an object's in double precision, and only that offset is converted to float; the view matrix has no translation. Use it for everything that is drawn, so that rendering is the same at any distance from the world origin.
- **What is drawn.** A `WStaticMesh` is a shape: vertices and triangle indices in its own space, set with `SetGeometry`, which refuses anything that is not a valid list of triangles. A vertex has a position, a normal and a texture coordinate, whose (0, 0) is the top left corner of a texture, as in glTF. A `WTexture` is a picture (`SetImage`) with a colour space, sRGB for colours and linear for data, and a wrap mode per direction. A `WMaterial` says what a surface looks like. A `WStaticMeshComponent` on an entity shows a mesh with a material at the entity's place in the world. `WStaticMesh::GetBounds` is the box that holds a mesh's vertices, in the mesh's space, and `WStaticMeshComponent::GetWorldBounds` that box where the entity puts it (`FBox`): what to ask for the room that something takes up. Meshes and materials are objects like any other: a component's reference keeps them alive, and many components can share one. An entity whose scale is below zero along one axis, or along all three, shows the mirror image of its mesh, with its front where the mirror puts it; the renderer draws such a mesh with a second set of pipelines (`FMeshPipelines`, D-065).
- **`FSceneRenderer`** draws a scene's visible static mesh components, as seen from an `FCameraView`, onto a render target: `Renderer->Render(*Scene, View, *Target, Error)`. It keeps each mesh's geometry on the graphics card, copies it again when `SetGeometry` gives the mesh a new version, and releases it when the mesh is collected. Textures are kept the same way, by an `FTextureCache` (`Private/Rendering/TextureCache.h`) that each renderer has. `GetStatistics()` says what the last call drew, uploaded and lit with.
- **Lighting is physically based and in high dynamic range.** Materials follow glTF's metallic-roughness model (`WMaterial`: `BaseColor`, `Metallic`, `Roughness`, `EmissiveColor`), and each of those can be varied over a surface by a texture that multiplies it (`BaseColorTexture`, `MetallicRoughnessTexture`, `EmissiveTexture`); a `NormalTexture` bends the surface's normal. Meshes need no tangents: the shader works them out. Lights are components: `WDirectionalLightComponent` shines along its entity's -Z axis with an `Intensity` in lux, `WPointLightComponent` shines from its entity's location with an `Intensity` in candela and an optional `Range`. Light may be any amount brighter than white; `FCameraView::Exposure` scales it and a tone mapper fits it to the display. One picture is lit by at most `FSceneRenderer::MaxDirectionalLights` and `MaxPointLights` lights.
- **Light from the surroundings.** A `WEnvironmentMap` holds a picture of a whole surrounding (`SetImage` with an `FHdrImage`, for example an HDRI read with `FImageFile::LoadHdr`): twice as wide as high, its middle straight ahead (-Z), its right half to the right, its top row up. A `WSkyLightComponent` lights a scene with it and shows it as the sky behind the scene; the entity's rotation turns the surrounding, and a scene is lit by its first sky light. Matte surfaces gather its light, metals and smooth surfaces mirror it, blurred by their roughness, and a material's `OcclusionTexture` dims it where a surface is hidden from its surroundings. Without a sky light, what no lamp or sun reaches is black. `WEnvironmentMap::GetIrradiance()` is the light a matte surface gathers from it for every direction, as spherical harmonics.
- **Shadows.** A `WDirectionalLightComponent` with `bCastShadows` (the default) casts the shadows of every `WStaticMeshComponent` with `bCastShadows`, up to `FSceneRenderer::ShadowDistance` from the camera, with soft edges. Each such light has four shadow maps of `FSceneRenderer::ShadowMapSize` pixels, one per slice of the view (`FShadowCascades`). Point lights cast no shadows. Shadow maps are laid out on a grid fixed in the world so that shadows do not crawl when the camera moves; the price is that a picture with shadows is not identical to the last bit when the same scene is built somewhere else, so tests of such pictures compare with `FImageDifference` and a small tolerance.
- **Ambient occlusion.** In the lit view mode the renderer first draws the depth of the picture and the way its surfaces face, works out from them how much of its surrounding each surface point can see, within `FSceneRenderer::AmbientOcclusionRadius` metres, and dims the sky light by it, scaled by `AmbientOcclusionIntensity`. Lamps and suns are not dimmed. A flat surface in the open is never occluded, at any angle.
- **Lines around things.** `FSceneRenderer::OutlinedEntities` names the entities that get a line around what they and the entities below them show, in `OutlineColor` and `OutlineWidth` pixels wide (D-068). Their meshes are drawn into a mask of the render target, without regard to depth, and a pass over the finished picture then draws the line around what is marked (`FOutlines`, `OutlineMask.frag`, `Outline.frag`). So the line goes around a thing also where something hides it, around several things that overlap as around one, and has its own colour in every view mode. Test it as `Tests/Rendering/SceneRendererOutlineTests.cpp` does, by the pixels beside a square.
- **View modes.** `FSceneRenderer::ViewMode` is `Lit` by default. `AmbientOcclusion` shows the ambient occlusion by itself as shades of grey. `Unlit` shows every surface in exactly its base colour, with no lights, exposure or tone mapping; use it for tests that check where things are drawn.
- **User interfaces are drawn from lists of triangles.** `FUiRenderer::Draw(DrawList, *Target, Error)` draws an `FUiDrawList` onto a render target, over what it shows: vertices with a position in pixels from the top left corner, a texture coordinate and an sRGB-encoded `FColor`, 32-bit indices, and commands that each draw a run of triangles with one `WTexture`, or another render target's surface, inside a clip rectangle (D-046). Colours are blended in linear light. Whatever lays a user interface out produces such a list; nothing outside `Engine/Private/Rendering` draws by itself. Test it as `Tests/Rendering/UiRendererTests.cpp` does: rectangles on a small surface, compared pixel for pixel.
- **Shader data.** Per-mesh data is an array of `FDrawData` that a shader indexes with `gl_InstanceIndex`; the lights are an `FViewConstants`. Both are declared in `Engine/Shaders/SceneData.glsl` and again in `Engine/Private/Rendering`, and must stay identical. They are descriptor set 0, together with the sky light's environment and the shadow maps; the textures of the material being drawn are set 1 (`MaterialTextures.glsl`), and what belongs to the render target, its ambient occlusion, is set 2 (`TargetData.glsl`). NVRHI puts textures at binding 0 and up, samplers at 128 and up and constant buffers at 256 and up within a set. Shaders work in camera-relative space: positions measured from the camera along the world's axes.
- **Shaders** are GLSL 4.60 files in `Engine/Shaders/`: `*.vert` and `*.frag` are shaders, anything else (`*.glsl`) is there to be included, with paths relative to that directory. The build compiles them with `WhiteWolfShaderCompiler` and embeds the result in the DLL; a shader error fails the build like a C++ error. Engine code creates a shader on the graphics card with `FShaderLibrary::CreateShader(Device, "Unlit.frag", nvrhi::ShaderType::Pixel)` (`Private/Rendering/ShaderLibrary.h`); `GetCode` is its SPIR-V. A structure that C++ and a shader share, such as `FDrawData`, is declared once on each side with a comment that names the other; keep the two identical. A new module gets shaders with `ww_add_shaders(<Module> ROOT <dir>)`.
- **Test what is drawn by reading pixels.** Draw a small scene to a small render target and check pixels whose colour follows from the geometry (see `Tests/Rendering/SceneRendererTests.cpp`). Use colours that encode to exact byte values, such as pure red, so that the check does not depend on how a driver rounds. Test lighting against the model, not against a stored number: `Tests/Rendering/TestSceneLighting.h` has the lighting model and the tone mapper written out again in C++, and the lighting tests compare the pixel in the middle of the picture with it. Test textures by showing them pixel for pixel: a 16 by 16 texture on a surface that covers 16 by 16 pixels (`Tests/Rendering/SceneRendererTextureTests.cpp`).
- **Depth is reversed and has no far plane:** 1 at the near plane, towards 0 at infinity. Clear depth to 0 and draw with a greater-or-equal depth test.
- `FRenderDevice::Create` opens the graphics card through Vulkan and NVRHI. It needs no window. Only one render device can exist at a time. Vulkan itself, the loader and one instance, is loaded when the first render device is created and stays loaded until the process ends (`FVulkanInstance`, D-044): never unload it, and never create a second instance.
- An `FRenderTarget` is a surface to draw on; `ReadPixels` copies it into an `FImage`, which is how tests and screenshots see what was drawn. Colours given to the renderer are linear (`FLinearColor`); pixels read back are sRGB-encoded bytes (`FColor`). `FSrgb::Encode` and `FSrgb::Decode` (`Math/Srgb.h`) turn one channel of the one into the other, for C++ code that needs to; the shaders have the same curve in GLSL.
- **Assets are the files of the content directory.** `WStaticMesh`, `WMaterial`, `WTexture` and `WEnvironmentMap` are `WAsset` classes. `FAssetManager::Load<WTexture>("Textures/Wood.png", Error)` returns the asset with an asset path (`FAssetPath`): a file relative to `FAssetManager::GetContentDirectory()`, with a key after `#` for a file that holds several, as in `Models/Crate.glb#Materials/0`. glTF, PNG, JPEG and Radiance HDR files are loaded as they are (D-043). A file's assets are kept or collected together, and what `Load` returns must be held by a reflected `TObjectPtr` before the next collection. A scene file saves a reference to a loaded asset as its path; an asset made with `NewObject` has no path and is saved as null, with a warning, so anything a saved scene should keep has to come from the content directory or be one of the assets that the engine has built in. Those are no files and need no content directory: `FAssetManager::CubeShape`, `SphereShape` and `PlaneShape` (`Engine/Shapes#Cube` and so on) are meshes of one metre, which an entity sizes with its scale (D-060).
- **`FGltfImporter::Import(Path, Scene, Error, &Warnings)`** brings a glTF 2.0 file (.gltf or .glb) into a scene: one entity named after the file, its nodes below it, with meshes, materials, textures and lights. Nothing is converted, because the engine uses glTF's axes, units, material model and light units. What the engine does not support (animations, skins, cameras, transparency and more) is left out with a warning each; only a file that cannot be read at all is an error. No cgltf type or header appears outside `Engine/Private/Rendering/GltfImporter.cpp`.
- **What is seen at a point of a picture is found with a ray** (D-066). `FCameraView::GetRay` is the ray along which the camera sees a point of its picture, and `FSceneRayCast::FindFirstHit` what a ray meets first in a scene: the front of a triangle of a mesh that a visible static mesh component shows, which is what is drawn there. The ray is taken into the space of each mesh (`WStaticMeshComponent::IntersectRay`, with the step from the world to the entity in double precision) and tried against its triangles (`WStaticMesh::IntersectRay`, `FIntersection`). That is worked out on the processor, a triangle at a time: right for a question now and then, such as what the user has clicked on, and not for many rays in every frame. Lights, and entities that show no mesh, are met by nothing. `FCameraView::Project` is the way back, from a point of the world to where it is in the picture, and `FRay::IntersectPlane` is where a ray goes through a plane: what something works with that lies over a picture and is dragged across it.
- `FMeshShapes` makes meshes of a box, a sphere and a plane, with normals and texture coordinates. `FImageDifference::Measure` says how much two pictures differ, which is how a screenshot is compared with a reference image.
- `FImageFile` reads PNG and JPEG files into an `FImage` and writes an `FImage` as a PNG file, from and to disk or memory. It reads and writes Radiance HDR files (`.hdr`) as an `FHdrImage`, a picture whose pixels are amounts of light (`FLinearColor`) that can be far brighter than white. No stb type or header appears outside `Engine/Private/Rendering/ImageFile.cpp`.
- **A window is shown pictures by an `FSwapChain`,** and that is all it is for: `SwapChain->Present(*Target, Error)` copies a finished render target onto the window's surface, pixel for pixel, and returns when the display takes it, which paces a program that draws in a loop (D-050). Everything is drawn onto render targets as before; nothing draws into a window directly, so what a window shows is what a test reads back. The swap chain follows the size of its window by itself; give it a render target of the window's size. Destroy it before its window. A window that exists in memory only can be shown pictures only where the graphics driver has a surface for such windows, which software drivers have and most graphics cards' drivers have not: ask `FSwapChain::IsSupported`, and write tests so that they pass either way, as `Tests/Rendering/SwapChainTests.cpp` does.
- Everything created from a render device keeps the graphics card alive, so the order in which the device and its resources are destroyed does not matter.
- No NVRHI or Vulkan type or header appears outside `Engine/Private/Rendering/`. Include `Rendering/VulkanApi.h` there, never `<vulkan/vulkan.hpp>`, so that the engine and NVRHI configure Vulkan identically; call Vulkan through the function table it sets up, with the plain C functions.
- Test rendering with validation on (`FRenderDeviceDescription::bEnableValidation`) and check that nothing was logged as an error: the validation layer reports misuse of the rendering interface to `LogRendering`.

## The editor and its commands

- **Everything the editor does is a command.** A command has a name (`Entity.Create`), typed parameters and a function, and is registered in the editor's `FCommandRegistry` (D-040). The user interface, the tests and the agent interface all act through `Editor.ExecuteCommand(Name, Arguments)` and nothing else: never give the user interface a way to change the scene that is not a command.
- **`FEditor` is the editor without its windows:** the scene being edited (`GetScene()`; there always is one), the commands, and the history of the scene (`Undo()`, `Redo()`, `GetHistory()`). It needs no window or graphics card. Only the editor replaces or destroys its scene (`SetScene`).
- **Adding a command.** Write a function `FCommandResult MyCommand(FEditor& Editor, const FCommandArguments& Arguments)` in the anonymous namespace of the area's file in `Editor/Private/Commands/` (`EntityCommands.cpp` for `Entity.*`) and register it in that file's `Register` function, with a summary and a description for every parameter: those sentences are all an agent knows about the command. A new area is a new `F<Area>Commands` class, registered in `FEditor`'s constructor. Add the command's name to the list in `Editor/Tests/EditorTests.cpp`.
- **Arguments are checked before the command runs.** The registry refuses a call with a missing argument, an argument of the wrong type or an argument that has no parameter. In the function, read arguments with `GetString`, `GetInteger`, `GetNumber`, `GetBoolean` and `GetJson` without checking them again, and ask `Has` first for an optional one.
- **Undo and redo need nothing from a command.** After every command the editor writes the whole scene down as the text of a scene file, and a difference from the last state is a step of the history (`FUndoHistory`, D-041); `Edit.Undo` and `Edit.Redo` load a state. What a command changes must therefore be something a scene file holds. A command that cannot change the scene says so in its description with `ECommandEffect::LeavesScene`, which spares the editor the writing-down; add it to the second list in `Editor/Tests/EditorTests.cpp`.
- **What the user does as one thing is one step.** A command is a step of the history by itself. Where one action of the user takes several commands, such as deleting everything that is selected or dragging an entity through many positions, put them between `Editor.BeginStep("Name")` and `Editor.EndStep()`, or in the scope of an `FScopedEditorStep` (`History/ScopedEditorStep.h`): everything that changed is then one step with that name (D-054). Steps do not nest.
- **Do not keep a pointer into the scene from one command to the next.** Undo, redo, `Scene.New` and `Scene.Open` replace the scene object, with all its entities and components. They keep their identifiers: keep those, and ask `Editor.GetScene()` again.
- **What is selected is the editor's, not the scene's** (D-047): `Editor.GetSelection()` is the identifiers of the selected entities, the last of which is the one whose properties are shown, and `Selection.Set` and `Selection.Get` are its commands. Selecting is not a step of the history. A destroyed entity drops out of the selection by itself and is back in it when an undo brings the entity back, so nothing that destroys entities has to tend the selection.
- **The viewport's camera and view mode are the editor's too** (D-048): `Editor.GetViewportCamera()` is an `FViewportCamera`, a location with a yaw and a pitch in degrees, and `Editor.GetViewMode()` an `ESceneViewMode`. `Viewport.SetCamera` and `Viewport.SetViewMode` change them, so a test or an agent can say exactly what the viewport shows; they are not saved with the scene and not steps of the history. The camera also has a focus point, what it looks at, `FocusDistance` in front of it, and the functions that steer it: `Turn`, `Fly`, `Orbit`, `Dolly`, `Pan` and `Frame` (D-059). `Viewport.Focus` brings it to where it sees entities whole.
- **So is how the gizmo is set:** `Editor.GetGizmoSettings()` is an `FGizmoSettings`, which says whether dragging the gizmo of the viewport moves, turns or sizes things (`EGizmoMode`), along the axes of the world or along their own (`EGizmoSpace`), and whether in steps, with the size of a step for each of the three. `SetGizmoSettings` changes it. `FTransformGizmo` is where the handles of the gizmo are in a picture, and `FGizmoDrag` what dragging one of them does to a thing: both are tested with numbers that can be done in the head (`Tests/Viewport/`). The viewport does not show the gizmo yet.
- **Assets belong to a project.** `Project.New` and `Project.Open` give the editor an `FProject`, a directory with a `.wwproject` file and a `Content` directory, which becomes the engine's content directory (D-045). `Asset.Import` copies a model, picture or environment into it, `Asset.Place` puts a model into the scene, and components take single assets as paths through `Object.SetProperty`. A relative path given to `Scene.Save` or `Scene.Open` is relative to the content directory, and `FEditor::MakeProjectPath` is the way back, from a file to what names it there. A scene file is called `Something.wwscene` (`FSceneSerializer::FileExtension`), which is how `Scene.List` tells the scenes of a project from its other files (D-062). A command that needs the project fails with a sentence that says so while none is open.
- **What the editor remembers from one run to the next is its settings** (`FEditorSettings`, D-063): for now the projects that were opened last. Every way of opening a project goes through `FEditor::SetProject`, which makes it the first of them, and `FEditor::FindRecentProjects` is those that can be opened now. An editor keeps its settings in memory, and in a file only if it is given one (`SetSettingsFile`). Only the editor in its window on the desktop is given the file of the person who is logged in (`FEditorSettings::GetUserFile`): a test, an agent's session and a screenshot never read or write it, and are the same whoever starts them. Something new to remember is a member of `FEditorSettings` that its `Load` and `Save` read and write.
- **Agents reach the commands through `WhiteWolfEditor --mcp`,** a Model Context Protocol server on standard input and output in which every registered command is a tool (`FAgentServer`, D-042). A new command needs nothing to appear there. With `--mcp`, standard output carries the protocol and nothing else: never write to `std::cout` from editor or engine code, use the log. `FEditorApplication::Run` is what the executable does, with its streams as parameters, which is how tests run it.
- **Failures are results.** Whatever can go wrong because of what was asked (no such entity, a class that is not a component, a file that cannot be read) returns `FCommandResult::Failure` with a sentence that says what is wrong, and leaves the editor as it was. Check before changing anything. `WW_CHECK` is for mistakes in the code only.
- **Refer to entities and components by identifier,** in arguments, in results and in property values. `FCommandTargets` (`Private/Commands/CommandTargets.h`) finds them and words the failure.
- **Which components there are is asked, not known.** `Component.ListClasses` names the classes that `Component.Add` accepts: every class that derives from `WComponent` and is not abstract, whichever module registered it, but not `WComponent` itself and not `WMissingComponent` (D-057). The rule is `FComponentClasses` (`Private/Commands/ComponentClasses.h`); a new component class needs nothing to be listed and offered.
- **Properties go through reflection.** `Object.GetProperty` and `Object.SetProperty` work for every `WPROPERTY()` that is not `Transient`, of any class, so a new component needs no command of its own for its data.
- **Test a command through `FEditor::ExecuteCommand`:** set the scene up with the scene's own functions, give the command, then check the scene and the result, and for every failure the exact reason and that nothing changed. Start such a test with `const FScopedEditorTest EditorTest;` before the `FEditor` (`Tests/TestEditorObjects.h`).

## The editor's user interface

- **`FEditorUi` is the user interface, and it does not know about windows.** `Ui->Draw(Target, Input, DeltaSeconds, Error)` takes in a frame of input from an `FInputState`, lays the menus and panels out for the size of a render target and draws them onto it (D-049). The executable gives it what a window shows; a test gives it a render target and input that the test makes up.
- **`FEditorWindow` is the editor's main window:** an `FWindow`, the render device, an `FSwapChain` and the `FEditorUi`, with `RunFrame(DeltaSeconds, Error)` drawing the user interface at the window's size and showing it (D-051). It works the same in the platform layer's headless mode, where the window exists in memory only and `GetPicture()` is what would be shown; that is how `Tests/EditorWindowTests.cpp` runs it. The executable's loop is `FPlatform::PollEvents()`, `Window->RunFrame(...)`, `FGarbageCollector::Tick(...)` until the window is closed (`FEditorApplication`). It takes the mouse of its window (`FWindow::CaptureMouse`) for as long as `FEditorUi::WantsMouseCaptured` says that the user is in the middle of something that needs it, and gives it back after. A window on the desktop cannot be opened in the test run: after changing how the editor starts or shows itself, start it by hand as well.
- **It is laid out with Dear ImGui and drawn by the engine.** No Dear ImGui type or header appears outside `Editor/Private/Ui/`. Include `Ui/ImGuiLibrary.h` there, never `<imgui.h>`. The library has no access to the graphics card, a window, files or the clipboard: `FUiInput` gives it input, `FUiTextures` keeps its textures as `WTexture` objects, and `FUiDrawData` turns what it laid out into an `FUiDrawList` for `FUiRenderer`. A failed assertion of the library is a fatal error in every build configuration, like a failed `WW_CHECK`.
- **A widget changes the scene by giving a command, and in no other way.** The functions of `FUiCommands` build the arguments and call `FEditor::ExecuteCommand`; add one for each thing a widget lets the user do. Each is one step of the history with a name that says what the user did ("Delete", "Rename"), also when it takes several commands (D-054), and those that act on several entities leave out the ones that are below another of them. The editor logs a command that fails, so a widget does not have to report it. Reading is direct: a panel looks at `Frame.Editor.GetScene()` in every frame and keeps no copy of it, which is why undo, redo and an agent's commands show up without anything telling the panel. Moving the viewport's camera is not a change to the scene and is set on the editor directly (D-048).
- **What the user changes over many frames is one step too.** A number that is dragged gives a command in every frame. `FUiCommands::BeginChange(Editor, "Set Intensity")` begins the step with the first of them and `EndChange` ends it when the user lets go of the widget, as `FDetailsPanel` does: it keeps the identifier of the widget that is held and ends the step in the first frame in which that widget is no longer active, or no longer laid out at all (D-058). Every other function of `FUiCommands` ends such a step before it begins its own, because steps do not nest.
- **Do not change the scene while going through it.** A command that destroys or moves entities changes the lists a panel is in the middle of. Note what was asked for and give the command once the rows are laid out, as `FOutlinerPanel` does with a click.
- **A property is shown by its type, by `FPropertyEditor`.** The Details panel shows every property of the selected entity's components that is saved with the scene (`FSavedProperties`), under the categories of their `WPROPERTY()`: a box to tick for a bool, a number to drag or type, three for a vector, angles for a rotation (`FRotator`), a button that opens a picker for a colour, a list to choose from for a reference to an entity or component, a path for a reference to an asset, and rows that can be added and removed for a `std::vector`. A new component class needs nothing to be shown and changed. A new type of property needs a branch in `FPropertyEditor::EditValue`, a value for a new element of a list in `MakeNewElement` beside it, and a property of that type in `WEditorTestGadgetComponent` (`Tests/TestEditorObjects.h`) with a test in `Tests/Ui/Panels/DetailsPanelTests.cpp`. A widget returns the value that the user changed its property to, as the JSON that `Object.SetProperty` takes, and the panel gives the command. The arrow beside a value puts back what the property has in a new object of the class (`FPropertyDefaults`).
- **What is typed for one thing must reach that thing.** A click that selects another entity also ends what was being typed for the one that was selected. So the Details panel is laid out before the panels in which the user selects (`LayOut` in `EditorUi.cpp`), and its widgets are in the scope of the entity they show (`ImGui::PushID`): what was typed goes to the entity it was typed for, or nowhere, never to another.
- **A panel is a class in `Editor/Private/Ui/Panels/`** with a `Title` and a `Draw(FUiFrame&)` that begins with `FUiPanel::Begin(Title, bIsOpen)` and always ends with `ImGui::End()`. A new panel also needs: a `bShow...` flag in `FUiWorkspace`, an entry in the Window menu (`FMainMenuBar`), a place in the default layout (`BuildDefaultLayout` in `EditorUi.cpp`) and a call in `LayOut` there. The default layout is what the specification describes; changing it changes the reference image.
- **The viewport's camera is steered in the viewport panel, as in Unreal** (D-059). A button without a look of its own lies on the picture and takes the mouse buttons; which one goes down on it, and whether Alt is held, says how the camera is steered until the button comes up: the right button flies (the mouse looks around, W, A, S, D, Q and E move, the wheel changes the speed), Alt and the left button go around the focus point, Alt and the right button move closer or away, the middle button moves sideways and up, and the wheel by itself moves closer or away. `FViewportPanel::Navigate` only turns what the mouse and the keys did into calls of `FViewportCamera`'s functions, which are public and tested by themselves, and sets the result on the editor; it gives no command, because the camera is not part of the scene (D-048). A click with the left button, without Alt and without the mouse having gone anywhere between down and up, selects what is seen under it (`FViewportPanel::SelectClicked`, D-067): the entity whose mesh the ray through that point of the picture meets first (`FSceneRayCast`). With Ctrl held it is added to what is selected, or taken out, as in the outliner (`FUiCommands::ToggleSelection`); a click on nothing selects nothing. What is selected has an orange line around it in the picture: `FViewportPanel::RenderScene` gives the renderer the selection to outline (D-068), and `FUiTheme` the colour and the width of the line, which grows with the scale like every other size. What else the mouse is to do on the picture belongs in the same place and must leave these buttons alone.
- **The toolbar is `FMainToolbar`,** a bar across the main window below the menu bar. Its Add button lists what can be put into the scene: an empty entity, the engine's shapes and the lights. Each line is an entry of a table in `MainToolbar.cpp` that names the entity, the class of its component and the mesh it shows; `FUiCommands::AddEntity` makes it from the commands that exist (`Entity.Create`, `Object.SetProperty`, `Component.Add`), as one step, where the viewport's camera looks, and selects it (D-061). Something new to add is a line in that table.
- **A window that has to be answered is a class in `Editor/Private/Ui/Dialogs/`** with an `Open` that says it should open and a `Draw` that begins with `FUiDialog::Begin(Title)` and lays it out while it is open. `FMainMenuBar` has the dialogs, opens them from its entries and shortcuts, and lays them out after the bar; another part of the user interface asks through `FUiRequests`, as the toolbar's Save button does. A dialog gives its command when the user confirms, closes if the command succeeded, and otherwise stays open and shows the command's sentence with `FUiDialog::ShowError`: what reads or writes files can fail for reasons the user has to see. Test one as `Tests/Ui/Dialogs/SaveSceneDialogTests.cpp` does, with a project in a temporary directory (`CreateTestProject`).
- **Projects are made and opened in two such windows** (D-064), both in the File menu. `FOpenProjectDialog` lists the projects that were opened last (`FEditor::FindRecentProjects`) and takes the file or the directory of any other; its rows are named `"OpenProject/<project file>"`. `FNewProjectDialog` takes a name and the directory to make the project's own directory in, and suggests the one beside the project that was opened last, or `FProject::GetDefaultLocation` for a first project. `FEditorUi::AskForProject` opens the first of the two, which is what the executable does when it starts. A test that makes a project through the window opens one of its own first, in its temporary directory: the place that the window suggests is then there, and never among the documents of whoever runs the tests.
- **A path is shown from its end where it does not fit** (`FUiDialog::FitPath`, which is `FUiText::FitEnd` with the letters that are drawn): the end says which file or directory it is.
- **Shortcuts are entries of the menu bar.** `FMainMenuBar` asks for them with `ImGui::Shortcut(..., ImGuiInputFlags_RouteGlobal)`, which lets a text field that is being typed into keep Ctrl+Z and Delete for itself, and not at all while a window is open that has to be answered first. What a shortcut does that belongs to a panel, such as F2 starting to rename in the outliner, it asks of the panel through `FUiRequests`.
- **Text is typed into a `std::string`:** `ImGui::InputText("##Name", &Text)`, which grows with what is typed. Never a buffer of a fixed size, which would cut a long name short. A field that shows a value of the scene is `FPropertyEditor::EditText`: it keeps what is typed to itself until the user is done, so that half a name or half a path is never given to anything, and Escape leaves the value as it was.
- **Items that a test clicks have names.** After laying an item out, call `Frame.Items.NameLastItem("Area/Thing")`: `"Outliner/7"` is the row of the entity 7, `"Menu/Edit/Undo"` a menu entry, `"Toolbar/Add/Cube"` a line of the list that `"Toolbar/Add"` opens. `FEditorUi::FindItem` says where a named item was drawn. A name says what an item is, not where, so tests do not break when the layout changes. In the Details panel, `"Details/7/Intensity"` is the widget of the property `Intensity` of the entity or component 7, `"Details/7/RelativeTransform/Location/X"` one number of a value that has several, and `".../Reset"` the arrow beside a value.
- **Colours are sRGB**, as they are written in a style sheet; `FUiTheme` has the editor's. The engine decodes them and blends in linear light (D-046).
- **Sizes are written for a scale of one, and nowhere but in the theme.** `FEditorUi::SetScale` multiplies every size of `FUiTheme` and has the letters drawn anew at their size; `FEditorWindow` gives the user interface the scale of its window's display in every frame (D-052). A panel that needs a size of its own takes a multiple of `ImGui::GetFontSize()` or a value of `ImGui::GetStyle()`, both of which are scaled, never a number of pixels. A button that shows a sign instead of a word is one of `FUiIcons`, which draws its signs with lines at whatever size that comes to.
- **Test the user interface by using it.** `FEditorUiTest` (`Editor/Tests/Ui/TestEditorUi.h`) is an editor with its user interface on a validated render device: `Test.Click("Outliner/7")`, `Test.Click("Outliner/7", EMouseButton::Right)`, `Test.Drag("Outliner/7", "Outliner/3")`, `Test.DragBy("Details/7/Intensity", FVector2D(60.0, 0.0))`, `Test.PressKey(EKey::Z, {EKey::LeftControl})`, `Test.Type("cam")`, then check what the editor's scene, selection or history now is, or read a pixel with `Test.ReadPicture()`. Each action draws the frames the library needs to take it in. What these do not cover, such as holding a mouse button down while keys are pressed, a test does with `Test.Input` and `Test.DrawFrame()` itself, as `Tests/Ui/Panels/ViewportPanelTests.cpp` does. Only one `FEditorUiTest` can exist at a time, because it owns the render device.
- **The default layout has a reference image,** `Editor/Tests/Ui/ReferenceImages/EditorDefaultLayout.png`, which `Tests/Ui/EditorUiTests.cpp` compares a picture of the user interface with, as the renderer's test scene is compared with its own. When a change is meant to alter how the editor looks, look at `Build/<preset>/TestOutput/EditorDefaultLayout.png`, copy it over the reference image and say in the commit message why the picture changed.

## Third-party dependencies

- Prefer small, widely used, permissively licensed libraries. The engine's required stack is fixed by the specification.
- Fetch with CMake `FetchContent`, pinned to a full commit SHA with the release tag in a comment, and marked `SYSTEM` so its headers cannot break warnings-as-errors.
- A dependency that only a plugin needs is fetched inside that plugin's CMake option, so a disabled plugin downloads and compiles nothing.
- Third-party types stay out of engine public headers unless the specification says otherwise.
- Record every dependency in `docs/DECISIONS.md`: what it is, its version and licence, and why it was chosen.

## Git workflow

- Small, focused commits. One logical change per commit, with its tests and documentation.
- Commit message: an imperative subject line of at most 72 characters ("Add version parsing"), a blank line, then a body explaining what changed and why.
- `main` must always build and pass every test.
- Optional but recommended once per clone: `git config core.hooksPath Tools/GitHooks` to reject unformatted commits.
- Continuous integration (`.github/workflows/ci.yml`) builds and tests with MSVC from Visual Studio 2022 and from Visual Studio 2026 on Windows and with the Linux cross-check presets, and checks formatting. A milestone is complete only when CI is green on the commit that finishes it.
- If you worked on a machine without MSVC, say so when you hand off, and treat the change as unverified until the Windows CI job passes.

### Pre-commit review checklist

Run this before every commit. Do not commit until every item is true.

1. **Formatted**: `cmake -P Tools/Format.cmake` leaves no changes.
2. **Builds and passes on every preset this machine supports**: on Windows, `windows-debug` and `windows-release`; on Linux, `linux-gcc-debug`, `linux-gcc-release`, `linux-clang-debug` and `linux-sanitize`. Zero warnings, zero failed tests, no skipped test you could have enabled.
3. **Review the staged diff** (`git diff --staged`) line by line and confirm:
   1. **Code style**: naming, prefixes, formatting, comments and language rules above are followed.
   2. **Production quality**: no `TODO`/`FIXME`/hack, no dead or commented-out code, no debugging leftovers, every error path handled, no warning suppressed without a recorded reason, no behaviour that depends on undefined or unspecified results.
   3. **Tests**: every new or changed behaviour has unit tests, including failure paths, and the full suite passes.
   4. **Exports**: everything another module needs is marked with the module API macro; nothing else is.
   5. **Documentation**: `AGENTS.md`, `README.md`, `docs/DECISIONS.md` and the skills still describe the repository truthfully.
4. **Scope**: the commit contains one logical change and nothing unrelated.

## Skills

Step-by-step procedures live in `.claude/skills/`:

| Skill | Use it when |
| --- | --- |
| `build-and-test` | Building, running tests, running a single test, or diagnosing a build failure |
| `pre-commit-review` | About to commit: formatting, verification, diff review and the commit itself |
| `add-third-party-dependency` | Adding or updating a pinned third-party library |
