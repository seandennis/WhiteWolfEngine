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

- Tests use [doctest](https://github.com/doctest/doctest). Each module has one test executable, `<Module>Tests`, built from every file under `<Module>/Tests/`, and one CTest entry of the same name. The build tools have their own: `HeaderToolTests` and `ShaderCompilerTests`.
- Tests are headless: they never open a window, wait for input or need a display. The renderer tests do need Vulkan 1.3: they create an `FRenderDevice`, draw to an `FRenderTarget` and read the pixels back, on whatever graphics card or software driver the machine has. A machine without Vulkan fails them; it does not skip them.
- Test executables are built with `DOCTEST_CONFIG_NO_MULTITHREADING`: call doctest assertions (`CHECK`, `REQUIRE`) on the main thread only. A test may start threads that run engine code. A test that needs a window creates an `FScopedHeadlessPlatform` (`Tests/Platform/ScopedHeadlessPlatform.h`), under which `FWindow::Create` makes windows that exist in memory only, and feeds input through `Window->GetInput().HandleKeyEvent(...)`.
- `EngineTests.DetectsMemoryLeaks` is a CTest entry that leaks memory on purpose and passes only if the leak is reported. It exists in MSVC Debug builds and under `linux-sanitize`, the builds that can detect leaked allocations.
- A test executable's `main` starts with `FProcessDiagnostics::DisableErrorDialogs()` and `FProcessDiagnostics::EnableMemoryLeakReport()`, so a crash cannot hang the run in a dialog and leaks are reported at exit.
- `CodeStyle.ClangFormat` is a CTest entry that runs the format check. It reports **Skipped** (not Passed) when clang-format 18 is not installed; install it rather than committing unchecked code.
- Test files mirror the source tree and are named `<Subject>Tests.cpp`, for example `Engine/Tests/Core/VersionTests.cpp` tests `Engine/Public/Core/Version.h`.
- Group the cases for a type in `TEST_SUITE("<TypeName>")`. Name each `TEST_CASE` as a sentence that states the expected behaviour.
- Test the contract, including every failure path: malformed input, boundaries, overflow, empty input.
- To test logging or a fatal error, create an `FScopedLogCapture` (`TestSupport/ScopedLogCapture.h`). While it is alive, log messages are recorded and a Fatal message or failed `WW_CHECK` throws `FFatalErrorException` instead of terminating: `CHECK_THROWS_AS(DoBadThing(), FFatalErrorException);`.
- Tests that write files use an `FScopedTempDirectory` (`TestSupport/ScopedTempDirectory.h`) and never write into the repository.
- **Reference images.** `Tests/Rendering/RendererTestSceneTests.cpp` draws the renderer's test scene (`Tests/Rendering/RendererTestScene.h`) and compares it with `Engine/Tests/Rendering/ReferenceImages/RendererTestScene.png`, allowing the small differences between graphics cards (D-034). When the picture does not match, the test fails and saves what it drew as `Build/<preset>/TestOutput/RendererTestScene.png`. Look at that picture. If the renderer is wrong, fix the renderer. If the picture is right, because a change was meant to alter how the scene looks, copy it over the reference image and commit it with the change, and say in the commit message why the picture changed.

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
  WhiteWolfThirdParty.cmake         Pinned engine dependencies (glm, GLFW, Vulkan-Headers, NVRHI, glslang, stb, cgltf)
  WhiteWolfThirdPartyTesting.cmake  Pinned test-only dependencies (doctest)
Engine/                   The core engine module (WhiteWolfEngine.dll)
  CMakeLists.txt
  Public/                 Headers other modules may include
    CoreMinimal.h         Included first by every engine header: core types, assertions, logging
    Core/                 Fundamental types and assertions: CoreTypes.h, Assert.h, Version.h, EngineVersion.h,
                          EnumClassFlags.h (WW_ENUM_CLASS_FLAGS), FileHelper.h (FFileHelper: whole text files),
                          ProcessDiagnostics.h (error dialogs, memory leak report)
    Input/                Keyboard and mouse: InputState.h (FInputState), Key.h (EKey), MouseButton.h (EMouseButton)
    Json/                 JSON text: JsonValue.h (FJsonValue), JsonReader.h (FJsonReader), JsonWriter.h (FJsonWriter)
    Logging/              The engine log: Log.h (FLog, WW_LOG), categories, messages and sinks
    Math/                 Math types: Vector.h (FVector, FVector3f, FVector2f, FVector2D), Quat.h (FQuat), Transform.h (FTransform),
                          LinearColor.h (FLinearColor), Color.h (FColor), Matrix.h (FMatrix4f)
    Object/               The object system: Object.h (WObject), Class.h (WClass), ObjectPtr.h (TObjectPtr),
                          WeakObjectPtr.h (TWeakObjectPtr), ObjectGlobals.h (NewObject, Cast, IsValid,
                          CollectGarbage), GarbageCollector.h (FGarbageCollector), ObjectMacros.h (class
                          registration)
    Platform/             The window system: Platform.h (FPlatform), Window.h (FWindow)
    Rendering/            The renderer: RenderDevice.h (FRenderDevice), RenderTarget.h (FRenderTarget), Image.h (FImage),
                          HdrImage.h (FHdrImage), ImageFile.h (FImageFile: PNG, JPEG and Radiance HDR files),
                          ImageDifference.h (FImageDifference),
                          Texture.h (WTexture), MipChain.h (FMipChain), MeshShapes.h (FMeshShapes: box, sphere, plane),
                          EnvironmentMap.h (WEnvironmentMap), SphericalHarmonics.h (FSphericalHarmonics),
                          SkyLightComponent.h (WSkyLightComponent), ShadowCascades.h (FShadowCascades),
                          CameraView.h (FCameraView), StaticMesh.h (WStaticMesh), Material.h (WMaterial),
                          StaticMeshComponent.h (WStaticMeshComponent), DirectionalLightComponent.h,
                          PointLightComponent.h, SceneRenderer.h (FSceneRenderer), SceneViewMode.h,
                          GltfImporter.h (FGltfImporter: .gltf and .glb files)
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
    Rendering/TestSceneLighting.h   The lighting model written out again in C++, and a lit test scene
    Rendering/TestGltfFiles.h       Builds small .gltf and .glb files for the importer's tests
    Rendering/RendererTestScene.h   The renderer's test scene, which is compared with a reference image
    Rendering/ReferenceImages/      The reference images
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

Reserved by the specification and created by later milestones: `Editor/` (editor executable), `Runtime/` (game runtime executable) and `Plugins/<Name>/` (optional plugin DLLs).

## Adding code

- **A new class or struct**: one type per header/source pair. Put the header in `<Module>/Public/<Area>/` if other modules may use it, otherwise in `<Module>/Private/<Area>/`. Put the source in `<Module>/Private/<Area>/`. The file name is the type name without its prefix: `FVersion` lives in `Version.h` and `Version.cpp`.
- **No CMake edit is needed** for new files: modules glob their `Public/`, `Private/` and `Tests/` directories.
- **Exporting from a module**: mark every function or class that another module (including the tests) must link to with the module's API macro, `ENGINE_API` for the Engine module. Annotate individual functions on value types and whole classes when they have virtual functions. Symbols without the macro are not visible outside the DLL on any compiler.
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
A class is registered when its module loads. Class names are unique across all modules. A class with a pure virtual function is abstract and cannot be created with `NewObject`.

Tests that create objects start with `const FScopedObjectTest ObjectTest;` (`Tests/Object/TestObjects.h`), which collects garbage before and after the test so that tests cannot affect each other.

## Scenes, entities and components

A `WScene` is a tree of `WEntity` objects; each entity has a name, a transform relative to its parent, and a list of `WComponent` objects that give it data and behaviour.

- **Create through the owner.** Entities come from `Scene->CreateEntity("Name", Parent)` and components from `Entity->AddComponent<WMyComponent>()`. Do not create either with `NewObject`. A scene itself is created with `NewObject<WScene>()` and must be held by a reflected `TObjectPtr` or added to the root set.
- **A new component type is one class.** Derive from `WComponent`, mark it with `WCLASS()` and its data with `WPROPERTY()`. It can then be added to entities, is saved and loaded with the scene, and appears in the editor, with no further code. Override `OnAttached()` and `OnDetached()` to react to joining and leaving an entity; call the `Super` version in both.
- **Transforms.** `GetRelativeTransform()` is relative to the parent; `GetWorldTransform()` and `GetWorldLocation()` are in world space and are calculated from the ancestors on every call. Locations are double precision (`FVector`).
- **Destroying.** `Scene->Destroy()` destroys a scene with everything in it and makes the collector start on its next tick. `Entity->Destroy()` destroys the entity, its components and all its descendants at once and removes it from its parent. `Component->Destroy()` and `Entity->RemoveComponent()` remove one component. These change the lists returned by `GetChildren()`, `GetComponents()` and `GetRootEntities()`: copy a list before destroying things while going through it.
- **Identifiers.** Every entity and component has an identifier that is unique within its scene and never reused (`GetId()`, `Scene->FindEntityById()`). Scene files and tools refer to objects by it.
- **Saving and loading.** `FSceneSerializer` writes a scene as JSON text (`SaveToFile`, `SaveToText`) and creates a scene from it (`LoadFromFile`, `LoadFromText`). Every `WPROPERTY()` that is not `Transient` is saved; nothing has to be written per component. A reference to an entity or component of the same scene is saved as its identifier. Loading reports problems with single values as warnings and refuses only a file whose structure is wrong; the format is described in `Scene/SceneSerializer.h` and D-024. A loaded scene is returned unreferenced: hold it before the next collection. In a component, do in `OnAttached()` whatever depends on property values; it runs after a loaded scene's properties are all set.
- **Failures are return values.** `CreateEntity`, `AddComponent` and `SetParent` refuse invalid requests (a parent from another scene, an abstract component class, an entity made its own ancestor) by returning null or false and logging a warning to `LogScene`; they are not assertions, because editor and agent commands can ask for them.

## Windows and input

- `FPlatform::Initialize(EPlatformMode::Windowed or Headless, Error)` starts the window system, `FWindow::Create` makes a window, and `FPlatform::Shutdown()` stops it after the last window is destroyed. `FPlatform::PollEvents()` is called once per frame. Everything runs on the main thread.
- **Headless mode** creates windows that exist in memory only. It needs no display, which is what lets tests, build servers and tools run the whole engine unattended. The Linux development builds support only this mode (D-025).
- **Input is read from an `FInputState`,** one per window: `IsKeyDown` for held keys, `WasKeyPressed` and `WasKeyReleased` for what changed in the current frame, and the mouse position, movement and scroll amount. Keys are `EKey` values named after their position on a US keyboard.
- **Input can be simulated.** The `Handle...` functions of `FInputState` are public: a window calls them with real events, and tests and tools call them to press keys and move the mouse without a person. Never read keys from GLFW directly.
- No GLFW type or header appears outside `Engine/Private/Platform/`. Include `Platform/Glfw.h` there, never `<GLFW/glfw3.h>`.

## Rendering

- **Nothing reaches the graphics card in world coordinates.** An `FCameraView` is the point of view of a picture. Its `GetCameraRelativeMatrix` subtracts the camera's location from an object's in double precision, and only that offset is converted to float; the view matrix has no translation. Use it for everything that is drawn, so that rendering is the same at any distance from the world origin.
- **What is drawn.** A `WStaticMesh` is a shape: vertices and triangle indices in its own space, set with `SetGeometry`, which refuses anything that is not a valid list of triangles. A vertex has a position, a normal and a texture coordinate, whose (0, 0) is the top left corner of a texture, as in glTF. A `WTexture` is a picture (`SetImage`) with a colour space, sRGB for colours and linear for data, and a wrap mode per direction. A `WMaterial` says what a surface looks like. A `WStaticMeshComponent` on an entity shows a mesh with a material at the entity's place in the world. Meshes and materials are objects like any other: a component's reference keeps them alive, and many components can share one.
- **`FSceneRenderer`** draws a scene's visible static mesh components, as seen from an `FCameraView`, onto a render target: `Renderer->Render(*Scene, View, *Target, Error)`. It keeps each mesh's geometry on the graphics card, copies it again when `SetGeometry` gives the mesh a new version, and releases it when the mesh is collected. `GetStatistics()` says what the last call drew, uploaded and lit with.
- **Lighting is physically based and in high dynamic range.** Materials follow glTF's metallic-roughness model (`WMaterial`: `BaseColor`, `Metallic`, `Roughness`, `EmissiveColor`), and each of those can be varied over a surface by a texture that multiplies it (`BaseColorTexture`, `MetallicRoughnessTexture`, `EmissiveTexture`); a `NormalTexture` bends the surface's normal. Meshes need no tangents: the shader works them out. Lights are components: `WDirectionalLightComponent` shines along its entity's -Z axis with an `Intensity` in lux, `WPointLightComponent` shines from its entity's location with an `Intensity` in candela and an optional `Range`. Light may be any amount brighter than white; `FCameraView::Exposure` scales it and a tone mapper fits it to the display. One picture is lit by at most `FSceneRenderer::MaxDirectionalLights` and `MaxPointLights` lights.
- **Light from the surroundings.** A `WEnvironmentMap` holds a picture of a whole surrounding (`SetImage` with an `FHdrImage`, for example an HDRI read with `FImageFile::LoadHdr`): twice as wide as high, its middle straight ahead (-Z), its right half to the right, its top row up. A `WSkyLightComponent` lights a scene with it and shows it as the sky behind the scene; the entity's rotation turns the surrounding, and a scene is lit by its first sky light. Matte surfaces gather its light, metals and smooth surfaces mirror it, blurred by their roughness, and a material's `OcclusionTexture` dims it where a surface is hidden from its surroundings. Without a sky light, what no lamp or sun reaches is black. `WEnvironmentMap::GetIrradiance()` is the light a matte surface gathers from it for every direction, as spherical harmonics.
- **Shadows.** A `WDirectionalLightComponent` with `bCastShadows` (the default) casts the shadows of every `WStaticMeshComponent` with `bCastShadows`, up to `FSceneRenderer::ShadowDistance` from the camera, with soft edges. Each such light has four shadow maps of `FSceneRenderer::ShadowMapSize` pixels, one per slice of the view (`FShadowCascades`). Point lights cast no shadows. Shadow maps are laid out on a grid fixed in the world so that shadows do not crawl when the camera moves; the price is that a picture with shadows is not identical to the last bit when the same scene is built somewhere else, so tests of such pictures compare with `FImageDifference` and a small tolerance.
- **Ambient occlusion.** In the lit view mode the renderer first draws the depth of the picture and the way its surfaces face, works out from them how much of its surrounding each surface point can see, within `FSceneRenderer::AmbientOcclusionRadius` metres, and dims the sky light by it, scaled by `AmbientOcclusionIntensity`. Lamps and suns are not dimmed. A flat surface in the open is never occluded, at any angle.
- **View modes.** `FSceneRenderer::ViewMode` is `Lit` by default. `AmbientOcclusion` shows the ambient occlusion by itself as shades of grey. `Unlit` shows every surface in exactly its base colour, with no lights, exposure or tone mapping; use it for tests that check where things are drawn.
- **Shader data.** Per-mesh data is an array of `FDrawData` that a shader indexes with `gl_InstanceIndex`; the lights are an `FViewConstants`. Both are declared in `Engine/Shaders/SceneData.glsl` and again in `Engine/Private/Rendering`, and must stay identical. They are descriptor set 0, together with the sky light's environment and the shadow maps; the textures of the material being drawn are set 1 (`MaterialTextures.glsl`), and what belongs to the render target, its ambient occlusion, is set 2 (`TargetData.glsl`). NVRHI puts textures at binding 0 and up, samplers at 128 and up and constant buffers at 256 and up within a set. Shaders work in camera-relative space: positions measured from the camera along the world's axes.
- **Shaders** are GLSL 4.60 files in `Engine/Shaders/`: `*.vert` and `*.frag` are shaders, anything else (`*.glsl`) is there to be included, with paths relative to that directory. The build compiles them with `WhiteWolfShaderCompiler` and embeds the result in the DLL; a shader error fails the build like a C++ error. Engine code gets a shader's SPIR-V with `FShaderLibrary::GetCode("Unlit.frag")` (`Private/Rendering/ShaderLibrary.h`). A structure that C++ and a shader share, such as `FDrawData`, is declared once on each side with a comment that names the other; keep the two identical. A new module gets shaders with `ww_add_shaders(<Module> ROOT <dir>)`.
- **Test what is drawn by reading pixels.** Draw a small scene to a small render target and check pixels whose colour follows from the geometry (see `Tests/Rendering/SceneRendererTests.cpp`). Use colours that encode to exact byte values, such as pure red, so that the check does not depend on how a driver rounds. Test lighting against the model, not against a stored number: `Tests/Rendering/TestSceneLighting.h` has the lighting model and the tone mapper written out again in C++, and the lighting tests compare the pixel in the middle of the picture with it. Test textures by showing them pixel for pixel: a 16 by 16 texture on a surface that covers 16 by 16 pixels (`Tests/Rendering/SceneRendererTextureTests.cpp`).
- **Depth is reversed and has no far plane:** 1 at the near plane, towards 0 at infinity. Clear depth to 0 and draw with a greater-or-equal depth test.
- `FRenderDevice::Create` opens the graphics card through Vulkan and NVRHI. It needs no window. Only one render device can exist at a time.
- An `FRenderTarget` is a surface to draw on; `ReadPixels` copies it into an `FImage`, which is how tests and screenshots see what was drawn. Colours given to the renderer are linear (`FLinearColor`); pixels read back are sRGB-encoded bytes (`FColor`).
- **`FGltfImporter::Import(Path, Scene, Error, &Warnings)`** brings a glTF 2.0 file (.gltf or .glb) into a scene: one entity named after the file, its nodes below it, with meshes, materials, textures and lights. Nothing is converted, because the engine uses glTF's axes, units, material model and light units. What the engine does not support (animations, skins, cameras, transparency and more) is left out with a warning each; only a file that cannot be read at all is an error. No cgltf type or header appears outside `Engine/Private/Rendering/GltfImporter.cpp`.
- `FMeshShapes` makes meshes of a box, a sphere and a plane, with normals and texture coordinates. `FImageDifference::Measure` says how much two pictures differ, which is how a screenshot is compared with a reference image.
- `FImageFile` reads PNG and JPEG files into an `FImage` and writes an `FImage` as a PNG file, from and to disk or memory. It reads and writes Radiance HDR files (`.hdr`) as an `FHdrImage`, a picture whose pixels are amounts of light (`FLinearColor`) that can be far brighter than white. No stb type or header appears outside `Engine/Private/Rendering/ImageFile.cpp`.
- Everything created from a render device keeps the graphics card alive, so the order in which the device and its resources are destroyed does not matter.
- No NVRHI or Vulkan type or header appears outside `Engine/Private/Rendering/`. Include `Rendering/VulkanApi.h` there, never `<vulkan/vulkan.hpp>`, so that the engine and NVRHI configure Vulkan identically; call Vulkan through the function table it sets up, with the plain C functions.
- Test rendering with validation on (`FRenderDeviceDescription::bEnableValidation`) and check that nothing was logged as an error: the validation layer reports misuse of the rendering interface to `LogRendering`.

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
