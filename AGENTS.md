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
| Product compiler | MSVC, Visual Studio 2022 17.6 or later ("Desktop development with C++" workload), x64 |
| Build system | CMake 3.25 or later (bundled with Visual Studio), driven through `CMakePresets.json` |
| Formatter | clang-format 18 (exactly major version 18) |
| Development cross-checks | GCC 13+, Clang 18+ and Ninja on Linux. Not a shipping platform; see D-004 |

On Windows, run every command below from a **Developer PowerShell for VS 2022** so that `cmake`, `ctest` and `git` are on the `PATH`. The first configure needs internet access to fetch the pinned third-party sources.

## Build commands

One command configures, builds and runs every test:

```
cmake --workflow --preset windows-debug
cmake --workflow --preset windows-release
```

The same steps individually, for incremental work:

```
cmake --preset windows                    # configure (generates Build/windows/WhiteWolfEngine.sln)
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

- Tests use [doctest](https://github.com/doctest/doctest). Each module has one test executable, `<Module>Tests`, built from every file under `<Module>/Tests/`, and one CTest entry of the same name.
- Tests are headless: they never open a window, wait for input or need a GPU-attached display.
- `CodeStyle.ClangFormat` is a CTest entry that runs the format check. It reports **Skipped** (not Passed) when clang-format 18 is not installed; install it rather than committing unchecked code.
- Test files mirror the source tree and are named `<Subject>Tests.cpp`, for example `Engine/Tests/Core/VersionTests.cpp` tests `Engine/Public/Core/Version.h`.
- Group the cases for a type in `TEST_SUITE("<TypeName>")`. Name each `TEST_CASE` as a sentence that states the expected behaviour.
- Test the contract, including every failure path: malformed input, boundaries, overflow, empty input.
- To test logging or a fatal error, create an `FScopedLogCapture` (`TestSupport/ScopedLogCapture.h`). While it is alive, log messages are recorded and a Fatal message or failed `WW_CHECK` throws `FFatalErrorException` instead of terminating: `CHECK_THROWS_AS(DoBadThing(), FFatalErrorException);`.
- Tests that write files use an `FScopedTempDirectory` (`TestSupport/ScopedTempDirectory.h`) and never write into the repository.

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
  WhiteWolfModule.cmake             ww_add_module() and ww_add_module_tests()
  WhiteWolfThirdParty.cmake         Pinned engine dependencies (glm)
  WhiteWolfThirdPartyTesting.cmake  Pinned test-only dependencies (doctest)
Engine/                   The core engine module (WhiteWolfEngine.dll)
  CMakeLists.txt
  Public/                 Headers other modules may include
    CoreMinimal.h         Included first by every engine header: core types, assertions, logging
    Core/                 Fundamental types and assertions: CoreTypes.h, Assert.h, Version.h, EngineVersion.h
    Logging/              The engine log: Log.h (FLog, WW_LOG), categories, messages and sinks
    Math/                 Math types: Vector.h (FVector, FVector3f), Quat.h (FQuat), Transform.h (FTransform)
    Object/               The object system: Object.h (WObject), Class.h (WClass), ObjectPtr.h (TObjectPtr),
                          WeakObjectPtr.h (TWeakObjectPtr), ObjectGlobals.h (NewObject, Cast, IsValid,
                          CollectGarbage), ObjectMacros.h (class registration)
    Reflection/           Reflected properties: Property.h (FProperty), ObjectProperty.h, WeakObjectProperty.h,
                          ArrayProperty.h, PropertyFactory.h (MakeProperty)
  Private/                Sources and module-internal headers, mirroring Public/
  Tests/                  Unit tests, mirroring Public/ (EngineTests executable)
    TestSupport/          Helpers shared by tests: FScopedLogCapture, FScopedTempDirectory
    Object/TestObjects.h  WObject classes used by the object system tests, and FScopedObjectTest
Tools/
  Format.cmake            Formats or checks all C++ sources
  GitHooks/pre-commit     Optional hook that runs the format check
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
- **A new module**: call `ww_add_module(<Name>)` and `ww_add_module_tests(<Name>)` from the module's `CMakeLists.txt`; see `Engine/CMakeLists.txt`.

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
- Math types are glm types under engine names: `FVector` (`glm::dvec3`) for every world-space position, `FVector3f` (`glm::vec3`) for directions, scales and mesh-local data, `FQuat` (`glm::quat`, constructed as w, x, y, z) for rotations, and `FTransform` for location, rotation and scale. Their components are glm's lowercase `x`, `y`, `z`, `w`. Never store a world-space position in single precision.
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
- **`BeginDestroy()`** is where an object releases GPU, audio, physics and other non-memory resources. Always call `Super::BeginDestroy()`. All other objects are still allocated while it runs. **Destructors** run later and must not touch any other object.
- **Collection points.** `CollectGarbage()` destroys everything unreachable before it returns. Never call it while a function further up the stack holds a raw pointer to an object that nothing else keeps alive, and never from `BeginDestroy()`.
- **Main thread only.** Objects are created, used and collected on the main thread.

Declaring a class, until the header tool generates this from `WCLASS()` and `WPROPERTY()`:

```cpp
// Door.h
class ENGINE_API WDoor : public WObject
{
	WW_DECLARE_CLASS(WDoor, WObject)

public:
	TObjectPtr<WHinge> Hinge;
	std::vector<TObjectPtr<WPanel>> Panels;
};

// Door.cpp
WW_IMPLEMENT_CLASS(WDoor)

void WDoor::RegisterClassMembers(WClass& Class)
{
	Class.AddProperty(MakeProperty<&WDoor::Hinge>("Hinge"));
	Class.AddProperty(MakeProperty<&WDoor::Panels>("Panels"));
}
```

A class is registered when its module loads. Class names are unique across all modules. A class with a pure virtual function is abstract and cannot be created with `NewObject`.

Tests that create objects start with `const FScopedObjectTest ObjectTest;` (`Tests/Object/TestObjects.h`), which collects garbage before and after the test so that tests cannot affect each other.

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
- Continuous integration (`.github/workflows/ci.yml`) builds and tests with MSVC on Windows and with the Linux cross-check presets, and checks formatting. A milestone is complete only when CI is green on the commit that finishes it.
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
