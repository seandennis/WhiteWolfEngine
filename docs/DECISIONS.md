# Decision log

Decisions made while building White Wolf Engine, with their reasons. The specification ([`WhiteWolfEngine.md`](../WhiteWolfEngine.md)) asks for every call that it leaves open to be recorded here.

Add new decisions at the end with the next number. Never renumber. To reverse a decision, add a new entry that supersedes it and mark the old one "Superseded by D-xxx".

## Open questions for the project owner

These are the owner's to decide and do not block development.

- **Licence and copyright notice.** The repository has no `LICENSE` file and source files carry no copyright header, because the specification names neither a licence nor a copyright holder. Once chosen, add the `LICENSE` file and, if wanted, the header line the Epic standard calls for.

---

## D-001: The engine is a DLL (`WhiteWolfEngine.dll`), not a static library

**Status:** Accepted. **Deviates from the specification**, which says "Static library for the core engine".

**Context.** The specification also requires plugins built as DLLs, a hot-reloadable game module DLL, and one garbage collector and one reflection registry shared by the engine, every plugin and the game module. If the engine were a static library, each DLL that used engine classes would link its own copy of the engine's global state (object lists, class registry, allocator tracking), so there would be several garbage collectors in one process. Exactly one module must own the engine's code and state, and every other module must import from it.

**Options considered.**

1. *Static library linked into each executable, with the executable exporting the engine's symbols to plugins.* Plugins would bind to the executable by file name, so an exported game could not rename its executable (`Tetris.exe`), every plugin would have to be built once per host executable (editor, runtime, each test runner), and plugin unit tests could not load the same binary that ships.
2. *Static library wrapped whole into a DLL.* Works, but the static library would exist only as an intermediate file that nothing links directly.
3. *The engine as a DLL.* One module owns the engine; executables, plugins, the game module and tests all import from it.

**Decision.** Option 3. The `Engine` module builds `WhiteWolfEngine.dll`. The editor and runtime executables, every plugin, the game module and every test executable link to it.

**Consequences.**

- Engine API that other modules use must be marked `ENGINE_API` (D-002). Unreal users know this pattern.
- An exported game folder contains the runtime executable, `WhiteWolfEngine.dll`, the enabled plugin DLLs, the game module DLL and the packaged assets.
- This is a build-level choice. Reverting to option 1 or 2 later changes CMake target types, not engine source code.

## D-002: Module layout, export macros and source globbing

**Status:** Accepted.

**Decision.**

- A module is a DLL with `Public/` (headers other modules may include), `Private/` (sources and internal headers) and `Tests/` (unit tests), declared with `ww_add_module()` and `ww_add_module_tests()` from `CMake/WhiteWolfModule.cmake`. This is Unreal's module layout.
- Each module exports symbols with its own macro, `<MODULE>_API` (`ENGINE_API`), which the build defines as `DLLEXPORT` inside the module and `DLLIMPORT` outside it, exactly as Unreal does.
- GCC and Clang builds hide symbols by default (`-fvisibility=hidden`, inlines hidden), so a missing export annotation fails to link on every compiler, not only on MSVC.
- Source files are found with `file(GLOB_RECURSE ... CONFIGURE_DEPENDS)`, so adding a class needs no CMake edit. CMake re-checks the globs on every build.

**Reasons.** The layout and macro are familiar to Unreal users. Hidden visibility makes the Linux development builds a faithful check of the DLL boundary. Globbing removes a manual step that agents and people forget; the cost is a short glob re-check at the start of each build.

## D-003: Warning policy and the two disabled MSVC warnings

**Status:** Accepted.

**Decision.** First-party targets compile with `/W4 /WX /permissive- /utf-8 /Zc:preprocessor` on MSVC. On GCC and Clang they compile with `-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion -Wold-style-cast -Wnon-virtual-dtor -Woverloaded-virtual -Wimplicit-fallthrough -Wundef -Wextra-semi -Werror`, chosen to flag what MSVC flags at `/W4`. Third-party headers are consumed as `SYSTEM` includes and are exempt.

Two MSVC warnings are disabled project-wide: **C4251** and **C4275**. They report that a DLL-exported class has a member or base class without a DLL interface, which in this codebase means standard library types such as `std::string` and `std::vector`.

**Reasons.** The warnings guard against mixing C++ runtimes or compiler versions across a DLL boundary. Here every module is built together, with the same compiler, flags and shared C runtime (`/MD`), so the layouts always agree. Engine API uses the standard library directly (D-009), which makes the warnings fire on nearly every exported class with no real defect behind them.

## D-004: MSVC is the product toolchain; GCC and Clang builds are development cross-checks

**Status:** Accepted.

**Decision.** The engine ships for Windows x64 built with MSVC from Visual Studio 2022, as the specification requires, or from Visual Studio 2026 (D-019). The code is also kept compiling and passing its tests with GCC and Clang on Linux through the `linux-gcc`, `linux-clang` and `linux-sanitize` presets. Linux is not a supported platform for users of the engine. The Linux builds run headless only: they cannot open a window (D-025).

**Reasons.** Three independent compilers catch more defects than one. AddressSanitizer, LeakSanitizer and UBSan on Linux are the strongest available check on a garbage-collected object system, and they run on every commit. Agents frequently work in Linux environments and must be able to verify their changes; a Windows-only codebase would leave that work unverified until CI. The cost is keeping platform-specific code behind small interfaces, which the Epic standard asks for anyway.

**Consequence.** A change is complete only when it passes on MSVC. Linux results are supporting evidence, never a substitute.

## D-005: Third-party code is fetched with FetchContent, pinned to commit SHAs

**Status:** Accepted.

**Decision.** Every dependency is declared with CMake `FetchContent`, pinned to a full commit SHA (with its release tag noted beside it) and marked `SYSTEM`. Nothing is vendored and there are no git submodules. Libraries must be permissively licensed (MIT, BSD, zlib, Apache-2.0, BSL-1.0, MPL-2.0 for Eigen via Chrono, public domain); GPL and LGPL code is not used.

**Reasons.** The specification allows submodules or FetchContent. FetchContent keeps "a fresh clone builds with one command" true without `--recursive`, and it lets a dependency be declared inside a plugin's CMake option so that a disabled plugin downloads and compiles nothing, which the specification requires. A commit SHA is immutable; a tag can be moved.

## D-006: Unit tests use doctest

**Status:** Accepted.

**Decision.** Tests are written with [doctest](https://github.com/doctest/doctest) 2.5.3 (commit `2d0a9359a60c51affe2a9bebb1be1dca47868151`, MIT licence). Each module has one test executable, `<Module>Tests`, registered as one CTest entry.

**Alternatives considered.** Catch2 v3 and GoogleTest are both good but must be compiled as libraries and are much larger. doctest is a single header with the fastest compile times of the three, has the assertion and test-suite features needed here, and is widely used.

**Reasons for one CTest entry per executable.** Per-test-case discovery runs the test executable during the build, which adds a failure mode (the executable must be runnable at build time, with its DLLs in place) for little gain: doctest already reports each failing case, and a single case can be run with `--test-case`.

## D-007: The build is driven by CMake presets

**Status:** Accepted.

**Decision.** CMake 3.25 is the minimum version. `CMakePresets.json` defines configure, build, test and workflow presets; `cmake --workflow --preset windows-debug` is the documented one-command build from a fresh clone. Build trees live in `Build/<configure preset>/`. Executables and DLLs share `Binaries/<Config>/` inside the build tree.

**Reasons.** Workflow presets (CMake 3.25) give one command that configures, builds and tests, with the same spelling locally and in CI. CMake 3.25 also provides `SYSTEM` on `FetchContent_Declare`. Visual Studio 2022 17.6 and later bundle a new enough CMake. A single binary directory lets Windows find the engine and plugin DLLs beside each executable without changing `PATH`.

## D-008: Formatting is enforced with clang-format 18

**Status:** Accepted.

**Decision.** `.clang-format` encodes the Epic layout rules: Allman braces, mandatory braces, tabs four columns wide, pointers bound to the type, one argument per line when a call does not fit. Where the Epic standard is silent, this project chooses a 120-column limit, indented namespace bodies, sorted include blocks and no single-line functions.

The formatter is pinned to **clang-format major version 18**. `Tools/Format.cmake` formats or checks all first-party sources and rejects other major versions. The check also runs as the CTest entry `CodeStyle.ClangFormat`, which is reported as skipped when clang-format 18 is not installed, and as a CI job that cannot skip.

**Reasons.** Different clang-format major versions format some constructs differently, so an unpinned formatter makes the check fail depending on who ran it. Version 18 is the version packaged for the Ubuntu 24.04 CI image and the one the code is formatted with. Running the check inside CTest means "all tests pass" also means "the code is formatted".

**Limit.** clang-format cannot check naming (PascalCase, type prefixes, `b` for booleans). Those rules are enforced by the pre-commit review checklist in `AGENTS.md`.

## D-009: Adaptations of the Epic C++ Coding Standard

**Status:** Accepted.

**Decision.** The standard applies as written, except:

- `W` replaces `U` as the object prefix, as the specification directs. There is no `A` prefix; entities and components are `W` classes.
- The C++ standard library is used directly (`std::string`, `std::vector`, `std::optional`, `std::unique_ptr`, `std::format`). Unreal's containers and string classes are not reimplemented.
- Strings are UTF-8 `std::string`. There is no `TEXT()` macro and no `TCHAR`.
- First-party macros use the prefix `WW_` instead of `UE_`.
- `int32` is used for counts and indices in engine APIs; `std::size_t` appears only where a standard container requires it.
- Fall-through in a `switch` is marked with `[[fallthrough]];` rather than a comment, because compilers check the attribute.

**Reasons.** The specification asks for a small engine; rebuilding Unreal's core library would be a large feature with no user-visible benefit. The standard library is tested, documented and known to every C++ programmer and agent. UTF-8 everywhere avoids a second string type and conversion bugs.

## D-010: Agent guidance lives in AGENTS.md, with skills in `.claude/skills/`

**Status:** Accepted.

**Decision.** `AGENTS.md` is the single source of truth for build commands, test commands, code style, directory layout and the review checklist. `CLAUDE.md` contains only an import of `AGENTS.md`, so tools that read either file get the same rules. Task procedures are Agent Skills under `.claude/skills/`: `build-and-test`, `pre-commit-review` and `add-third-party-dependency`.

**Reasons.** One canonical file cannot drift from a copy. Skills are loaded only when relevant, so procedures can be detailed without lengthening the always-loaded guide.

## D-011: Continuous integration on GitHub Actions

**Status:** Accepted.

**Decision.** `.github/workflows/ci.yml` runs on every push to `main` and every pull request: MSVC Debug and Release on `windows-2022` (the hosted image that carries Visual Studio 2022) and on `windows-2025` (which carries Visual Studio 2026; see D-019), the four Linux cross-check presets on `ubuntu-24.04`, and a formatting job. Actions are pinned to commit SHAs. The build jobs install a software Vulkan driver for the renderer tests (D-026), and problem matchers turn compiler and test errors into annotations on the run.

**Reasons.** The specification forbids committing a broken build and names MSVC as the compiler. CI is the only check that runs on the product toolchain no matter where a change was written.

## D-012: The specification is kept in the repository

**Status:** Accepted.

**Decision.** `WhiteWolfEngine.md` is committed at the repository root, unmodified.

**Reasons.** It is the source of truth that `AGENTS.md` points to, and a fresh clone on any machine must contain everything an agent needs to continue the work.

## D-013: Logging and assertions

**Status:** Accepted.

**Decision.**

- Messages are written with `WW_LOG(Category, Verbosity, "format", Args...)`, the counterpart of Unreal's `UE_LOG`, with Unreal's verbosity levels (`Fatal`, `Error`, `Warning`, `Display`, `Log`, `Verbose`, `VeryVerbose`). Format strings use `std::format` and are checked at compile time. Arguments are not evaluated when the category suppresses the message.
- A category (`FLogCategory`) is a constant-initialized, trivially destructible global, so logging is safe during static initialization and shutdown, and a plugin's categories need no registration or cleanup.
- `FLog` delivers each message to registered sinks (`ILogSink`): `FConsoleLogSink`, `FFileLogSink` and `FMemoryLogSink`. The memory sink is how the editor's Output Log, the agent interface and tests read the log back. Sinks are called one message at a time and must make each message durable before returning.
- Timestamps are UTC in ISO 8601 form. Local time would need the time zone database, which differs between platforms and allocates memory that is never released, and UTC is unambiguous in a log file.
- A Fatal message is delivered to every sink, then the fatal error handler runs, then the process aborts. Tests install a handler that throws, which is the only way to observe a fatal error without ending the test process.
- `WW_CHECK` and `WW_CHECKF` assert programming errors by logging a Fatal message with the expression and its location. They are active in every build configuration.
- Engine code neither throws nor catches exceptions. Failures are reported through return values and the log.

**Reasons.** Keeping checks on in release builds means a broken invariant stops the program at the point of failure instead of corrupting a saved scene or crashing later; the cost is a predictable branch. A single, small logging path with pluggable sinks covers the console, files, the editor and the agent interface without special cases.

## D-014: Math types

**Status:** Accepted.

**Decision.**

- Math is [glm](https://github.com/g-truc/glm) 1.0.3 (commit `8d1fd52e5ab5590e2c81768ace50c72bae28f2ed`, MIT licence), as the specification requires, used header-only with its default configuration. glm types are part of the engine's public API under engine names.
- `FVector` is `glm::dvec3` and is the type of every world-space position, as the specification requires. `FVector3f` is `glm::vec3` for directions, scales and mesh-local data. `FQuat` is `glm::quat`, single precision. The names follow Unreal Engine 5's large-world-coordinate naming, except that `FQuat` is single precision because the specification keeps rotations in 32-bit floats.
- `FTransform` holds a double-precision location, a rotation and a scale, and applies scale, then rotation, then translation. Positions are transformed in double precision. It offers `Combine(Parent, Child)` and `GetRelativeTransform(Parent)` instead of an `operator*`, because Unreal and glm multiply transforms in opposite orders and a named function cannot be misread.
- `FTransform` cannot represent shear, so combining a rotated child with a non-uniformly scaled parent is approximate, exactly as with Unreal's `FTransform`. Inverting a zero scale component yields zero for that axis rather than infinity.
- Members of the glm types keep glm's lowercase names (`x`, `y`, `z`, `w`), the one place where engine code departs from PascalCase.

**Decided later.** The world's handedness, up axis and unit of length are fixed by D-028; nothing in the transform maths depends on them.

## D-015: Object model and garbage collector

**Status:** Accepted.

**Decision.**

- **`WClass` is a `WObject`,** as `UClass` is a `UObject` in Unreal. Classes are ordinary objects in the object table, rooted for as long as their module is loaded. This lets class objects that are created at run time (Blueprint classes, milestone 9) be garbage-collected like anything else, and an instance keeps its class alive simply by referring to it. The two classes that describe each other, `WObject` and `WClass`, are wired together by hand when the object system starts.
- **Objects exist only through `NewObject`.** `WObject`'s constructor accepts only a request placed by the object system, so `new`, stack and member instances are fatal errors at the point of the mistake rather than memory corruption later. Likewise the destructor accepts only the collector.
- **Strong references are `TObjectPtr<T>` properties.** The specification says a pointer that keeps its target alive must be a `WPROPERTY()`; this engine additionally requires it to be a `TObjectPtr<T>`, as Unreal Engine 5 recommends for `UPROPERTY` members, rather than a raw `T*`. Every `TObjectPtr` has the same representation, so the collector can read and rewrite any reference without knowing its type, and assignments go through one place where the incremental collector's write barrier lives.
- **Properties locate their values through generated accessor functions** built from pointers to members, not through `offsetof`, which is undefined for the non-standard-layout classes that every `WObject` class is. This is checked by UBSan.
- **Collection is mark-and-sweep in three phases**: mark everything reachable from the roots (iteratively, so a long chain cannot overflow the stack); call `BeginDestroy()` on every unreachable object; then destruct and free them. Running all `BeginDestroy()` calls before any destructor means `BeginDestroy()` can safely use other objects that are being destroyed in the same collection.
- **Classes are registered when their module loads**, by a static object next to each class, so a class that is only ever created by name from a scene file exists without any code naming it. A property refers to the class of its target through a function, resolved on first use, so a class can refer to itself or to a class registered later.
- **Registration is generated.** The header tool writes `WW_IMPLEMENT_CLASS` and `RegisterClassMembers` for every class marked with `WCLASS()` (D-022). A class can still be registered by hand with the same two pieces, which is how the object system's own tests cover the registration API.
- **The object system is single-threaded.** Objects are created, used and collected on the main thread.

## D-016: Destroy, IsValid and weak references

**Status:** Accepted.

**Decision.**

- `WObject::Destroy()` only sets a flag (and removes the object from the root set). `IsValid()` is false from that moment, as the specification requires, but the object stays allocated until the next collection, so pointers that code already holds stay safe to use for the rest of the frame.
- The collector never marks a destroyed object. When marking meets a reference to one it sets that reference to null, so by the time the object is freed nothing reachable points at it. A destroyed object is not traversed either, so it does not keep the objects it refers to alive.
- A weak reference stores the object's slot in the object table and the slot's serial number, which changes whenever the slot is released. Resolving it is two comparisons, needs no list of weak references per object, and can never return a different object that later reuses the slot.
- A weak reference resolves to null as soon as its object is marked for destruction, and during a collection as soon as marking has shown the object to be unreachable. `BeginDestroy()` therefore cannot obtain a pointer to another object that is about to be freed and hand it to something that survives.
- Destroying a class, or adding a destroyed object to the root set, is a fatal error.

## D-017: Incremental collection with a write barrier

**Status:** Accepted.

**Context.** The specification requires collection to be incremental with a per-frame time budget, so that it causes no frame hitches. Game code therefore runs between the steps of a collection and changes references while the collector is part-way through marking. Without care, the collector can miss an object: it scans object A, then the game moves the only reference to object B out of a not-yet-scanned object and into A, and B is never marked.

**Decision.**

- A collection is a state machine with three phases, marking, notifying (`BeginDestroy`) and purging, advanced in work units of roughly one object each. `FGarbageCollector::Tick` runs units until the time budget for the frame is spent, reading the clock once per 64 units. The defaults are a collection every 60 seconds and 2 ms per frame.
- **Write barrier.** Every construction, copy and assignment of a `TObjectPtr` reports the stored object to the collector. During marking the collector marks it, so an object that is given a new referrer cannot be missed. This is the reason strong references must be `TObjectPtr` and not raw pointers (D-015).
- **Objects created during a collection survive it.** They are not scanned; everything they refer to is reported by the write barrier as it is stored.
- **Nothing can start referring to a dying object.** Storing a reference to an object that has been destroyed with `Destroy`, or that marking has shown to be unreachable, stores null, and such an object cannot be added to the root set. Together with weak references resolving to null (D-016) this makes it impossible to resurrect an object that is about to be freed.
- **An object destroyed after the current collection has already marked it** is freed by the next collection instead, because objects scanned earlier may still refer to it and those references are only cleared when their owners are scanned again.
- `CollectGarbage()` finishes any collection in progress and then runs a complete new one, so that it destroys everything unreachable at the time of the call.
- `StartCollection` and `AdvanceCollection` expose single steps so that tests can change references at exact points of a collection.

**Consequences.**

- Between two frames only registered `TObjectPtr` properties, weak references and the root set are safe. A raw pointer kept across frames may dangle. This is the same rule as in Unreal.
- Copying a `TObjectPtr` costs a function call. Arrays of references are therefore slower to grow and sort than arrays of raw pointers. This can be reduced later by inlining the common case; correctness came first.
- A single object is scanned in one unit however many references it holds, so one object with a very large array is scanned without interruption.
- The tests were checked by removing the barrier: AddressSanitizer then reports a use-after-free in the incremental collection tests.

## D-018: Leak detection

**Status:** Accepted.

**Decision.**

- `FGarbageCollector::ReportLeakedObjects()` runs a full collection and logs an error for every object that still exists and is not a class. The test executable calls it after the tests and fails the run if anything is reported, so a test that leaves an object rooted cannot pass.
- Engine memory allocations other than objects are checked by LeakSanitizer in the `linux-sanitize` preset, which fails the test run on any leak.

- On the product platform, `FProcessDiagnostics::EnableMemoryLeakReport()` switches on the leak check of the debug C runtime in MSVC Debug builds. The runtime writes its report ("Detected memory leaks!" and one line per block) to stderr when it shuts down, which is after every module has destroyed its static objects, so memory owned by a static object is never reported. The report comes too late to change the exit code, so every module test is registered with CTest to fail when that line appears in its output.
- The CTest entry `EngineTests.DetectsMemoryLeaks` runs one test case that leaks on purpose and passes only if the leak is reported. It guards against the check being switched off by accident, and runs in MSVC Debug builds and under the `linux-sanitize` preset.
- `FProcessDiagnostics::DisableErrorDialogs()` makes `abort()`, failed C runtime assertions and crashes write to stderr and end the process instead of opening a dialog, so an unattended test run can fail but cannot hang.

**Alternative considered.** Comparing heap snapshots taken before and after the tests reports as leaks everything that legitimately outlives the tests: lazily created static objects and containers that grew. Letting the runtime report at exit has no such false positives.

**Limit.** Release builds have no debug heap, so allocation leaks are detected in Debug builds and under the sanitizers only. Leaked objects are detected in every configuration.

## D-019: Visual Studio 2022 and Visual Studio 2026 are both supported

**Status:** Accepted. **Extends the specification**, which names Visual Studio 2022.

**Context.** The specification names MSVC from Visual Studio 2022. The project owner's development machine has Visual Studio 2026, and GitHub's current Windows image (`windows-2025`) carries Visual Studio 2026 too. Its compiler is the direct successor of the Visual Studio 2022 one and accepts the same options. The `windows` preset used to name the generator `Visual Studio 17 2022`, which cannot configure on a machine that has only Visual Studio 2026.

**Decision.**

- The product toolchain is MSVC from Visual Studio 2022 (17.6 or later) or from Visual Studio 2026.
- The `windows` configure preset names no generator. CMake then uses the newest Visual Studio installed, so the same presets and commands work with either version. The CMake bundled with each Visual Studio knows that version's generator.
- CI builds and tests Debug and Release with both versions (D-011), so neither can break unnoticed.
- The first MSVC build and test run of the engine was made with Visual Studio 2026 (18.12, MSVC 19.52).

**Alternatives considered.** A second set of presets for Visual Studio 2026 would double the Windows presets and give every documented command two spellings. Requiring Visual Studio 2026 would drop the version the specification names for no benefit. Ninja with MSVC would work with any version but generates no solution to open in Visual Studio.

**Consequences.**

- A machine with both versions installed builds with the newer one.
- The generated solution is `WhiteWolfEngine.slnx` with Visual Studio 2026 and `WhiteWolfEngine.sln` with Visual Studio 2022.
- The clang-format bundled with Visual Studio 2026 is not version 18, so the formatter must still be installed separately (D-008).

## D-020: JSON is read and written by the engine itself

**Status:** Accepted.

**Context.** Scenes, prefabs, project files, plugin descriptors and Blueprint assets are human-readable text, and the agent interface exchanges structured arguments and results. All of them are JSON, so a JSON value appears in the engine's public API (properties convert to and from it, commands take and return it).

**Decision.** `FJsonValue`, `FJsonReader` and `FJsonWriter` (`Engine/Public/Json/`) are first-party code built on `std::variant`, `std::from_chars` and `std::to_chars`.

- A number is a 64-bit integer when it is written without fraction or exponent and fits, otherwise a double. Entity identifiers and sector coordinates therefore survive exactly, and a double is written with the fewest digits that read back as the same double.
- A single-precision value is stored as the double with the same shortest decimal form, so `0.1f` is saved as `0.1`.
- An object keeps its members in insertion order, and the pretty format keeps arrays of scalars on one line, so saved files are stable and diff cleanly.
- The reader accepts strict RFC 8259 only, rejects duplicate keys, limits nesting to 128 levels and reports the line and column of an error.

**Alternative considered.** nlohmann/json is excellent and widely used, but third-party types stay out of engine public headers (see "Third-party dependencies" in `AGENTS.md`), so the engine would still need its own value type and a conversion layer on top of the library, and every file that touches JSON would pay for a very large header. A reader and writer for this small, fixed format are a few hundred lines that the standard library's number conversion makes straightforward, and they are covered by unit tests for every production of the grammar and every error path.

## D-021: Reflected properties and functions

**Status:** Accepted.

**Decision.**

- **One description per value.** An `FProperty` describes a member variable, an array element, or a parameter or return value of a function. It knows the value's C++ type name, its flags and category, and how to convert a value to and from JSON. Plain value types (`bool`, the fixed-width integers, `float`, `double`, `std::string`, `FVector`, `FVector3f`, `FQuat`, `FTransform` and, since milestone 3, `FLinearColor`) share one class, `TValueProperty<T>`, driven by a `TPropertyValueTraits<T>` specialization per type; adding a value type means adding one specialization.
- **JSON is the one external form of a property value.** Scene files, prefabs, the editor's copy and paste and the agent interface's "get and set any reflected property" all go through `ExportJson` and `ImportJson`, so a new component needs no hand-written save or load code, as the specification requires. An import reads into a temporary first, so a value that cannot be read leaves the property unchanged, and the error says what was expected.
- **Object references are written by the document, not by the property.** A property hands references to an `IPropertyJsonContext`, because how an object is identified depends on where it is saved: a scene identifies its entities and components by number. A reference to an object of the wrong class is refused on import.
- **Vectors are arrays, quaternions are `[w, x, y, z]`.** The pretty JSON format keeps arrays of numbers on one line, so a location is `[1.0, 2.0, 3.0]`. The quaternion order is that of `FQuat`'s constructor.
- **Specifiers.** `WPROPERTY` accepts `Transient` (not saved), `BlueprintReadOnly`, `BlueprintReadWrite` and `Category = "..."`. `WFUNCTION` accepts `BlueprintCallable`, `BlueprintPure` and `Category = "..."`. A property is saved and shown in the editor unless it is `Transient`; there is no `EditAnywhere`, because the specification makes every `WPROPERTY()` visible in the Details panel.
- **Functions are called through a generated thunk.** `MakeFunction<&WClass::Function>` derives the parameter and return properties from the member function's type, and an invoker that takes one value pointer per parameter. A caller supplies each value as the parameter's type without const or reference; a pointer to a `WObject` class is supplied as a `TObjectPtr`, so every object reference that reflection handles has the one representation the garbage collector understands. A non-const reference parameter is an output parameter. Static and overloaded functions cannot be reflected.

**Not yet decided.** Reflected enums and user-defined structs are added when the first component needs one. `BlueprintImplementableEvent` arrives with the Blueprint VM in milestone 9, which is also when properties gain the ability to construct and destroy values in memory that a VM owns.

## D-022: The header tool

**Status:** Accepted.

**Decision.**

- `WhiteWolfHeaderTool` (`Tools/HeaderTool/`) reads a module's headers and writes one source file per module, `<Module>.generated.cpp`, that registers every class marked with `WCLASS()`, with a property for each `WPROPERTY()` and a function for each `WFUNCTION()`. The generated code is a list of `MakeProperty<&WClass::Member>` and `MakeFunction<&WClass::Function>` calls, the same API a class registered by hand uses (D-015).
- **The tool reads names; the compiler reads types.** The tool finds the class name, its base class, the names of the marked members and the names of function parameters. It does not interpret types: `MakeProperty` and `MakeFunction` deduce them from the member pointers, so the tool needs no knowledge of C++ type syntax and cannot disagree with the compiler about a type. A member of a type that cannot be reflected is a compile error in the generated file.
- **There is no generated header.** A reflected class declares its reflection members with `WW_DECLARE_CLASS(Class, Base)`, written out in the class body, and the tool checks that the two names match the class declaration. Unreal's `GENERATED_BODY()` needs a `.generated.h` that every reflected header must include last and that must exist before anything compiles; naming the class and its base explicitly removes that file, its include-order rule and the stale-header failures that come with it, at the cost of one line that the tool verifies.
- **The tool is strict.** Unknown specifiers, static or overloaded functions, unnamed parameters, classes inside namespaces and every other construct it cannot register correctly are errors with the file and line, written in the `file(line): error:` form that Visual Studio and other editors make clickable.
- **Build integration.** `ww_add_module()` and `ww_add_module_tests()` call `ww_generate_reflection()`, which runs the tool as a custom target on every build. The tool rewrites the generated file only when its content changes, so an unrelated header edit compiles nothing extra, and an added or deleted header is always noticed. The tool cannot link to the engine, which is built with it; it shares only `Core/CoreTypes.h`.
- **Formatting.** `.clang-format` no longer inserts blank lines between definitions (`SeparateDefinitionBlocks: Leave`), because it would put one between `WCLASS()` and the class it marks.

**Alternatives considered.** Registration through macros alone, without a tool, cannot list members automatically in C++20 and would repeat every member name by hand, which is exactly what the specification's "no hand-written editor or save/load code" rules out. A libclang-based tool would understand all of C++, but adds a very large dependency to parse a handful of declarations whose types the compiler already checks.

**Limits.** A reflected class must be at global scope, and the header of a reflected class from another module must be included where a `TObjectPtr` to it is declared, because the generated file needs the complete type. Classes of the same module may be forward-declared.

## D-023: Scenes, entities and components

**Status:** Accepted.

**Context.** The specification asks for "a simple ECS for authoring scenes from entities and components", with components that are `WObject`s with reflected properties, scripts that are components with `BeginPlay`, `Tick` and `EndPlay`, and an editor modelled on Unreal's.

**Decision.**

- **Entities and components are objects.** A `WScene` owns a tree of `WEntity` objects and each entity owns a list of `WComponent` objects, all garbage-collected and reflected like everything else. This is the entity-component model of Unreal and Unity rather than a data-oriented ECS with archetype tables. It is what the rest of the specification assumes (components are created with `NewObject`, edited through `WPROPERTY()` and subclassed by scripts and Blueprints), and it is the model an Unreal user expects. Systems that need every component of a type ask the scene (`WScene::GetComponents<T>()`).
- **The transform belongs to the entity,** not to a component. Every entity has exactly one, the hierarchy is a hierarchy of entities, and nothing has to look up a transform component. It is an `FTransform` relative to the parent, so locations are double precision; world transforms are calculated from the ancestors on request. Caching them is left until a profile asks for it.
- **Ownership is strong downwards and upwards.** The scene refers to its root entities, entities to their children and components, and each of those back to its owner, all through reflected `TObjectPtr` members marked `Transient`. A tracing collector is not troubled by the cycles, a scene that nothing else refers to is collected whole, and a component can still reach its entity while it is being detached from one that is already marked for destruction.
- **Destroying is immediate and recursive.** `WObject` gains `OnDestroyed()`, called once by `Destroy()`. An entity uses it to leave its parent, detach and destroy its components and destroy its children in the same call, so no reachable list ever holds a destroyed entity. The memory is still freed by the collector.
- **A scene change starts a collection.** Destroying a scene calls `FGarbageCollector::RequestCollection()`, so the next tick begins collecting the old scene instead of waiting for the timer. This is the "on scene change" trigger the specification asks for.
- **Identifiers.** A scene numbers its entities and components from one counter and never reuses a number. Scene files, undo records and the agent interface refer to objects by these numbers, which stay small and readable; they are unique per scene, not globally.
- **Structure is not a property value.** The hierarchy and the component lists are `Transient`: the scene serializer writes them as the structure of the file, and only `Name`, `RelativeTransform` and component data are written as properties.
- **Requests that can be wrong return a result.** Creating an entity under a parent from another scene, adding an abstract component, or making an entity its own ancestor is refused with a warning and a null or false result, because editor and agent commands will ask for exactly these things.

**Not yet.** `BeginPlay`, `Tick` and `EndPlay` arrive with scripting in milestone 7; the world origin offset and origin rebasing arrive with the renderer and physics that have to move with it.

## D-024: Scene files

**Status:** Accepted.

**Decision.** A scene is saved as one JSON document, written by `FSceneSerializer` in the pretty format of D-020:

```json
{
  "Format": "WhiteWolfScene",
  "Version": 1,
  "NextId": 4,
  "Properties": { "Name": "Level 1" },
  "Entities": [
    {
      "Id": 1,
      "Properties": {
        "Name": "Player",
        "RelativeTransform": { "Location": [0.0, 0.0, 0.0], "Rotation": [1.0, 0.0, 0.0, 0.0], "Scale": [1.0, 1.0, 1.0] }
      },
      "Components": [
        { "Id": 2, "Class": "WHealthComponent", "Properties": { "Health": 100, "Target": 3 } }
      ]
    },
    { "Id": 3, "Parent": 1, "Properties": { "Name": "Camera" }, "Components": [] }
  ]
}
```

- **Reflection writes the data.** `"Properties"` holds every `WPROPERTY()` of the object that is not `Transient`, base class first, through `ExportJson`. The scene, its entities and its components are all saved this way, so a new component type needs no save or load code, as the specification requires. Reflected values always sit under `"Properties"`, apart from the structural keys (`"Id"`, `"Parent"`, `"Class"`, `"Components"`), so a component may have a property with any name.
- **The entity list is flat.** Entities are listed parents before children with a `"Parent"` identifier, rather than nested. Moving an entity to another parent then changes one line, and a file diff stays readable however deep the hierarchy is.
- **References are identifiers.** A reference to an entity or component of the same scene is its `"Id"`, a small whole number. A reference to anything else cannot be saved and is written as null with a warning; references to assets will be written as strings when assets arrive in milestone 3, which the number cannot be confused with.
- **Every value is written, defaults included.** A file then says everything about the scene without the reader knowing the defaults of this engine version, and a changed default does not silently change existing scenes.
- **`"NextId"` is saved,** so identifiers are not reused after a reload either.
- **Loading is strict about structure and tolerant about values.** A document that is not a scene, has a newer `"Version"`, a missing or duplicate `"Id"`, an unknown parent or a parent cycle is refused as a whole, and nothing is created. An unknown property, a value of the wrong type or a reference that does not resolve is reported as a warning, in the log and to the caller, and that property keeps its default: a renamed property must not make a scene unopenable. Unknown keys are reported too, because they are usually typing mistakes.
- **Unknown component classes are preserved.** A component whose class is not registered, which is what a scene looks like when its plugin is disabled, is loaded as a `WMissingComponent` that keeps the saved properties verbatim and writes them back under the original class name. The specification requires that opening and saving such a scene loses no data.
- **Two passes.** All entities and components are created before any property is read, so references may point forwards, and components receive `OnAttached()` only after every property is set.
- **Files are replaced atomically.** `FFileHelper::SaveStringToFile` writes a temporary file and renames it over the target, so a crash while saving cannot leave half a scene.

**Versioning.** `"Version"` is 1. A change that an older build could misread raises it, and a build refuses a version newer than its own instead of guessing.

## D-025: Windows and input on GLFW

**Status:** Accepted.

**Decision.**

- **GLFW 3.4** (commit `7b6aead9fb88b3623e3b3725ebb42670cbe4c579`, zlib licence) provides windows and input, as the specification requires. It is built as a static library and linked privately into the engine DLL. No GLFW type appears in an engine header; `FWindow` hides its GLFW handle behind a private implementation struct, and one private header, `Platform/Glfw.h`, is the only place that includes GLFW.
- **Headless mode is GLFW's null platform.** GLFW 3.4 always contains a platform that creates windows in memory and touches no window system. `FPlatform::Initialize(EPlatformMode::Headless)` selects it, so the same `FWindow` code runs in tests, on build servers and in tools as on a desktop, and the specification's requirement that the engine runs with no window and no user input is met by one switch rather than by a second code path.
- **The Linux development builds contain only the null platform.** GLFW's X11 and Wayland back ends are switched off there. Linux is not a shipping platform (D-004), its builds only run headless, and this way they need no window system packages to compile. `FPlatform::Initialize(EPlatformMode::Windowed)` fails with an error on such a build.
- **Input state is engine code, separate from the window.** `FInputState` holds what is down, what changed in the current frame and how the mouse moved. A window forwards GLFW's events to it through public `Handle...` functions, and tests and tools call the same functions to simulate input. That is how input is unit-tested without a window system, and how the agent interface will press keys in play mode.
- **`EKey` and `EMouseButton` have GLFW's code values,** so an event becomes an engine key with a range check and a cast. `Private/Platform/GlfwInputCodes.cpp` holds a `static_assert` for every value, which turns any disagreement, now or after a GLFW update, into a compile error. A conversion table would need a test that the null platform cannot provide, because it never produces key events.
- **Per-frame input.** `FPlatform::PollEvents()` starts a new input frame for every window and then delivers the events, so "pressed this frame" has one meaning everywhere. A window that loses the focus releases all its keys, because their releases go to another window.
- **Windows are created without an OpenGL context** (`GLFW_NO_API`); the renderer attaches a Vulkan surface in milestone 3.

**Limits.** The forwarding of real GLFW events into `FInputState` is a few lines that no unit test executes, because the null platform produces no events; the key codes are checked at compile time and the state logic by its own tests. Text entry, gamepads, cursor capture and full-screen modes are added when the editor and scripting need them.

## D-026: The render device: Vulkan through NVRHI, without a window

**Status:** Accepted.

**Dependencies.**

- **NVRHI** (commit `bf0bbd9b666a7297492beb1999a4ddc0dfded143`, the main branch on 2026-11-21; MIT licence), as the specification requires, with only its Vulkan backend and its validation layer built. NVRHI publishes no release tags, so the pin is a commit of its main branch, with the date noted.
- **Vulkan-Headers** v1.4.352 (commit `015e25c3c91b70eb1a754d36fb14c4ba6ad9b0b9`; Apache-2.0 OR MIT), the version NVRHI asks for. NVRHI would fetch it itself, by tag; the engine declares it first so that it is pinned to a commit like every other dependency.
- Both are linked privately into the engine DLL. Neither the Vulkan SDK nor a Vulkan library is needed to build: the Vulkan loader (`vulkan-1.dll`), which comes with graphics drivers, is opened at run time, and a machine without it gets an error message, not a failure to start. (Since D-044 it is opened once for the process and stays open.)

**Decision.**

- **Rendering does not need a window.** `FRenderDevice::Create` makes a Vulkan instance and device with no surface. What is drawn goes to an `FRenderTarget`, and `ReadPixels` returns it as an `FImage`. Screenshot tests, the agent interface's "capture a screenshot" and exporting all use this path; presenting to a window is one more consumer of a render target, added later, rather than the foundation.
- **The engine creates the Vulkan device; NVRHI is given it.** That is NVRHI's design. The engine requires Vulkan 1.3 with timeline semaphores, dynamic rendering and synchronization2, which NVRHI's backend uses unconditionally, and picks the most capable graphics card: dedicated, then integrated, then software.
- **One render device at a time.** NVRHI's Vulkan backend calls Vulkan through vulkan.hpp's default dispatcher, one global table of function pointers that is bound to one device. The engine defines that table (NVRHI, built as a static library, leaves it to the application) and refuses to create a second device while one exists.
- **Vulkan is included one way only.** `Private/Rendering/VulkanApi.h` includes vulkan.hpp configured exactly as NVRHI configures it, because the two must agree on the dispatcher. The engine then calls plain C Vulkan functions through the dispatcher's pointers. It does not use vulkan.hpp's C++ wrappers: their error handling is switched by macros, and a setting different from NVRHI's would give the same inline functions two definitions in one program.
- **Resources keep the device alive.** `FRenderDevice` and everything created from it share one `FRenderContext`, released with the last of them, so nothing can outlive the graphics card it lives on and no destruction order has to be remembered.
- **No NVRHI or Vulkan type appears in an engine header.** Public classes hold a private implementation, as `FWindow` does for GLFW.
- **Render targets are 8-bit sRGB.** Colours are drawn in linear space and the graphics card encodes them, so an image read back is ready to save or compare.

**Testing.**

- The renderer tests run for real, on whatever Vulkan the machine has, with NVRHI's validation layer on, and fail if it logs an error. They are not skipped where Vulkan is missing: a skipped rendering test would let the renderer break unnoticed.
- CI has no graphics card, so it installs **Lavapipe**, Mesa's implementation of Vulkan on the processor: from the `mesa-vulkan-drivers` package on Linux, and on Windows with the action `jakoch/install-vulkan-sdk-action` (v1.8.1, pinned to its commit), which also installs the Vulkan loader and registers Lavapipe with it.
- **doctest runs single-threaded** (`DOCTEST_CONFIG_NO_MULTITHREADING`). doctest keeps per-thread state in `thread_local` containers. A `thread_local` is constructed in every thread that starts in the process, including the worker threads of a graphics driver, which are still running when the process exits; their copies are never destroyed, and the debug heap reported them as leaks as soon as a test opened the graphics card. The tests assert on the main thread only, so nothing is lost. The same reasoning applies to engine code: a `thread_local` with a destructor will be reported as a leak by the same mechanism.

**Not yet.** Shaders and pipelines come with the next steps of milestone 3 (D-027 onwards). A swap chain for windows comes with the first executable that has a window (D-034), and Vulkan's own validation layers would need the Vulkan SDK.

## D-027: Shaders are GLSL, compiled while the engine is built

**Status:** Accepted.

**Dependency.** **glslang** 16.6.0 (commit `e1b562a8bed273a02f30b59b66a5d499793cede5`; BSD-3-Clause, with parts under Apache-2.0 and MIT), Khronos's reference compiler from GLSL to SPIR-V. Only its library is built, without its optimiser, its HLSL front end and its command-line program. It is linked into a build tool and is not part of the engine or of a shipped game.

**Decision.**

- **Shaders are written in GLSL 4.60 and compiled to SPIR-V 1.6 for Vulkan 1.3 during the build**, by `WhiteWolfShaderCompiler`, a first-party tool in `Tools/ShaderCompiler/` built on the glslang library. A shader that does not compile fails the build with `file(line): error: message`, like a C++ error, and warnings are errors.
- **The compiled shaders are embedded in the module that uses them.** The tool writes one C++ source file that holds the SPIR-V of every shader as arrays of numbers. The engine therefore needs no shader files beside it at run time, cannot be run with shaders that do not match its code, and contains no shader compiler.
- **The tool compiles `*.vert` and `*.frag` below one directory**, names each shader by its path below that directory, and resolves `#include` against that directory. Other stages are added to the tool when the renderer first needs them.
- **Only the first error of a shader is reported.** That is glslang's behaviour; the errors it would print after the first are mostly consequences of it.
- **File helpers shared by the build tools** (finding files in a fixed order, writing a file only when it changed) moved to `Tools/ToolSupport/`, used by the header tool and the shader compiler.

**Alternatives considered.**

- *The compilers of the Vulkan SDK (`glslc`, `dxc`).* The SDK is a large separate installation that every developer and build server would need, in a matching version. The toolchain needs none today.
- *glslang's own command-line compiler.* Building it runs a Python script; Python would become a requirement of the build for one generated header.
- *Compiling shaders when the engine runs.* It puts a compiler in every shipped game and moves shader errors from the build to the first run.
- *Committing compiled SPIR-V.* Binary files in the repository that can silently differ from their source.
- *HLSL or Slang.* Either adds a much larger compiler. GLSL is enough for the renderer the specification describes.

## D-028: The world is right-handed, Y-up and measured in metres; cameras render relative to themselves

**Status:** Accepted. Settles what D-014 left open.

**Decision.**

- **Axes and units.** The world is right-handed with +Y up. An object, and a camera, faces along its own -Z axis with +X to its right. Lengths are metres. Triangles are front-facing when their corners run counter-clockwise as seen. This is exactly glTF 2.0's convention, so imported scenes need no conversion, and no axis is swapped anywhere between a file and the screen.
- **Angles** in properties that people edit (a camera's field of view) are degrees. Rotations are quaternions.
- **`FCameraView`** is the point of view of a picture: location (`FVector`), rotation, vertical field of view and near plane. It produces the three matrices the renderer needs:
  - the *camera-relative matrix* of an object, whose translation is the object's location minus the camera's, subtracted in double precision and only then converted to float;
  - the *view matrix*, the inverse of the camera's rotation, with no translation;
  - the *projection matrix*, with reversed depth and no far plane: depth is `NearPlane / Distance`, 1 at the near plane and 0 at infinity.
- **Clip space** has y pointing up and depth from 0 to 1, the convention NVRHI gives every backend (on Vulkan it flips the viewport).
- **`FMatrix4f`** (`glm::mat4`) is the matrix type. Matrices are a renderer detail: gameplay code uses `FTransform`, and no matrix ever holds a position measured from the world origin.

**Why.** Single precision has about seven significant digits. Ten billion kilometres from the origin, the distance the specification tests at, neighbouring floats are about a thousand kilometres apart, so nothing may reach the graphics card in world coordinates. Subtracting the camera's location first leaves numbers whose size is the distance to the camera, which is where precision is needed. Reversed depth puts floating point's dense values near zero at the far end of the scene, where a conventional depth buffer has almost no precision left, and an infinite far plane costs nothing with it.

## D-029: The scene renderer

**Status:** Accepted.

**Decision.**

- **`FSceneRenderer` draws a scene from an `FCameraView` onto an `FRenderTarget`.** It walks the entity tree once per picture, combining transforms from the roots down, and draws every visible `WStaticMeshComponent` whose mesh has geometry. Each object's matrix is built relative to the camera (D-028).
- **The renderer owns the copies of meshes on the graphics card.** A `WStaticMesh` holds its geometry in main memory and knows nothing about the graphics card. The renderer keeps vertex and index buffers per mesh, creates them the first time the mesh is drawn, replaces them when the mesh's geometry version changes, and releases them when its weak reference to the mesh goes null. Assets therefore never keep the graphics card alive, and destroying the renderer releases everything it created.
- **Geometry versions are unique across all meshes.** The cache is keyed by the mesh's address, and a new mesh can be allocated at the address of a collected one; because no two geometries ever share a version, a stale entry can never be mistaken for a current one.
- **Every render target has a depth buffer** (32-bit float) and one framebuffer, created with it. All targets share their formats, so the renderer builds each pipeline once, when it is created, and reports any failure then.
- **A failed `Render` changes nothing.** Everything that can fail (an invalid view, a destroyed scene, the graphics card refusing a buffer) happens before the first command is recorded.
- **Per-object data reaches the shaders as push constants** (`FDrawConstants`: the local-to-clip matrix and the base colour), which need no buffer and no descriptor per object. (Superseded by D-030, which needs more data per object than push constants hold.)
- **Shading is unlit for now:** a surface has its material's base colour. This is the pipeline the editor's Unlit view mode will use; lighting is added beside it.
- **`FSceneRenderStatistics`** reports what the last picture drew and uploaded, so that tests can check caching and skipping, which no pixel shows.

**Testing.** The renderer is tested by drawing small scenes and reading pixels whose colour follows from the geometry: placement to the pixel, orientation, hierarchy, back-face culling, depth order regardless of draw order, an object 25 cm away together with ones two million kilometres away, and pixel-for-pixel equal pictures of one scene built at the origin, 1,000 km and ten billion kilometres from it.

## D-030: Lighting: glTF's material model, real light units, HDR and the Khronos PBR Neutral tone mapper

**Status:** Accepted.

**Decision.**

- **Materials are glTF 2.0 metallic-roughness materials:** base colour, metallic, roughness and emissive colour, shaded with the model of the glTF specification's appendix B (GGX microfacet distribution, height-correlated Smith visibility, Schlick Fresnel, Lambert diffuse). A material imported from glTF has the same numbers in the engine as in its file. Roughness is clamped to at least 0.045, below which the model's arithmetic breaks down in single precision.
- **Lights use glTF's units** (`KHR_lights_punctual`): a directional light's `Intensity` is illuminance in lux and a point light's is luminous intensity in candela, falling off with the square of the distance. A point light's optional `Range` fades it out with the window glTF recommends. The default directional intensity is pi, which lights a white matte surface that faces the light to a brightness of one, so a scene looks right before anyone has thought about units.
- **Lighting is calculated in a 16-bit floating-point buffer** that every render target carries, then a full-screen pass multiplies by the camera's `Exposure` and applies the tone mapper, writing the 8-bit sRGB surface. Light is clamped to 65,000, just under the largest 16-bit float, because an infinity in the buffer would turn into NaN in the tone mapper.
- **The tone mapper is Khronos PBR Neutral.** It leaves colours of moderate brightness almost unchanged, so materials look as authored and as glTF viewers show them, and compresses only highlights, towards white. A filmic curve such as ACES changes every colour, which is a look, and can be added as an option later; a neutral default suits an engine whose assets come from glTF.
- **Exposure is a plain multiplier on `FCameraView`,** default one. Real-world light levels need a matching exposure (sunlight of 100,000 lux needs about 1/30,000). Automatic exposure is not part of this milestone.
- **Shading happens in camera-relative space.** The vertex shader outputs positions relative to the camera along the world's axes, the camera is the origin, and point lights are placed relative to the camera in double precision on the CPU. No shader sees a world coordinate.
- **Per-mesh data is one array per picture.** The renderer fills an array of `FDrawData` (three matrices and the material), uploads it once, and draws mesh *i* as instance *i*, so a shader reads its data with `gl_InstanceIndex`. This replaces the push constants of D-029, which are limited to 128 bytes, and costs one buffer write per picture however many meshes there are. The array grows by doubling.
- **At most 4 directional and 32 point lights light one picture,** passed in one constant buffer and applied to every surface. More are ignored with a warning, once per renderer. Culling lights per object or per screen tile is not needed at this scale and can replace the loop later without changing the scene format.
- **No ambient light.** Where no light reaches, a surface shows only what it emits. Image-based lighting, which fills in the rest, is milestone 4 (D-035).
- **`ESceneViewMode::Unlit`** draws base colours straight to the surface and skips lights, exposure and tone mapping.

**Testing.** The lighting model and the tone mapper are written a second time, in C++ and double precision, in the lighting tests. A test lights a square and compares the pixel in the middle of the picture with that reference, within two steps of 255, for matte, metallic, rough and smooth surfaces, tilted and stretched surfaces, directional and point lights, ranges, several lights at once, exposure and real-world light levels. A lit scene must also come out pixel for pixel equal at the origin, 1,000 km and ten billion kilometres from it.

## D-031: Image files are read and written with stb

**Status:** Accepted.

**Dependency.** **stb** (commit `6b5799bae8a1fe3792691ef4b72ae26434c3e799`, the master branch on 2026-11-21; public domain or MIT, at the user's choice): `stb_image.h` to read PNG and JPEG and `stb_image_write.h` to write PNG. stb publishes no releases, so the pin is a commit of its master branch, with the date noted.

**Decision.**

- **`FImageFile` reads PNG and JPEG and writes PNG.** These are the two formats glTF allows for textures, and PNG, which is lossless, is what screenshots and reference images are stored as. Everything is read as four 8-bit channels into an `FImage`.
- **stb is compiled as a library of its own.** Its two headers are turned into code by one source file that the third-party CMake script writes, so stb is built without the engine's warnings-as-errors and only `ImageFile.cpp` includes its declarations.
- **The engine does the file access.** stb is built without its own file functions (`STBI_NO_STDIO`): `FImageFile` reads and writes whole files through `FFileHelper`, which handles paths that are not ASCII and replaces a file only when the new one is complete, and hands stb memory. Only the PNG and JPEG decoders are compiled in.
- **A picture's size is checked before it is decoded,** against a limit of 16,384 pixels a side, so a damaged or hostile file cannot make the engine allocate gigabytes.

**Alternatives considered.** libpng with libjpeg-turbo is faster and more complete, but is two libraries with build systems of their own and zlib beneath them, for decoding that happens once per asset at import time. Windows Imaging Component would tie image reading to Windows and to the tests' build servers' configuration. Compressed GPU textures are decided when they are needed. (Milestone 4 added Radiance HDR files, `.hdr`, through the same two stb headers: it is the format HDRIs for lighting are published in, and stb reads and writes it. OpenEXR, the other common format, would need a library of its own.)

## D-032: Textures in materials

**Status:** Accepted.

**Decision.**

- **A material has four optional textures, as in glTF:** base colour and emissive (sRGB), metallic-roughness and normal (linear). A texture multiplies the material's plain value, so a material without a texture needs no special case: it reads a white pixel. glTF's occlusion texture only affects ambient light, which the engine does not have before milestone 4, so it was added with image-based lighting (D-035).
- **`WTexture` holds its picture in main memory;** the renderer owns the copy on the graphics card, exactly as for meshes (D-029): created on first use, replaced when the picture's version changes, released when the texture is collected.
- **Mipmaps are made on the CPU** by `FMipChain` when a texture is uploaded, averaging sRGB colours as light. NVRHI has no mipmap generation on Vulkan, and doing it in the engine makes the result the same on every graphics card and testable without one.
- **Normal mapping needs no tangents.** The fragment shader derives the tangent and bitangent from how the position and the texture coordinate change between neighbouring pixels. Vertices stay 32 bytes, importing a mesh needs no tangent generation, and mirrored texture layouts come out right because the bitangent's side is taken from the layout itself. Tangents stored in a glTF file are not used; where a file's tangents were made to deliberately disagree with its texture layout, the result differs.
- **Textures are filtered trilinearly, without anisotropic filtering,** which would need a device feature the renderer does not yet ask for. Each texture has a wrap mode per direction: repeat, mirrored repeat or clamp.
- **Each material has one binding set** with its four textures and their samplers, as descriptor set 1 beside the scene's data in set 0. It is made again when the material's textures, their pictures or their wrap modes change, and released when the material is collected. A texture that could be created but not yet written, because a later step of the same picture failed, is written by the next picture.
- **A wrap mode and a texture's picture are not reflected properties yet**, so textures are not saved with a scene; they come from files, through the importer of the next step.

**Testing.** A 16 by 16 pixel texture on a surface that covers exactly 16 by 16 pixels must come out pixel for pixel. That checks orientation, wrap modes and colour space exactly. Mipmapping is checked with a one-pixel checkerboard that must come out as its average, and the lit textures against the reference model, with the normal texture on four layouts: upright, turned, mirrored and repeated.

## D-033: glTF files are imported with cgltf

**Status:** Accepted.

**Dependency.** **cgltf** v1.15 (commit `360db1a95480fe102ae9c69b27c5d101167ff5ba`; MIT), a single-header C library that parses glTF 2.0 files, .gltf and .glb, and gives access to their buffers. It is compiled as a library of its own, like stb (D-031), and only `GltfImporter.cpp` includes it.

**Decision.**

- **`FGltfImporter::Import` puts a file into a scene:** one entity named after the file, and below it one entity per node of the file's scene, with name and transform. A mesh becomes one `WStaticMesh` per part (glTF "primitive") and one `WStaticMeshComponent` per part on the node's entity; materials, textures and lights become `WMaterial`, `WTexture` and light components. What the file uses several times is created once.
- **Nothing is converted.** Axes, units, the material model and the light units are glTF's in the engine (D-028, D-030), so the importer copies numbers. A node's matrix is split into location, rotation and scale; shear, which `FTransform` cannot hold, is dropped.
- **A picture is a colour or data depending on its use.** The same image used as a base colour and as a normal map becomes two textures, one sRGB and one linear.
- **Missing normals are calculated flat,** as the glTF specification asks: every triangle gets its own vertices. Tangents in the file are not read (D-032).
- **Unsupported content is a warning, not an error.** Animations, skins, cameras, spot lights, points and lines, transparency, double-sided and unlit materials, further texture coordinate sets, texture transforms and morph targets are each left out with one warning that names them, returned to the caller and logged. A part of a mesh or a texture that cannot be read is left out the same way. An import fails only when the file as a whole cannot be used: it cannot be read, is not valid glTF 2.0, lacks a buffer, or lists an extension as required that the engine does not support; the scene is then untouched.
- **The engine does the file access.** cgltf is given a read function that goes through `FFileHelper`, so paths that are not ASCII work, and that refuses a data file shorter than the glTF file says it is, so no accessor can read beyond a buffer's end. cgltf's own validation runs before anything is created.
- **Imported assets live only in memory for now.** Entities refer to them, which keeps them alive, but a saved scene does not yet record where its meshes and textures came from; that needs the asset references that come with the editor's content browser. (D-043 gives the assets of files in the content directory paths that a scene saves.)

**Alternatives considered.** tinygltf is as widely used but brings a JSON library and image loading of its own, both of which the engine already has. Assimp reads many more formats at many times the size, and converts everything to its own conventions; the specification asks for glTF.

**Testing.** The tests build their glTF files in code, as text with embedded base64 data, with a .bin file beside it, and as .glb, so no binary fixtures are stored and each test shows what its file contains. They cover the three ways of storing data, hierarchies and transforms, sharing, flat normals, every index size, materials and textures in all three storage forms, lights, each warning, each way a file can be unusable, and drawing an imported file.

## D-034: The renderer's test scene and its reference image

**Status:** Accepted.

**Decision.**

- **The renderer has one test scene,** built in code by `Tests/Rendering/RendererTestScene.h`: a textured floor, spheres of plastic, metal and chalk, a box with a colour texture, a box with a normal texture, an emissive sphere, a point light and a directional light. It is built in code, not loaded from a file, so that it needs no assets, exercises `FMeshShapes`, and can be placed anywhere in the world by passing an origin. (D-039 adds a sky and lowers the sun.)
- **A test draws the scene without a window, 320 by 180 pixels, and compares the picture with a PNG in the repository,** `Engine/Tests/Rendering/ReferenceImages/RendererTestScene.png`, as the specification asks.
- **The comparison has a tolerance.** A pixel counts as different if a channel is off by more than 12 of 255, and at most 2% of the pixels may be different. The reference was drawn on one graphics card and the tests run on several, including software rendering in CI: lighting arithmetic differs in the last bits, and a triangle's edge may fall on the other side of a pixel centre. The tolerance is wide enough for that and far too narrow to pass a missing object, a wrong material or a light that is off, each of which changes thousands of pixels by much more.
- **The same reference serves the scene at the origin, 1,000 km and ten billion kilometres from it,** as the specification asks. The test also requires the three pictures to be equal to each other pixel for pixel on the machine that draws them, which no tolerance could hide a precision problem from.
- **A mismatch never changes the repository.** The test saves what it drew under `Build/<preset>/TestOutput/` and fails with the path of both pictures and a description of the difference. Replacing a reference image is a deliberate act: a person or agent looks at the new picture, copies it over the old one, and commits it with the change that caused it.
- **Exact tests stay the main defence.** The reference image catches what nobody thought to test; placement, depth, culling, lighting values, textures and the importer are each tested exactly or against the lighting model, by the tests of D-029 to D-033.

**Not in this milestone.** Showing the picture in a window. The specification's milestone 3 asks for headless rendering; presenting a render target on screen needs a swap chain and belongs to the first executable that has a window.

## D-035: Image-based lighting

**Status:** Accepted.

**Decision.**

- **An environment is a picture of a whole surrounding** (`WEnvironmentMap`, an `FHdrImage`), in the layout HDRIs are published in: twice as wide as high, a map of directions. The engine fixes which way round it is: the middle of the picture is straight ahead (-Z), its right half is to the right, its top row is up. `WSkyLightComponent` lights a scene with one, scaled by an intensity and turned by its entity's rotation. A scene has one sky light, its first.
- **Matte light is nine numbers per colour.** The light a matte surface gathers from its surrounding depends only on the broad distribution of the light, which three bands of spherical harmonics hold almost exactly. `FSphericalHarmonics` computes them on the CPU when the environment gets its picture, in double precision and with each pixel weighted by the part of the sphere it covers. They travel to the shader in the constants of the view; no texture is involved, and the result is the same on every graphics card and tested without one.
- **Mirrored light is a cube map blurred by roughness.** For each environment the renderer draws a cube map of 256 pixels a side with six levels: level 0 is the environment as a mirror shows it, and each further level is the environment as a rougher surface mirrors it, made by importance-sampling the GGX distribution of the lighting model with 64 samples that each read the picture at a level of detail matching their share of the sphere. A surface reads its reflection with one look-up at the level its roughness selects.
- **The share of the mirrored light a surface reflects is Karis' approximation** ("Physically Based Shading on Mobile", 2014) of the lighting model integrated over a surrounding, a short formula, instead of the usual look-up table that would have to be generated, stored and tested. The matte share is reduced by what is mirrored, so a surface never sends back more light than it receives.
- **The picture is uploaded as it is, with smaller versions,** and both the reflections and the sky are read from it by direction. The sky is a triangle over the whole screen at the farthest depth, drawn after the scene so that only uncovered pixels are shaded, at the level of detail at which a pixel of the picture covers about a pixel of the screen.
- **Light in an environment is clamped to 65,000** when it goes to the graphics card, as scene light is (D-030). A real sun in an HDRI can be brighter; it then lights a little less than it should, and nothing overflows.
- **Everything is a direction.** The sky light's place in the world is ignored, and all look-ups use directions along the world's axes, turned by the sky light's rotation, so image-based lighting is as independent of the world origin as everything else.
- **`WMaterial::OcclusionTexture`** dims the sky light's share of a surface's light, and no other, as glTF defines it. Screen-space ambient occlusion will dim the same share.

**Not chosen.** An irradiance cube map instead of spherical harmonics: a second texture and a second blur pass for a result that is less exact at low frequencies and no better at high ones. Computing the reflection cube on the CPU: deterministic, but seconds per environment in a debug build. A table for the reflectance: more exact at grazing angles on very smooth surfaces, at the cost of a generated asset.

**Testing.** Spherical harmonics are tested on the CPU against cases with known answers: an even light gathers pi times itself in every direction; a bright sky over a black ground lights what faces up fully, the side half and below not at all. On the graphics card, an even environment must light matte, metallic, rough and smooth surfaces to what the model says; an environment with a different colour in each of the six directions must show the right colour in the sky looking each way, in mirrors tilted each way, and after the sky light is turned; and a scene with a sky light must be pixel for pixel the same at the origin and ten billion kilometres from it.

## D-036: Shadows of directional lights

**Status:** Accepted.

**Decision.**

- **Cascaded shadow maps with percentage-closer filtering,** as the specification asks. Every directional light with `bCastShadows` gets four shadow maps, one for each of four slices of the view up to `FSceneRenderer::ShadowDistance` (100 m by default). The slices end at the fraction ((i + 1) / 4) to the power 2.5 of the shadow distance: about 3, 18, 49 and 100 metres. A rule that does not depend on the near plane, unlike the usual logarithmic one, so changing the near plane does not move the shadows' quality around.
- **Stable cascades.** A cascade covers the sphere around its slice, so its size does not change when the camera turns, and its position is snapped to whole pixels of its map, measured in the world in double precision; everything else is relative to the camera. Shadow edges therefore do not crawl as the camera moves. `FShadowCascades` does this arithmetic on the CPU and is tested on its own.
- **A consequence, accepted:** the grid a shadow map lies on is fixed in the world, so the same scene built in another place sits on it differently, and the soft edge of a shadow differs by a shade. A picture with shadows is no longer identical to the last bit at the origin and ten billion kilometres from it, as pictures without shadows are; it is equal within a small tolerance, which is what the specification asks. Ten billion kilometres out, double precision resolves about two millimetres, half a pixel of the nearest cascade, so shadows there can shimmer slightly when the camera moves.
- **Soft edges** come from averaging 5 by 5 depth comparisons, each of them the graphics card's blend of four, around a point. Where one cascade ends, its answer gives way to the next over its last tenth, and the last gives way to no shadow, so no line shows.
- **Self-shadowing is prevented when the map is drawn, not when it is read:** a slope-scaled depth bias of four pixels' worth of slope, slightly more than the reach of the filter, plus one pixel's worth of depth when reading. A surface then never shadows itself at any angle of the light, which the tests check across a whole wall for angles up to 85 degrees. The price is that a shadow starts a few shadow-map pixels away from where a caster touches a receiver: millimetres in the nearest cascade, centimetres farther out.
- **Both sides of a surface cast shadows.** The shadow pass does not cull, so a wall seen from behind by the light, or a one-sided mesh such as a plane, still blocks it.
- **One texture, a layer per map.** The maps are the layers of one depth texture, read through a comparison sampler. It has as many layers as the picture needs, four per shadow-casting light, and is made again only when more are needed or the size changes. `ShadowMapSize` defaults to 2048, which is 64 MB per light.
- **Point lights cast no shadows.** The specification asks for directional shadows; point light shadows need cube shadow maps and are not part of this milestone.
- **The shadow pass has its own bindings.** NVRHI counts push constants as a constant buffer and requires them to be set for every pipeline whose layout has them, so the constant that says which map is being drawn lives in a small layout of the shadow pass alone, beside the two scene buffers it reads.

**Testing.** One arrangement, a wall, an occluder floating in front of it and a sun at 45 degrees, is drawn at six sizes that put the wall into each cascade in turn: the middle of the shadow must be black and the same place on the other side must be lit exactly as the lighting model says. Further tests move the sun, check that the edge is soft and monotonic, that nothing self-shadows, that shadows end at the shadow distance, that lights and meshes can opt out, that two suns have separate shadows, that sky light and lamps shine into a sun's shadow, and that shadows fall the same ten billion kilometres from the origin.

## D-037: Screen-space ambient occlusion

**Status:** Accepted.

**Decision.**

- **A prepass draws depth and normals.** Before the lit pass, the meshes are drawn once into the render target's depth buffer and a normal texture. The lit pass then draws the same meshes at the same depths; `StaticMesh.vert` declares its position invariant so that both passes arrive at exactly the same depth.
- **The estimator is "scalable ambient obscurance"** (McGuire, Mara, Luebke, 2012): 16 samples on a spiral within a radius that is fixed in the world, `AmbientOcclusionRadius` (0.5 m by default), each counting how far the surface it finds rises above the plane of the pixel's own surface. It needs only depth and normals, works with a far plane at infinity, and gives a flat surface exactly no occlusion at any viewing angle, which a test demands of every pixel.
- **The result is smoothed in two passes,** across and down, over every second pixel up to eight away, without spreading across differences in depth of more than three percent. (Superseded by D-038, which smooths over every pixel up to four away, with weights that cancel the pattern of the samples.)
- **Ambient occlusion dims the sky light only,** like a material's occlusion texture (D-035). Light from lamps and suns is not ambient and is left alone.
- **It belongs to the render target.** The normal texture and two occlusion textures are created with each render target, and reach the lit pass as descriptor set 2. When ambient occlusion is off, the prepass is skipped and the lit pass reads a white pixel instead.
- **`ESceneViewMode::AmbientOcclusion`** shows the result by itself, for inspecting it and for testing it exactly.

**Testing.** In the occlusion view mode: an empty scene, a wall seen head-on and a floor seen at flat angles must be white in every pixel; a corner must be dark at its crease, lighter away from it and white beyond the radius; the radius and intensity must do what they say; something near the camera must not darken a wall five metres behind it; the band of occlusion must be as dark in a picture twice the size; and the picture must be pixel for pixel the same ten billion kilometres from the origin. Lit, ambient occlusion must darken a crease under a sky light and change nothing under a sun alone.

## D-038: Ambient occlusion turns its samples in a pattern that the smoothing cancels

**Status:** Accepted. Supersedes the smoothing of D-037.

**Context.** With 16 samples in the same places for every pixel, ambient occlusion shows bands, so each pixel turns its spiral of samples by its own angle and a blur afterwards averages neighbouring pixels. D-037 took the angle from interleaved gradient noise and blurred over every second pixel. That left a grain: a straight crease drawn 128 pixels high had neighbouring pixels up to 36 shades of 255 apart, and the wall above it had ripples of up to 18 shades. A blur over every second pixel averages the even and the odd pixels separately, and nothing makes those two averages equal.

**Decision.**

- **The angle comes from a pattern whose rows repeat every four pixels:** angle number (4x + 5y) mod 16, of sixteen angles a sixteenth of a turn apart. Each row holds four angles a quarter of a turn apart, and any four rows, one below the other, hold all sixteen once. Four rows down the pattern is the same again, moved one pixel across.
- **The blur takes every pixel up to four away, with the weights 1, 4, 8, 12, 14, 12, 8, 4, 1 of 64.** The pixels at each of four places in succession add up to the same share, 16 of 64, so a pass across averages the four angles of a row exactly, which leaves every fourth row the same, and a pass down then averages the four rows: all sixteen spirals, 256 samples, in equal parts. The pattern cancels instead of being softened, wherever every pixel counts in full.
- **It costs what it did:** nine pixels in each of two passes.
- **Where it is not exact.** The blur still leaves out pixels whose depth differs by more than three percent, so next to an edge, and on a surface that falls away from the camera steeply, the shares are no longer equal. What remains there is small, because a pass across has by then averaged four angles a quarter of a turn apart.

**Alternatives considered.** Blurring every pixel with the noise of D-037 removed the grain but left diagonal streaks in large pictures, because that noise changes slowly along one direction. A dither matrix in place of the lattice cancels as exactly, but its rows hold angles in two pairs instead of evenly spread, which leaves more behind wherever one of the two passes is cut short.

**Testing.** A crease that runs straight across the picture, seen level and from above: a pixel must be within 4 shades of the next along every row, where D-037 measured 20 and 36, and going up or down from the crease the picture must not get darker again by more than 2 shades, where it measured 6 and 18. The tests of D-037 hold unchanged.

## D-039: The renderer's test scene shows image-based lighting, shadows and ambient occlusion

**Status:** Accepted. Extends D-034.

**Context.** The test scene of D-034 had a sun and a lamp but no sky light, so its reference image showed neither image-based lighting nor ambient occlusion, which dims sky light only (D-037). Its sun stood high and behind the camera, so most of each shadow lay hidden behind the thing that cast it: without shadows only 1.5% of the picture changed by more than the comparison's tolerance, less than the 2% the comparison lets pass. The reference image would not have noticed shadows stop working.

**Decision.**

- **The scene has a sky,** an environment map made in code like the rest of the scene: 64 by 32 pixels, deep blue overhead, pale at the horizon, warm grey below it. It changes smoothly with height and not at all with direction, so it gives two graphics cards no sharp edge to draw differently. It lights the scene, fills the shadows, is mirrored in the gold sphere and is the background of the picture, which no longer needs a background colour.
- **The sun stands low on the right:** turned 70 degrees and 35 degrees below the horizon instead of 35 and 50. Shadows now fall to the left across the floor, beside the things that cast them and onto each other, where the camera sees them, and the fronts of the things are still in the sun.
- **A test proves that the picture depends on each of the three.** It draws the scene whole, then with the sky light, the shadows or ambient occlusion switched off, and requires more of the picture to change than the comparison with the reference image allows: 99%, 5.8% and 3.2% of the pixels, against the 2% that may differ. A reference image tests only what its picture depends on; this keeps it from quietly ceasing to test one of them.
- **The reference image was drawn by Lavapipe,** the software implementation of Vulkan that CI also uses, with the tolerances of D-034 unchanged. The image it replaces was drawn by a graphics card, and Lavapipe differed from it in 0.08% of the pixels, a twenty-fifth of what the comparison allows.

## D-040: The editor is a module of commands; its windows come on top

**Status:** Accepted.

**Context.** The specification wants every editor action to be a named command with typed arguments that returns a structured result, the user interface and the agent interface to call the same commands, and editor behaviour to be unit-tested through commands without driving the user interface. It leaves open whether the editor is part of the runtime executable or an executable of its own.

**Decision.**

- **The editor is an executable of its own, and nearly all of it is a module.** `Editor/` is a module like the engine (D-002): `WhiteWolfEditorCore.dll`, with the export macro `EDITOR_API` and its own unit tests, `EditorTests`. The executable, `WhiteWolfEditor.exe`, is a thin entry point on top of the module, because a test cannot link to an executable; it serves agents (D-042) and will open the editor's windows. The runtime executable never links to the editor, so an exported game contains none of it and there is nothing to strip.
- **`FEditor` is the editor without its windows:** the scene being edited and the commands. It needs no window, graphics card or user, so tests and agents drive all of it unattended. An editor always has a scene, keeps it alive, and destroys the scene it replaces.
- **A command is a description and a function.** The description (`FCommandDescription`) has a name of the form `Area.Verb`, a sentence that says what the command does, and parameters, each with a name, a type, a description and whether it may be left out. The function takes the editor and the arguments and returns an `FCommandResult`. Commands are registered in the editor's `FCommandRegistry`: the editor's own by the editor, and others by whatever adds them, which is how plugins will add theirs.
- **Arguments and results are JSON** (D-020), which is what an agent sends and what the user interface can build as easily. A parameter is a `Boolean`, an `Integer`, a `Number`, a `String`, or `Json` for a value whose type depends on something else, such as the value of a property.
- **The registry checks the arguments before a command runs:** every required argument is there, each has the type of its parameter, and there is none without a parameter, so that a misspelt name cannot pass silently. Commands therefore do not check their arguments, and every command reports a wrong argument in the same words.
- **A result is a success with a value or a failure with a reason,** a sentence written for whoever gave the command. A command that fails leaves the editor as it was.
- **Entities and components are referred to by their identifiers** (D-023), in arguments, in results and in the values of properties that refer to them.
- **Properties are read and changed through reflection,** with `ExportJson` and `ImportJson` (D-021), so a new component class needs no command code. Only properties that are saved with the scene can be read and changed, which are also the ones the Details panel shows; a `Transient` property is the object's own business, and the hierarchy has commands of its own.
- **The first commands:** `Editor.ListCommands`; `Scene.New`, `Scene.Open`, `Scene.Save`, `Scene.GetHierarchy`; `Entity.Create`, `Entity.Delete`, `Entity.Rename`, `Entity.SetParent`; `Component.Add`, `Component.Remove`; `Object.GetProperties`, `Object.GetProperty`, `Object.SetProperty`.

**Alternatives considered.** A class per command, with a virtual function, is more code for each command and adds nothing: a description and a function are all a command is. Commands with C++ parameter lists, checked by the compiler, would still need the check at run time, because everything that gives a command gives it as a name and JSON. Putting the editor into the runtime executable and stripping it from exported games needs conditional compilation throughout; a module that the runtime does not link needs none.

**Limits.** Milestone 5 is not finished with this. There is no window yet; undo and redo came with D-041 and the process through which an agent reaches the commands with D-042. A property that refers to a mesh, a material or a texture read as null and could not be set until assets had names that a reference can be written as; D-043 gave them those. Setting an array property to a value with a bad element fails, but leaves the array filled up to that element, as `FProperty::ImportJson` says; since D-041 that half-change is a step that can be undone.

**Testing.** The registry: finding and listing commands, every way in which arguments can fail to fit, and that a command does not run when they do. The editor: that it keeps its scene alive and destroys the one it replaces. Every command, through `FEditor::ExecuteCommand` alone: what it does, what it returns, and for each way it can fail, the reason it gives and that nothing changed.

## D-041: Undo and redo put back the whole scene

**Status:** Accepted.

**Context.** The specification wants every edit to be undoable, undo and redo to go through the command system, and stopping play to restore the scene exactly as it was. The usual design gives every command a second function that reverses it. That is two functions to keep in agreement for each command, written again by every plugin that adds commands, and a command that forgets something leaves the scene in a state it was never in.

**Decision.**

- **A step of the history is a state of the whole scene.** After every command that can change the scene, the editor writes the scene down as the JSON text of a scene file (D-024), without the layout, and compares it with the last state. If it differs, that is a step, named after the command. (Since D-054 a step can also take several commands.) `FUndoHistory` keeps the states and knows nothing about scenes.
- **Undo and redo load a state.** The scene object is replaced by one loaded from the state's text, as when a scene file is opened, so undo is exactly as good as saving and loading, which has to be right anyway and has its own tests.
- **Commands have nothing to do to be undoable,** and cannot get it wrong: not the editor's own, and not the ones plugins will add. A command that changes nothing is not a step.
- **A command that only reads says so,** with `ECommandEffect::LeavesScene` in its description, and the editor then does not write the scene down after it. That is for speed only. If such a command changes the scene after all, the change is recorded with the next step, like a change made without a command.
- **Entities and components keep their identifiers** across undo and redo, and are new objects. Whatever refers to them from one command to the next refers by identifier (D-023), as arguments and results already do.
- **A change made without a command is a step too.** Before undoing or redoing, the editor records what has changed since the last step as a step called "Direct change". What a tool does to the scene by hand, such as a gizmo while it is dragged, is therefore part of the next command's step, or undone by itself.
- **A command that fails is recorded like one that succeeds.** It should leave the scene as it was, and then there is nothing to record; if one does not, what it left behind can be undone.
- **Another scene starts the history anew.** `Scene.New`, `Scene.Open` and `FEditor::SetScene` cannot be undone.
- **The history is limited by memory,** 256 MB of state text by default. Over that, the oldest steps are forgotten, but never the last one.
- **The commands** are `Edit.Undo`, `Edit.Redo` and `Edit.GetHistory`, named after the menu the specification puts them in.

**Consequences.**

- Every command that can change the scene costs one writing-down of the whole scene, and every undo one load. Measured with MSVC in a Release build, on a scene of 1,000 entities with one component of five properties each: 3.1 ms a command, 11 ms an undo, and 296 KB a step, so that the default budget holds about 900 steps of such a scene. A Debug build takes about twenty times as long. The cost grows with the scene: ten times the entities take ten times the time for each command and leave a tenth of the steps.
- An identifier that an undone command gave out is given out again by the next command, as if the undone one had never run. This departs from D-023's "never reused" in the one case where the object that had the identifier no longer exists in any state that can be reached.
- What a scene file does not hold does not survive undo: `Transient` properties, which objects work out again, and references to assets that were made in code and so have no path (D-043).

**Alternatives considered.** A reverse function per command, for the reasons above. Recording the old and new value of each changed property, as Unreal's transactions do, needs every change to announce itself before it happens, and the first forgotten announcement breaks undo silently. Storing only the entities that differ between two states would save most of the memory, and is what to do when scenes of tens of thousands of entities need a long history; it changes how `FUndoHistory` stores a state and nothing outside it. It would not save the time, which needs the scene to know what has changed in it.

**Testing.** `FUndoHistory` by itself: steps, undo and redo in order, a new step taking the place of redoable ones, and the memory budget. Through the editor: for a deleted entity with children and components, a removed component, property values of several kinds, a reference, a name, a place in the hierarchy and a location ten billion kilometres from the origin, the scene written as a file must be the same text after undo as before the command, and after redo as after it. Also: commands that read or fail are not steps, direct changes are, another scene starts anew, a component whose class is missing keeps its data, and the old scene object is destroyed.

## D-042: Agents reach the editor through a Model Context Protocol server on standard input and output

**Status:** Accepted.

**Context.** The specification wants the commands to be reachable without the user interface, "through a command-line interface and/or a local server (e.g. an MCP server)", and an agent to build, play and export a game through that interface alone. The editor keeps its scene in memory, so whatever gives it commands has to stay connected to one running editor; starting a program once per command would not do.

**Decision.**

- **`WhiteWolfEditor --mcp` is a server of the Model Context Protocol** (MCP), the protocol through which AI agents use tools. An agent's program starts the editor as a child process and exchanges JSON-RPC messages with it, one to a line, on its standard input and output: MCP's stdio transport. There is no port to open, nothing to authenticate and nothing left running when the agent ends; the editor ends when its input does.
- **Every command is a tool,** under its own name, described by its summary, with a JSON schema made from its parameters. The tools are listed from the editor's `FCommandRegistry` each time they are asked for, so a command that a plugin registers is a tool without any code for it.
- **A command's result is the tool's result:** the value as JSON text, or, for a command that fails, its reason, marked as an error. Arguments that do not fit a command are that command's failure too, so the agent reads why and tries again. Only a tool that does not exist is an error of the protocol.
- **Both generations of the protocol are served.** A client that opens with `initialize` gets the handshake of the revisions from 2024-11-05 to 2025-11-25. A client whose requests carry their protocol version in `_meta` gets revision 2026-07-28, which has no handshake, adds `server/discover`, and wants a `resultType` in every result. The server tells them apart request by request, as the specification says a server for both should.
- **Nothing a client sends is a fatal error.** Text that is not JSON, a message that is not a request, an unknown method and wrong parameters are answered with the error codes of JSON-RPC.
- **The log goes to standard error,** warnings and errors only, because the protocol allows nothing but its messages on standard output.
- **The garbage collector gets its turn between messages,** as it does between the frames of a game.
- **The protocol is written with the engine's own JSON** (D-020), in one class, `FAgentServer`. What a tools-only server needs of MCP is a handful of messages, and a C++ library for the whole protocol would be a larger dependency than that part of the editor.
- **`FEditorApplication::Run` is what the executable does,** with the command line and the three streams as parameters, so that the unit tests cover everything but `main`.

**Alternatives considered.** A command-line program that gives one command and ends cannot keep a scene between commands without saving and loading it each time, and would put what makes a session into files. A protocol of the engine's own on the same streams would be less to write and would need an adapter for every agent. A server on a local network port can be reached by any program on the machine and needs a way to say who may.

**Limits.** The server has been tested against the messages of the specification, not yet with an agent's program at the other end. Tools carry no annotations and no output schema, and the list of tools is not announced when it changes. With `--mcp` the editor starts with an empty scene and no project. There are no commands yet for play mode, screenshots, the log, assets, scripts or export, which the specification lists; they come with the parts of the engine they act on.

**Testing.** `FAgentServer` message by message: the handshake with a known and an unknown version, `ping`, notifications and answers that get no reply, the list of tools and the schema of a command with a parameter of every type, a tool that succeeds, fails, takes no arguments or does not exist, the same requests in the revision without a handshake, an unsupported version, and every kind of message that is not a request. `FEditorApplication::Run` with streams: `--mcp`, `--help`, no option, options it does not have, and that a warning of the log reaches the error output and not the output. And the executable itself, started by CTest with a session of messages on its standard input, as `WhiteWolfEditor.ServesAnAgent`.

## D-043: Assets are the files of a content directory, and scenes refer to them by path

**Status:** Accepted.

**Context.** A component refers to its mesh and material, a material to its textures, a sky light to its environment. Those were objects that an import created in memory, with nothing to say where they came from, so a scene file could only write such a reference as null (D-033): a scene with a model in it could not be saved. The specification wants scenes as text files that agents can read, glTF models and HDRIs as the sources, and an exported game that carries its assets.

**Decision.**

- **An asset is identified by an asset path:** the file it is in, relative to a content directory, and, for a file with several assets, a key after `#`. `Textures/Wood.png` is a texture, `Skies/Evening.hdr` an environment map, `Models/Crate.glb#Meshes/0/Primitives/0` and `Models/Crate.glb#Materials/2` a mesh and a material of a glTF file, numbered as in the file. A path cannot lead out of the content directory.
- **The source file is the asset.** There is no engine format that a model or a picture is converted into and that could go stale beside its source: a glTF, PNG, JPEG or HDR file in the content directory is loaded as it is, by the code that reads those formats already.
- **`WAsset` is the base class** of `WStaticMesh`, `WMaterial`, `WTexture` and `WEnvironmentMap`. An asset that was loaded knows its path; one that was made in code has none.
- **`FAssetManager::Load` returns the asset of a path,** loading its file if need be. A file is loaded whole, into a `WAssetPackage` whose assets and the package refer to each other: while anything refers to one of them, all stay, and asking again returns the same objects. When nothing does, they are collected like any object, and the manager, which only remembers them weakly, loads the file again when asked.
- **A scene file writes a reference to an asset as its path,** a string, where a reference to an entity or component is a number (D-024). Loading a scene loads the assets it names; one that cannot be loaded is a warning and a null reference, like any value that cannot be read. The editor's property commands read and write references the same way.
- **A picture is a colour texture, or with the key `Linear` a data texture,** and becomes either when it is first asked for. What a picture's channels mean is decided by what uses it, as in glTF.
- **`FGltfImporter::Import` of a file in the content directory** gives the new components the assets of the file's package, so the scene can be saved and a file imported twice is in memory once. A file anywhere else is imported as before, with objects of its own.
- **The content directory is one setting of the engine,** `FAssetManager::SetContentDirectory`. Who runs the engine says where it is; for the editor that is the project (D-045).

**Alternatives considered.** Converting every source into a file of the engine's own on import, as Unreal does, makes loading fast and lets an asset be edited apart from its source; it also means a second copy of everything, a format to keep compatible, and a step that can be forgotten. It can be added for the kinds of asset that need it. Numbering assets with identifiers that survive renaming needs a file beside each asset to keep the number in; paths are what a person and an agent can read in a scene file. Loading single assets of a file on demand saves memory for files of which little is used, and has to parse the file again for each of them or keep it parsed.

**Limits.** Assets cannot be changed and saved: a material of a glTF file is what the file says, and the editor has no file of its own for a material yet. (The project and the commands that import and place assets came with D-045.) A file that changes on disk is not loaded again while its assets are in use. Names are compared letter for letter, which a file system that ignores case does not do. Loading happens on the main thread, when the asset is asked for.

**Testing.** Asset paths: every way text can fail to be one. The manager, with files written into a temporary content directory: each kind of file, the keys of a glTF file and of its textures, that a file is loaded once while it is in use and again after it was collected, every reason a load fails, another content directory, and the mapping between asset paths and file-system paths. Scene files: references are saved as paths and loaded as the same shared objects, the text is the same after a round trip, and an asset that is missing, of the wrong class or not a path is a warning. The importer: a file in the content directory gives components assets with paths, a scene with it saves and loads, two imports share, and a file elsewhere behaves as before.

## D-044: Vulkan is loaded once for the process and stays loaded

**Status:** Accepted. Changes how D-026 opens Vulkan.

**Context.** Every render device loaded the Vulkan loader and created a Vulkan instance, and destroyed the instance and unloaded the loader when it was destroyed. Vulkan loads the graphics driver for an instance and unloads it with the instance, so a test run loaded and unloaded the driver once for every test that draws.

The sanitizer build failed now and then with a LeakSanitizer report from `EngineTests`: some hundreds of small allocations, on five runs in two days, one of them of a commit that changed only documentation. When the annotations of CI began to show the stacks of the report, every one was the same: an allocation made in a module that was no longer loaded when the report was written, on a thread that this module had started. The engine starts no threads. This is memory of the driver, Lavapipe in CI, allocated on one of its worker threads.

A leak check finds memory by following pointers from what is loaded. Memory that only a driver's own variables point to cannot be found once the driver has been unloaded, whether the driver has done with it or not, and whether a driver is really gone from the process when it has been told to unload depends on what its threads are doing at that moment. That fits a report that comes and goes.

**Decision.**

- **The loader is loaded and one instance created when a render device first needs them, and both are kept until the process ends** (`FVulkanInstance`). Every render device of the process uses that instance. A render device still creates and destroys its own Vulkan device.
- **Nothing releases them at the end.** Destroying the instance while the process ends would unload the driver before the leak check runs, which is the situation this decision removes. The operating system takes back what a process holds.
- **The instance is the same for every render device:** it enables no layers and no extensions. Validation is NVRHI's, which belongs to the device (D-026).

**Consequences.**

- The driver is loaded once in a process instead of once for each render device. One Vulkan instance for an application is also what Vulkan intends. The engine's tests, most of which draw, run in about 25 seconds instead of 40 in a Debug build with MSVC and a graphics card, and in 20 instead of 37 in a Release build.
- A leak check now sees the driver's memory as memory the driver still holds. Should the driver lose track of memory itself, the report will name the driver's functions, because the driver is still loaded when the report is written.
- When the editor shows a window, the extensions an instance needs for that have to be enabled for the one instance, whether or not a given render device presents to a window.

**Alternatives considered.** Telling LeakSanitizer to ignore the driver is not possible for a module that is unloaded, which has no name in the report, and would hide a leak of the engine's that happened to be reported with it. Switching leak detection off for the tests that draw gives up the check for the largest part of the engine. Keeping an instance per render device and holding a second one for the process, only to keep the driver loaded, is this decision by a detour.

**Limits.** The report was intermittent, so one run without it shows little. This entry is to be amended with what CI shows over the runs that follow: either that the report has not come back, or the stack it now names.

**Testing.** The render device's tests create and destroy a render device many times in one process, which now exercises a second and later device on an instance that already exists, with NVRHI's validation on; nothing new is observable from outside. The sanitizer job of CI is the test of what this was for.

## D-045: A project is a directory with a project file and a Content directory

**Status:** Accepted.

**Context.** Assets are the files of a content directory (D-043), and something has to say where that directory is. The specification speaks of a game project that the editor opens, with a project file that will list the project's plugins, and of bringing assets in by importing them.

**Decision.**

- **A project is a directory** that holds a project file, `<Name>.wwproject`, and a directory `Content`. The project file is JSON, like every file the editor writes: its format, a version and the project's name. `Content` is the content directory, for assets and for scenes.
- **The editor works on one project at a time.** `Project.New` and `Project.Open` make a project the editor's: its content directory becomes the engine's, and the scene is replaced by an empty one, because a scene's assets are those of its project. An editor without a project can still edit a scene, as before; it cannot refer to assets.
- **A path given to a scene command is relative to the content directory** when a project is open and the path is relative, so a scene is `Scenes/Level1.json` to an agent as an asset is `Models/Crate.glb`.
- **`Asset.Import` copies a file into the content directory** and loads it, to see that it can be. A `.gltf` file takes the files along that it keeps its data in, at the same places beside it; one that keeps data outside its own directory is refused. Nothing of the project is replaced, and a file that cannot be loaded is not left behind.
- **`Asset.Place` puts a model into the scene** as entities, through `FGltfImporter::Import`, and is a step of the history like any change. Single assets are used by setting the properties of components to their paths.
- **`Asset.List` and `Asset.Describe`** say what the project has: the asset files, and the path and class of every asset in one of them.

**Alternatives considered.** A content directory that is set by a command of its own, without a project, would have had to be replaced by the project as soon as plugins and export need one. Importing by reference, leaving the file where it is, makes a project that works on one machine. Converting on import is what D-043 decided against.

**Limits.** The project file holds nothing but the name yet. There is no command that moves, renames or removes an asset file, and a scene that names an asset by its old path does not follow it. (That nothing remembered which project was open last was a limit until D-063.)

**Testing.** `FProject`: creating and opening, and every way either can fail. The commands, on a project in a temporary directory: importing each kind of file, a `.gltf` file with a buffer and a picture beside it, every reason an import is refused and that nothing is left in the project then; listing and describing; placing a model at the root and below an entity, undoing it, and saving and opening the scene with it; and scenes by relative path.

## D-046: User interfaces are drawn by the engine, from lists of triangles

**Status:** Accepted.

**Context.** The editor needs a user interface, and so will games. Whatever lays a user interface out, a library or code of the engine's own, what comes out of it is the same: flat triangles in the pixels of a surface, in colours, covered with pictures, clipped to rectangles. Only `Engine/Private/Rendering` may know NVRHI and Vulkan, so the module that lays out the editor's user interface cannot draw it. And the specification wants pictures of the editor compared with reference images in the test run, where there is no window.

**Decision.**

- **`FUiRenderer` draws an `FUiDrawList` onto a render target.** The list is vertices (a position in pixels, a texture coordinate, a colour), 32-bit indices, and commands that each draw a run of triangles with one picture inside one clip rectangle. That is the form user-interface libraries produce, and the engine does not know which one made a list.
- **It draws onto a render target, as the scene renderer does,** over what the target shows. A user interface can therefore be drawn and read back without a window, and whatever shows a render target in a window shows a user interface.
- **Colours are sRGB-encoded where they are given and blended in linear light.** A vertex colour is an `FColor`, as colours are picked and stored; the vertex shader decodes it, and the surface encodes what is drawn. An opaque colour is read back as the colour that was given. Where colours are translucent, the result is that of blending light, not of blending encoded numbers, which is what many user-interface libraries assume: translucent overlays come out a little lighter than such a library's own pictures of itself.
- **A picture is a `WTexture`, or the surface of another render target.** Fonts, icons and thumbnails are textures like any other, through the cache the scene renderer uses (`FTextureCache`, which was part of the scene renderer until now). A render target is shown as an editor's viewport shows the scene, with its alpha ignored.
- **A list is checked before any of it is drawn:** every index, every range, every offset. A list that is not consistent is refused as a whole with a message that names the command.

**Alternatives considered.** Giving the editor module access to NVRHI would put knowledge of the graphics interface into a second module. Letting a user-interface library draw with a Vulkan backend of its own would put third-party code in charge of part of every frame, outside NVRHI's tracking of what the graphics card holds, and tied to a window. Drawing user interfaces with the scene renderer, as meshes, would bend a renderer built around depth, lighting and materials to something that needs none of them.

**Limits.** There is one picture to a command and no way to give a shader of one's own. Text is whatever triangles and font picture the layout produces; the engine has no notion of text.

**Testing.** Lists of rectangles on a surface of sixteen by sixteen pixels, read back and compared pixel for pixel: which pixels a rectangle covers, from which corner positions count, that an opaque colour is read back as given and a translucent one blends as light does, the order of commands, clip rectangles inside, across and outside the surface, a texture shown pixel for pixel and multiplied by a colour, a texture's alpha, a texture whose picture is replaced, a render target shown and then drawn to again, a command's own part of a list, a list larger than the buffers, and each way a list can be inconsistent, with nothing drawn then. All with NVRHI's validation on and no error logged.

## D-047: What is selected belongs to the editor, not to the scene

**Status:** Accepted.

**Context.** The editor's windows act on the selected entities: the outliner highlights them, the details panel shows their properties, the gizmo moves them, and Delete, Duplicate and Focus mean them. An agent that places things for a user to look at wants to select them too. Undo and redo put back the whole scene from the text of a scene file (D-041), and what is selected is not in a scene file.

**Decision.**

- **`FEditor` holds the selection:** the identifiers of the selected entities, in the order they were selected. Whatever shows one entity shows the last.
- **`Selection.Set` and `Selection.Get` are its commands,** like everything else the user interface does (D-040). `Selection.Set` takes the whole list; adding to a selection or taking from it is the caller's arithmetic, so there is one way to change it. Both leave the scene as it is (`ECommandEffect::LeavesScene`).
- **Selecting is not a step of the history.** Undo undoes changes to the scene, and a user who undoes a move does not expect to get an earlier selection back first.
- **The selection is identifiers, looked up when it is asked for.** An entity that has been destroyed is simply not among the selected ones any more, and when an undo brings it back, with the identifier it had (D-041), it is selected again. Nothing that destroys or restores entities has to tell the selection.
- **A scene that replaces the one being edited starts with nothing selected,** because the same identifiers name other entities there. An identifier that no entity has is not accepted, so that an entity created later cannot turn out to be selected.

**Alternatives considered.** Saving the selection with the scene would put a user's momentary attention into a file that is shared and compared. Making the selection part of the history's states, as editors that record every change as a transaction do, would make each click a step that an undo has to pass through. Selecting components as well as entities is left out until something needs it: the details panel shows the components of the selected entity.

**Testing.** That the selection is what was set, in order and without duplicates; that identifiers of no entity and of components select nothing; that a destroyed entity drops out and an undo brings it back; that selecting adds no step to the history; that a new scene starts with nothing selected; and for `Selection.Set`, each kind of argument that does not select entities, with its reason and the selection left as it was.

## D-048: The viewport's camera and view mode are the editor's, with commands of their own

**Status:** Accepted.

**Context.** The editor's viewport shows the scene from a point of view that the user steers, in a view mode that the user picks. The specification wants every editor action to be a command and an agent to be able to capture a screenshot, which is only of use if the agent can say from where. And the tests of the viewport have to know exactly what it shows.

**Decision.**

- **`FEditor` holds the viewport's camera and view mode,** though it has no viewport: they say what a picture of the scene being edited shows, whoever draws it, a window or a screenshot.
- **The camera is an `FViewportCamera`:** a location in world space (double precision, like every position), a yaw and a pitch in degrees, a field of view and an exposure. Two angles and not a rotation, because that is how a viewport is steered and what a person can read and type: yaw turns around the world's up axis, counter-clockwise seen from above, from looking along -Z (D-028); pitch looks up from the horizon and stops one degree short of straight up and down, where left and right would stop meaning anything. The horizon is always level.
- **`Viewport.GetCamera`, `Viewport.SetCamera`, `Viewport.GetViewMode` and `Viewport.SetViewMode`** are the commands. `Viewport.SetCamera` changes what it is given and leaves the rest, and takes `LookAt`, a point to face, in place of the two angles, because an agent that wants a picture of something knows where the thing is and not which angles face it.
- **Neither is part of the scene.** They are not saved with it, changing them is not a step of the history, and they stay as they are when another scene is opened. The user interface moves the camera every frame while the user flies through the scene, and does that by setting it on the editor directly: a command is for an action, and a frame of flying is not one. Only what changes the scene has to be a command (D-040).
- **A new scene is first seen from a short way off, looking down at its origin** from a diagonal, so that what is placed there shows its shape.

**Alternatives considered.** A camera entity in the scene would be saved with it and shipped with the game. Keeping the camera in the user interface would leave tests and agents without a way to say what the viewport shows, and a screenshot command without a point of view. A quaternion for the direction can roll the horizon and cannot be typed.

**Limits.** There is one viewport and one camera. Exposure is set by hand: nothing measures the brightness of the scene.

**Decided later.** How the user steers the camera with the mouse and the keyboard, and what the camera looks at, is D-059.

**Testing.** The camera: the directions that each angle gives, that the right-hand direction stays level, `LookAt` in each direction, far from the origin, straight up and down, and at the point the camera is at, the view it gives the renderer, and each number that makes it invalid. The commands: what they return, that `Viewport.SetCamera` changes only what it is given, `LookAt` with and without a location, every argument that is refused with its reason and the camera left as it was, including numbers that are not finite, each view mode by name, a name that is no view mode, and that none of it is a step of the history.

## D-049: The editor's user interface is laid out with Dear ImGui and drawn by the engine

**Status:** Accepted.

**Context.** The specification describes the editor's main window after Unreal Engine 5: a menu bar, a toolbar, the viewport, an outliner, a details panel, a content browser, an output log and a status bar, as tabs that can be dragged, split, floated and closed, in a dark theme. It wants pictures of that layout compared with reference images in the test run, where there is no window, and leaves the library for the user interface to be chosen: small, widely used and permissively licensed.

**Decision.**

- **Dear ImGui lays the user interface out.** Version 1.93.0 from the library's docking branch (tag `v1.93.0-docking`, commit `b916186e66aa3c02b72c6ca17455516512ed8049`), under the MIT licence. The docking branch is the one with windows that are dragged into tabs, split apart and floated, which is the specification's layout; the library's main branch has none. The library is used by a great many tools of the games industry, comes as five source files without dependencies, and is fetched and pinned like every other library (D-005).
- **It is an immediate-mode library, which suits an editor whose scene is replaced under it.** A panel is code that runs every frame and reads the scene as it is; there are no widget objects holding a second copy of the scene that would have to be kept in step. Undo and redo replace the scene object with all its entities (D-041), and an agent may change anything between two frames; a panel that looks again every frame shows either without being told.
- **The library only lays out. The engine draws, and the engine's input state is the input** (D-046). None of the library's ready-made connections to a window system or a graphics interface is used: `FUiInput` gives it a frame of keys, mouse and typed text from an `FInputState`; `FUiTextures` keeps the textures it asks for, those of its fonts, as `WTexture` objects; `FUiDrawData` turns what it laid out into an `FUiDrawList` that `FUiRenderer` draws onto a render target. The user interface therefore needs no window: it is drawn, driven and read back in the tests like any other picture.
- **`FEditorUi` is the whole user interface as one class of the editor module,** with `Draw(Target, Input, DeltaSeconds)`. No Dear ImGui type appears in a public header, and only `Editor/Private/Ui` includes the library, through one header. The library is linked into the editor's DLL and into nothing else: the engine, the runtime and exported games do not contain it.
- **The library is built by this repository, configured by a header that CMake writes.** A failed assertion of the library is reported like a failed check of the engine, in every build configuration, because it means the same: the code that uses the library is wrong. The library is given no way to reach the clipboard, files or other programs by itself, so that a test cannot touch the machine it runs on and the editor behaves the same with a window and without one. Functions that the library has replaced are left out, as are its demonstration windows. Characters beyond U+FFFF are text like any other, and indices are 32 bits wide, as in `FUiDrawList`.
- **A widget changes the scene by giving the editor a command** (D-040), through `FUiCommands`, and reads the scene directly. Keyboard shortcuts belong to the entries of the menu bar and go through the library's routing of shortcuts, which lets a text field that is being typed into keep Ctrl+Z for itself.
- **Items have names.** A panel gives the items that matter a name that says what they are (`"Outliner/7"`, `"Menu/Edit/Undo"`), and `FEditorUi::FindItem` says where a named item was drawn. That is how a test finds what to click without knowing the layout, and what makes it possible to test the user interface by using it.
- **The default layout is built in code** with the library's dock builder, which the library keeps in its internal header and may change between versions; updating the library means checking that call.
- **`FUiTheme` holds the look:** dark grey panels, light grey text and a blue accent, as sRGB values. The font is the scalable one that comes inside the library.

**Alternatives considered.** Qt is the usual choice for a desktop editor, but it is large, brings its own windows, rendering and build tools, and its licence is the LGPL or a commercial one. Libraries that keep a document of widgets and style them with style sheets, such as RmlUi, are made for the user interfaces of games: they have no docking, and a document is a second copy of the state to keep in step. The windows and controls of the operating system cannot be drawn onto a render target and compared with a reference image, and would tie the editor to one operating system's programming interface. Writing the user interface from nothing means writing text fields, trees, menus and docking, each a project of its own. Using the library's own Vulkan and GLFW connections was set aside with D-046.

**Limits.** This is the first of the user interface, not all of it: the menu bar has File, Edit, Window and Help, and the panels are the viewport and the outliner. The toolbar, the status bar, the details panel, the content browser, the output log and the Tools and Build menus come as what they show is built; a menu or panel is added when it has something real in it. The layout is not yet saved. Panels float inside the main window and not as windows of the desktop. The library's own font is a programmer's font in which every letter has the same width; a font made for user interfaces is to replace it. Keys are positions on a US keyboard (D-025), so on a keyboard layout that puts letters elsewhere the shortcuts are where the letters are on a US keyboard. Until the platform layer offers the system's clipboard and mouse pointers, copying and pasting works inside the editor only and the pointer does not change its shape over what can be dragged. A font texture is copied to the graphics card whole whenever characters are added to it. And translucent parts come out a little lighter than the library's own pictures of itself (D-046).

**Testing.** `FEditorUiTest` is an editor with its user interface on a render device with validation on, which a test drives with a mouse and a keyboard that exist only in the test. A picture of the default layout, with a small lit scene in the viewport and a tree of entities in the outliner, is compared with a reference image (D-034). Further: that the user interface covers a surface of any size, including one too small for anything, and is laid out again when the size changes; that the viewport shows the scene from the editor's camera in the editor's view mode, checked by the colour of a pixel; that the outliner lists entities as a tree, selects on a click, adds and removes with Ctrl held, selects nothing on a click beside the rows or on a row's arrow, and lists what its search field finds; that Ctrl+Z, Ctrl+Y, Ctrl+Shift+Z and Ctrl+N give their commands, that the same letters without Ctrl do not, and that Ctrl+Z in a text field undoes typing and not the scene; that the entries of the menus do what the shortcuts do, and nothing when there is nothing to undo; that the Window menu closes and opens panels and brings back the default layout exactly; that the window about the editor keeps the shortcuts from the scene until it is closed, by its button or by Escape; that nothing is highlighted once the mouse has left; and that a user interface can be created again after one was destroyed. All without an error in the log.

## D-050: A window is shown pictures by a swap chain that the engine makes with Vulkan itself

**Status:** Accepted.

**Context.** Until now nothing the engine drew reached a screen: everything is drawn onto render targets, without a window, and read back (D-026). The editor needs a window. NVRHI draws and leaves showing what was drawn to its user: its Vulkan side has no swap chain, only the two calls with which a submission waits for a semaphore or signals one.

**Decision.**

- **`FSwapChain` shows a render target in a window, and does nothing else.** `Present` copies the finished picture onto a surface of the window, pixel for pixel from the top left corner. Nothing is ever drawn into a window directly. What a window shows is therefore exactly what a test reads back from a render target, the renderers know nothing about windows, and a program without a window, such as a test or an agent's editor, is the same program minus its last step.
- **The surface and the swap chain are made with Vulkan's own functions,** and the surfaces of the swap chain are then given to NVRHI as textures (`createHandleForNativeTexture`) that it draws the copy to and hands back in the state the window system shows them in. Semaphores keep the two in step: the copy waits for the surface to be free, and the window system waits for the copy.
- **Vulkan is started with what windows need, wherever the machine has it.** The process has one instance, created before anything asks for a window (D-044), so it is created with the surface extensions that exist: `VK_KHR_surface` with `VK_KHR_win32_surface` on Windows and with `VK_EXT_headless_surface`. The device is opened with `VK_KHR_swapchain` if it has it. A machine that has none of this still draws; it only cannot show, and `FSwapChain::IsSupported` says so.
- **A window that exists in memory only gets a headless surface where the driver has one.** That is a surface that shows nothing anywhere but takes pictures like any other, so the whole path runs without a display. Software drivers such as Lavapipe have it; the drivers of most graphics cards do not. `GLFW` is asked for nothing but the window's handle (`FWindow::GetNativeHandle`): the engine creates the surface itself, with the instance it already has.
- **One picture for each refresh of the display, in order** (`VK_PRESENT_MODE_FIFO_KHR`), which every window system has. `Present` therefore returns at the display's pace, and that is what paces a program that draws in a loop. At most three pictures are on their way at once; the processor waits for the oldest to leave the graphics card before it starts another.
- **The surface is eight bits per channel, sRGB.** A format that the graphics card encodes is preferred, because a render target's pixels are such a format; where a window offers only formats that store what is written, the copying shader encodes.
- **The swap chain follows its window.** A window that was resized, or whose surface the window system declares out of date, gets a new swap chain at the next `Present`. A window without an area is shown nothing. For the frame in which sizes differ the picture is shown in the corner, cut off or surrounded by black, never stretched.

**Alternatives considered.** Drawing the user interface straight onto the swap chain's surfaces would save the copy of one picture per frame, and would make a window something the renderers have to know, and what is shown something no test can read. Letting GLFW create the surface would have it load Vulkan a second time or be handed the engine's loader before it starts, to save a dozen lines. Showing a new picture whenever one is ready, without waiting for the display, tears, or needs a present mode that not every system has.

**Limits.** A render target that is shown in a window carries the buffers of a render target that a scene is drawn onto, which a picture of a user interface does not need. There is no way yet to choose not to wait for the display.

**Testing.** In the test run windows exist in memory only (D-025), so the tests do what the driver allows: where it has a headless surface, as Lavapipe does in continuous integration, a swap chain is created, shown twelve pictures in a row, resized up and down with pictures of the wrong size, used after its render device is gone, and used beside a second one for another window, all with validation on and no error in the log; where the driver has none, creating one has to fail with a sentence that says so and names the graphics card. The path that only a window on a desktop takes, creating a Win32 surface, cannot run in the test run. It was run by hand on a machine with a graphics card, with validation on: sixty frames in a window that was resized in between, without an error.

## D-051: The editor's window is a class that runs without a display, and the executable opens it

**Status:** Accepted.

**Context.** The editor's user interface is drawn onto a render target (D-049) and a swap chain shows a render target in a window (D-050). What remained was the program around them: a window, a loop, and a way to start it. The window on a desktop is the one thing a test run cannot have.

**Decision.**

- **`FEditorWindow` is the editor's main window:** a window of the platform layer, the render device, the swap chain and the user interface, and `RunFrame`, which lays the user interface out for the window's size, draws it with the window's input and shows the picture. The title bar names the scene and the project.
- **It runs the same in the platform layer's headless mode,** where the window exists in memory only: it has a size that can change and input that can be given, and every frame is drawn. The picture is then shown nowhere, unless the graphics driver has a surface for such windows (D-050), and `GetPicture` is what a window on the desktop would show. All of the editor's window except the desktop itself is therefore in the test run.
- **Started without an option, the executable opens the window** and runs frames until the window is closed: the events of the window system, a frame, a step of the garbage collector. Showing a picture waits for the display, which paces the loop; a minimised window is shown nothing, so the loop rests between looks at it.
- **`WhiteWolfEditor --screenshot <file>` draws the window without opening one** and saves the picture as a PNG file. It is the headless mode from the command line: a way to see the editor on a machine without a display, for a person, for an agent and for the documentation, and the whole path of the program from its command line to a picture in the test run.
- **Exit codes:** 0 if the editor did what was asked, 1 if it could not (no display, no usable graphics card, a picture that cannot be saved), with the reason on the error stream, and 2 for a command line it does not understand, as before. The warnings and errors of the log go to the error stream.
- **It is still one executable, and a console program.** `--mcp` and `--help` need the standard streams of the terminal or agent that started it. Started from the desktop, the editor therefore comes with a console window, which shows the warnings and errors of the log.

**Alternatives considered.** A loop in the executable around a window, a device and a user interface that the executable owns would be the usual shape, and none of it could be tested. A second, windowless executable for agents and tools would spare the editor its console window, and is what exporting a game will need for the game's own executable; until then one program is simpler to build, document and start.

**Limits.** Closing the window ends the editor without asking about changes that were not saved. (That the window opened small and unscaled was a limit until D-052.) (That the editor did not ask for a project at the start was a limit until D-064.)

**Testing.** The window in memory: that it opens at the size and with the title it should have, that a frame draws the user interface at the window's size and the picture follows when the size changes, that the title follows the scene and the project, that a click made with the window's mouse selects an entity, that frames are counted as shown only where they can be, that a request to close is seen, that it cannot be opened without the platform layer or with a size of nothing, and that it can be opened again. The application: that `--screenshot` saves a picture of the right size that shows something and leaves the platform layer shut down, that it says why when the picture cannot be saved, and that a screenshot without a file name is a command line it does not understand. The window on the desktop was started by hand: the executable opened its window, showed the user interface and ended when the window was closed, without a warning in the log.

## D-052: The editor is scaled to its display, and opens maximised

**Status:** Accepted.

**Context.** Displays differ in how many pixels they have to the centimetre, and their users set a scale for each: 125 %, 150 %, 200 %. A program that tells the operating system it understands this, which GLFW does for every program that uses it, is given the display's real pixels and has to draw larger by itself; one that does not is drawn small and stretched by the system, which blurs it. The editor drew everything at one size, so on a dense display it was small.

**Decision.**

- **A window says what its display's scale is:** `FWindow::GetContentScale`, from the window system, 1 for the usual pixel density. It follows the window to another display.
- **The user interface has a scale,** `FEditorUi::SetScale`, by which every size of the theme is multiplied. Letters are drawn anew for the size they come out at and not stretched, which the library does for any size that is asked for. `FEditorWindow` gives the user interface the scale of its window's display in every frame, so moving the window to another display is followed without anything else knowing.
- **Sizes are written for a scale of one, in the theme.** A panel that needs a size of its own takes it from the font's height or the style, which are scaled, so that there is one place where the scale is applied.
- **On the desktop the editor's window opens maximised** (`FWindowDescription::bMaximized`), as a program to work in for hours does. `--screenshot` draws at the size the window is restored to, at a scale of one, so that its picture is the same on every machine.

**Alternatives considered.** Leaving the stretching to the operating system needs the program to deny what GLFW says for it, and blurs every letter. A scale that the user sets in the editor, in place of the display's, is what some editors have in addition; it can multiply this one when it is wanted.

**Limits.** Pictures that are not letters or shapes, such as icons and thumbnails, will need to exist at the sizes that scales ask for when the editor gets them. The scale of the display is the only scale: the user cannot add one of their own.

**Testing.** A window in memory has a scale of one and can be opened maximised. The user interface: at twice the scale its items are twice as large, within the tenth that letters drawn for their size differ by, and the panels still share the surface; set back, it draws the picture it drew at first, to the pixel; a scale beyond the limits is the nearest limit and one that is no number is one, and at both limits it is still drawn without an error. The editor's window gives the user interface its own scale in every frame, whatever the user interface was set to, and can be opened maximised. On a display set to 125 % the executable was started by hand: its window opened maximised with the user interface a quarter larger and sharp.

## D-053: Entities are copied through their saved properties

**Status:** Accepted.

**Context.** The specification's editor duplicates entities (Ctrl+D, Alt + drag), copies and pastes them, and scripts will spawn entities from prefabs. All of these make new entities that are like existing ones. A scene is already written down and read back by reflection (D-024), with no code per component type.

**Decision.**

- **`FSceneSerializer::DuplicateEntity` copies an entity with its components and all its descendants** within its scene. The copy has the name and transform of the original, becomes the last child of the same parent, and every copy has an identifier of its own.
- **What is copied is what is saved:** each `WPROPERTY()` that is not `Transient` is written by the original's property and read by the copy's, exactly as a scene file is, except that nothing goes through text. A component type is therefore copied without code of its own, and what copying misses is what saving would miss, which the tests of saving already guard.
- **References are followed by object, not by identifier.** Where a copy refers to an entity or component that was copied with it, it refers to that one's copy, so that a copied character's camera arm looks at the copied character. A reference to anything else is to the same object as the original's: an entity outside the copied tree, or an asset, including one that was made in code and has no path, which a scene file could not hold.
- **Components hear that they are attached once every property of every copy is set,** as when a scene is loaded.
- **A component whose class is not available** (D-024) is copied as the data the scene file had for it.
- **The editor's command is `Entity.Duplicate`,** which returns the identifier of the copy.

**Alternatives considered.** A virtual `Clone` function on components is code per component type, and the first one that forgets a member copies wrongly without anyone noticing. Writing the entities as scene text and reading that back would reuse more of the loader, but identifiers in text mean the scene they were written in: the same text read into another place needs every identifier replaced and every reference decided on again, which following the objects does directly.

**Limits.** A copy is made in the scene of the original. Copy and paste between scenes, and prefabs, need the copied entities as a document that outlives the scene; they will share the property copying and add that document.

**Testing.** That a copy has the name, the transform, at a location far from the origin, and an identifier of its own; where it is put among its siblings and at the root; that components are copied with their saved properties, of a derived class too, and without the ones that are not saved, leaving the original as it was; that descendants are copied in order with their components; that references inside the copied tree, strong, weak and in an array, lead to the copies and those outside it to the same objects; that an asset without a path is shared, without a warning; that components are attached after their properties are set; that a component of a missing class keeps its data; that a copy of a copy is its own; and that a destroyed entity is refused with a warning. For the command: the copy and its identifier, one step of the history that an undo takes back whole, and the reasons for refusing.

## D-054: A step of the history can take several commands

**Status:** Accepted.

**Context.** Every command that changes the scene is a step of the history (D-041). What a user does as one thing is not always one command: Delete with three entities selected is three `Entity.Delete` commands, and dragging an entity with the gizmo sets its transform in every frame of the drag. Undo has to take such a thing back at once.

**Decision.**

- **`FEditor::BeginStep(Name)` and `EndStep()` make the commands between them one step.** In between, commands change the scene as always but are not recorded one by one; `EndStep` writes the scene down once, and what differs from before the step is one step with the given name. Because a step is a state of the whole scene, nothing else is needed: there are no commands to gather or to reverse in order.
- **A step in which nothing came out different is no step,** as with a command.
- **Steps do not nest.** Beginning one while another is open is a mistake in the code and a fatal error. A function that wants its commands in one step is told so by its caller, or uses `FScopedEditorStep` at the one place that knows it is the outermost.
- **What ends a step besides `EndStep`:** `Undo` and `Redo`, which first make what the open step has changed so far a step, so that pressing Ctrl+Z in the middle of a drag undoes the drag; and a scene that replaces the one being edited, which leaves nothing to undo.
- **What changed without a command before the step** is recorded first, as a step of its own, as it is before an undo.

**Alternatives considered.** Commands that take a list, such as `Entity.Delete` with several entities, would cover the first case and not the drag, and every command would need its list form. Merging steps afterwards by name and time, as text editors merge typing, guesses at what the user meant.

**Limits.** Agents cannot yet make several commands one step: a step that an agent begins and forgets to end would swallow everything after it.

**Testing.** That commands in a step are one step with its name, which one undo takes back and one redo brings back; that a step that changed nothing in the end is none; that commands after a step are steps again; that a direct change before a step is a step of its own; that an undo in an open step ends and undoes it, and a redo ends it in place of what could have been redone; that another scene ends an open step with nothing to undo; that ending without beginning does nothing and beginning twice is a fatal error; and `FScopedEditorStep`.

## D-055: The outliner is where the tree of entities is edited

**Status:** Accepted.

**Context.** The specification's outliner shows the scene's entities as a tree, with search, dragging to change parents and a menu on the right mouse button, and its shortcuts are Delete, Ctrl+D and F2. Until now the outliner listed and selected; nothing in the editor's window could create or change an entity.

**Decision.**

- **A right click opens a menu about what is selected,** after selecting the row it was on if that was not selected: Create Child Entity, Rename, Duplicate, Delete. On the empty part of the panel the menu creates an entity at the root. A new entity is called "Entity" and is selected, as copies are after a duplication: what was made is what the user goes on to work with.
- **F2, Ctrl+D and Delete are entries of the Edit menu** and work wherever the keyboard is, except in a text field, which keeps Delete for its letters.
- **A name is typed where it stands.** F2, or Rename, turns the row into a text field with the old name selected. Enter gives the entity the name, and so does going elsewhere; Escape leaves it. A name confirmed as it was changes nothing and is no step.
- **Dragging a row onto another makes it that entity's child,** in the same place in the world, and dragging it onto the empty part of the panel gives it no parent. One of several selected rows takes the others along. A row cannot be dropped on itself or on what is below it, and such a place does not light up as one where it could.
- **Pressing the mouse button on a row that is selected with others does not yet select it alone,** because the press may begin a drag of all of them; the release does, if the mouse has not moved.
- **Each of these is one step of the history, named for what was done** ("Create Entity", "Rename", "Duplicate", "Delete", "Attach"), whatever number of commands it took (D-054). Where the selection holds an entity and something below it, the entity is acted on and takes what is below it along.
- **The keyboard moves through menus and panels** as it does elsewhere: the library's keyboard navigation is on, which is also what makes Escape close a menu.
- **Text fields work on `std::string`,** through the part of the library made for it, so that a name is as long as it is.

**Alternatives considered.** A dialog for a name is one more window to answer for the commonest change. Deleting and duplicating with one command per entity would leave three steps to undo for one key. Selecting on the press in every case is simpler and makes it impossible to drag several rows.

**Limits.** The outliner has no toggles for visibility yet, no way to select a range with Shift, and no way to put an entity between two others: a dropped entity becomes the last child. A copy has the name of its original.

**Testing.** With the mouse and keyboard of `FEditorUiTest`: the menu of a row, which selects it, and of several selected rows, which stay selected; creating below a row and at the root, with the selection and the names of the steps; renaming by F2 and by the menu, onto the end of the old name, cancelled by Escape, confirmed by a click elsewhere, confirmed unchanged, and with the entity taken away meanwhile; Delete with a parent and its child among what is selected, as one step that one undo takes back; Delete with nothing selected and in a text field; Ctrl+D with the copies selected; dragging onto a row and onto the empty part, with the place in the world kept; a drop refused on what is below the dragged row; several selected rows dragged by one; a click on one of several selected rows; and the entries of the Edit menu, with and without a selection.

## D-056: A rotation is shown as pitch, yaw and roll in degrees

**Status:** Accepted.

**Context.** Rotations are quaternions (`FQuat`, D-028), which nobody reads or types. The editor's Details panel has to show the rotation of an entity as numbers and take numbers for it, and scripts will want to say "turned 90 degrees to the left".

**Decision.** `FRotator` (`Math/Rotator.h`) is a rotation as three angles in degrees, with `ToQuat` and `FromQuat`.

- **Each angle turns around one axis of the world's convention** (D-028: +X right, +Y up, -Z forward), counter-clockwise as seen with the axis pointing at the viewer: `Pitch` around +X tilts what faces forward upwards, `Yaw` around +Y turns it to the left, `Roll` around +Z tips an object onto its left side. In a panel they are the X, Y and Z of a rotation.
- **Roll is applied first, then pitch, then yaw:** the rotation is yaw times pitch times roll. The yaw therefore always turns around the up axis, as a turret or a person's heading does, and the two angles that are used most, heading and tilt, mean what they say whatever the third is. It is the order in which the viewport's camera is turned and tilted (D-048).
- **`FromQuat` gives a pitch from -90 to 90 and a yaw and roll from -180 to 180.** Many sets of angles are one rotation, so the angles that went in need not be the ones that come out. Within about a tenth of a degree of straight up or down, where yaw and roll turn around the same axis and a quaternion's single precision cannot tell them apart, the pitch is exactly 90 or -90, the roll is 0 and the yaw is what the two do together.
- **It is not a property type and nothing stores one.** A rotator is for the moment a person looks at a rotation or types one. Whoever shows angles while they are being changed keeps the angles for that long, because reading them back from the quaternion in every frame would make a number that is dragged past 90 degrees jump.

**Alternatives considered.** glm's own functions for angles use another order (roll around Z last), in which a yaw of a tilted object is not a turn around the up axis. Storing angles in the transform, as Unreal does with its rotator, makes typed angles come back as typed, but then every rotation that is calculated has to be turned into angles, which loses precision and has no single answer. An axis and an angle are exact but not how people think of the way an object faces.

**Testing.** What each angle does to the forward, right and up directions; that yaw turns around the up axis whatever the other angles are; that angles in range come back as they went in; that angles out of range and arbitrary quaternions come back in range and as the same rotation; and both vertical cases, with the yaw taking over the roll.

## D-057: The components that can be added are the classes that derive from WComponent

**Status:** Accepted.

**Context.** An agent could add a component only if it knew the name of its class, and the Details panel needs the list to offer. Plugins and game modules will bring classes of their own, so the list cannot be written down anywhere.

**Decision.**

- **`WClass::GetAllClasses` returns every registered class,** of every module that is loaded, a class after its base class. It is how anything finds the classes that derive from one.
- **A class can be added if it derives from `WComponent`, is not abstract, is not `WComponent` itself and is not `WMissingComponent`.** The base class holds nothing and would only be a mistake in a scene. `WMissingComponent` stands in for a class that a scene file names and this build lacks (D-024); one that was added by hand would be saved under no class name at all. `FComponentClasses::CanBeAdded` is that rule, with a sentence for each refusal, and `Component.Add` refuses by it: before, it accepted both classes.
- **`Component.ListClasses` returns the names of the classes that can be added,** in the order of the names. It is `FComponentClasses::GetAddable`, which the Details panel offers from too, so that what the panel lists is what the command accepts.
- **What a class holds is not listed.** An agent adds a component and asks for its properties, which it has to do to work with it anyway.

**Alternatives considered.** A specifier on `WCLASS()` that hides a class from the list is what a second class that must be hidden will justify; for two classes of the engine it is machinery in the header tool for nothing. A registry that component classes add themselves to is one more thing for every new class to remember, which reflection exists to avoid.

**Testing.** That `GetAllClasses` lists every class once and after its base class, classes of the test module included; that `Component.ListClasses` lists the engine's components and the tests', sorted, without the base class, the stand-in, an abstract class and a class that is not a component, and that every class it lists can be added; and the sentences with which `Component.Add` refuses the base class and the stand-in.

## D-058: The Details panel shows properties by their type

**Status:** Accepted.

**Context.** The specification's Details panel shows the selected entity's components and their `WPROPERTY()` fields in collapsible categories, with search, an Add Component button and a reset-to-default arrow beside each changed value, and says that a new component needs no hand-written editor code. Until now a property could be changed only by an agent.

**Decision.**

- **The panel shows the entity that was selected last** (D-047): its name, its transform under a heading of its own, and each component under a heading with the properties that are saved with the scene, which are the ones commands change (`FSavedProperties`). Properties without a category come first, the others under their categories, in the order of declaration. Names are shown as words: `bCastShadows` as "Cast Shadows", `WPointLightComponent` as "Point Light Component" (`FUiText`).
- **A widget is chosen by the type of the property and nothing else** (`FPropertyEditor`): a box to tick; a number to drag, or to type after a click with Ctrl held; three numbers for a vector; three angles for a rotation; a button in the property's colour that opens a picker; a list of the scene's entities, or of its components of the right class, for a reference; the path of an asset for a reference to one; and for a list a row per element, with a button that adds one and a cross that removes one. A transform is three rows. A type that has no widget is shown as its name, so that it is not missing without a trace.
- **A widget does not change the value it shows.** It returns the value that the user changed its property to, as JSON, and the panel gives `Object.SetProperty` once everything is laid out; for a part of a value, an element of a list or a part of a transform, that is the whole value with the part replaced. The panel is therefore a user of the commands like an agent, and what it does is undone like anything else.
- **What a held widget changes is one step.** A dragged number gives a command in every frame. The first begins a step named for the property ("Set Intensity"), and the step ends in the first frame in which the widget is no longer active or no longer laid out, which also covers a widget that goes away under the mouse because something else changed the selection. A typed number is given when the user is done, by the library, and text likewise (below), so those are one command. Every other thing the user does in the editor ends such a step first (`FUiCommands`).
- **Numbers are shown with as few digits as they need,** up to three after the point, and two for angles: "0.5", not "0.500". A panel beside the viewport is narrow, and three numbers have to fit in a row. A dragged number is rounded to six digits, which keeps what a slow drag adds and removes what adding many steps leaves behind; a typed number is taken as typed. What is no number, as "nan" is, is given to nothing.
- **Angles belong to the user while they are held** (D-056). The angles read back from a rotation are not always the ones it was made from, so a pitch dragged past straight up would jump back; the panel keeps the angles of the rotation that is being changed until the user lets go.
- **A colour is picked as it looks and stored as it is.** Properties hold amounts of light (`FLinearColor`). The button and the picker work on the sRGB encoding (`FSrgb`), in which equal steps look equal; below the picker are the amounts themselves, which can be typed beyond white for a colour that glows.
- **Text is given when the user is done.** A field keeps what is typed until Enter or until the user goes elsewhere, and Escape puts back what was there, so that half a name, or half the path of an asset, is never given to anything.
- **The arrow puts back what a new object has.** The values that the properties of a class have in a new object are found once per class, by creating one and writing its properties down (`FPropertyDefaults`); the entity's transform goes back to none. A value is compared with them as JSON, so the arrow is there exactly when a scene file would differ.
- **Add Component offers the classes that the command accepts** (D-057), with a field that narrows them down, and the menu of a component's heading removes it. Both are steps named for the class ("Add Point Light Component").
- **Search finds a property by its name, its category or its component.** What is found is shown opened.
- **The panel is laid out before the panels in which the user selects, and its widgets are in the scope of the entity.** A click on another entity ends what was being typed; laid out first, the panel gives it to the entity it was typed for, and in another entity's scope it could not be mistaken for that entity's.

**Alternatives considered.** A class per kind of property that draws itself would put the user interface into the engine's reflection, or need a registry that maps one onto the other; a function that asks for the type is all that a dozen types need. Changing values through the address that reflection gives would be less code than a command per change, and would make the panel a second way to change the scene, with its own undo. Giving every keystroke of a text to the scene, as one step, needs no buffer, but a path that is half typed is a path to nothing, and the log would say so for every letter. Fixed digits ("0.000") do not fit three to a row in a narrow panel, and a format of significant digits would round a location far from the origin to metres.

**Limits.** The panel shows one entity; several cannot be changed together yet. A reference to an asset is typed as a path until the content browser gives something to pick from and to drag. A list of entities or components to choose from has no search. Numbers have no limits and no units, because `WPROPERTY()` has no way to say them yet. A component cannot be copied, moved or put back to its defaults as a whole.

**Testing.** With the mouse and keyboard of `FEditorUiTest`, on a component with a property of every kind: what is shown with nothing, one and several entities selected; the name typed, left with Escape, and given to the entity it was typed for when another is selected; a number typed and dragged, each as one step that one undo takes back; the arrows of the transform's parts; angles typed, and dragged past straight up; the tick box and its arrow; whole numbers, numbers with a point and "nan"; text; vectors; a transform as a property; the colour's window, its numbers beyond white and below none, and its picker; references to an entity and to a component, with only components of the right class listed; an asset by its path, a path to nothing, and an emptied field; a list that grows, is typed into, shrinks and is put back; categories that close; search by property, category, part of the transform and component; adding a component through its list and its search field; removing one by either way to its menu; a component whose class is missing; a step that ends when its widget goes away; and an undo in the middle of a drag.

## D-059: The viewport's camera is steered as in Unreal, around a focus point

**Status:** Accepted.

**Context.** The specification asks for Unreal's viewport navigation: the right mouse button with W, A, S and D to fly, Q and E for down and up, the mouse wheel for the speed, Alt and the left button to orbit, Alt and the right button to dolly, the middle button to pan, and F to focus on the selection. Until now the camera could only be put somewhere by a command (D-048).

**Decision.**

- **The camera knows what it looks at.** `FViewportCamera::FocusDistance` is how far in front of the camera its focus point is. Orbiting goes around that point, a dolly moves towards it, and panning moves the scene with the mouse at its distance. Flying and panning take the point along; `LookAt` and `Frame` put it on what they are given. A new camera's focus point is about the origin.
- **Steering is functions of the camera:** `Turn`, `Fly`, `Orbit`, `Dolly`, `Pan` and `Frame`, in the editor's public interface, which keep the camera valid whatever they are given: the pitch stops at its limit, the yaw stays within half a turn either way, and a dolly stops short of the focus point. The panel turns pixels, keys and seconds into their arguments and nothing more.
- **The viewport panel says how the mouse steers.** The right button flies: the mouse turns the camera by a fifth of a degree for each pixel, W, A, S and D move it the way it looks and sideways, Q and E down and up along the world's up axis, at `FlySpeed`, which each notch of the wheel multiplies or divides by 1.25, between 0.1 and 1000 metres per second. Alt and the left button orbit, Alt and the right button dolly by half a hundredth of the focus distance for each pixel to the right or upwards, and the middle button pans so that the scene follows the mouse. The wheel by itself moves a tenth of the focus distance closer or away for each notch. The left button without Alt is left for selecting.
- **How the camera is steered is settled when the button goes down** on the picture and lasts until it comes up, wherever the mouse goes meanwhile. The keys fly only while the right button is held, so that they stay free for everything else, such as the gizmo's W, E and R.
- **The mouse is taken for that long.** `FWindow::CaptureMouse` hides the pointer and lets the mouse move without end. The user interface does not know about windows (D-049), so it only says that it wants the mouse, with `FEditorUi::WantsMouseCaptured`, and `FEditorWindow` takes it and gives it back.
- **F is the command `Viewport.Focus`:** the camera goes, without turning, to where it sees the selected entities whole, or the entities it is given. What is looked at is the meshes of the entities and of everything below them, by their bounds; entities that show nothing are looked at where they are, as a ball of half a metre, so that the camera does not end up inside a lamp. It is a command because an agent needs to look at things too; steering by mouse is not, because it is the user's eye moving and no agent has a use for it.
- **None of it is a step of the history,** as nothing about the camera is (D-048).

**Alternatives considered.** Orbiting around the selection, or around whatever is under the mouse, needs something selected or a way to find what is under the mouse, which the viewport does not have yet; a point the camera carries always exists. A camera that has a target and a distance instead of a place is the same information, and makes flying the awkward case. Keys that fly without a mouse button would take W, E and R from the gizmo. Wrapping the pointer around the edges of the window instead of taking the mouse keeps the pointer in sight, where it is of no use while the mouse turns the camera.

**Limits.** The speed of flying has no place in the user interface yet; the viewport's toolbar will show it. There is no acceleration and no slower or faster flight with a key held. Bounds are those of meshes: a light, or anything else that shows nothing, has none. Taking the mouse of a window on the desktop is tested only as far as the window in memory goes.

**Testing.** The camera's functions against the geometry they stand for, with their limits, and what they refuse. In the user interface, with a mouse and keys that exist only in the test: looking around; each of the six keys, for a number of frames at the camera's speed; keys and a held button that are not on the viewport; the wheel during a flight, up to both limits; orbiting, with the focus point where it was; the left button without Alt; the dolly; the pan, by the size of a pixel at the focus point; the wheel over the viewport and over another panel; wanting the mouse during each way of steering and not otherwise; F with something selected, with nothing, and typed into a field; and the entry of the Edit menu. The command with one entity, with entities given, with meshes below an entity that shows nothing, with nothing that has a mesh, and with every way of having nothing to look at. And the window in memory taking and giving back the mouse.

## D-060: The engine has a cube, a sphere and a plane built in

**Status:** Accepted.

**Context.** A scene keeps only what has an asset path (D-043), and asset paths named files of the content directory. So nothing could be put into a scene and saved without a model file to import first, and an editor without a project could show nothing at all. Every editor has a few shapes to block a scene out with.

**Decision.**

- **Three meshes are assets of the engine itself:** `Engine/Shapes#Cube`, `Engine/Shapes#Sphere` and `Engine/Shapes#Plane` (`FAssetManager::CubeShape` and so on). They are made by `FMeshShapes` when first asked for, are no files, and need no content directory, so they also work in an editor that has no project and in a test that has no directory.
- **They are named like every other asset,** a file and a key, the file being `Engine/Shapes`. Nothing that refers to assets has to know that they are special: a component's property takes the path, a scene file writes it, `FAssetManager::Load` returns the mesh, and the package keeps the three together like the assets of a file. The name means the engine's whatever is in the content directory; a file `Engine/Shapes` there, which has no ending that a loadable file has, is not looked at.
- **Each is one metre across and centred on its origin,** the plane lying flat and facing up. The scale of the entity that shows one is then its size in metres, and one mesh serves for every box and ball of a scene.
- **They stay loaded when the content directory changes,** because nothing about them depends on it.
- **`Asset.List` and `Asset.Describe` cover them,** with or without a project, so that an agent finds them where it looks for assets.

**Alternatives considered.** A small file per shape in every project (`Shapes/Cube.shape`) keeps the rule that an asset is a file, at the price of a file format of our own and of files that every project has to have and can lose. Shapes as a kind of component that makes its own mesh would need no asset, and would not be a mesh that anything else can refer to. A separate way to name built-in assets, such as a prefix that no file name can have, would have to be taught to everything that reads an asset path.

**Limits.** There are three shapes; a cylinder, a cone and a capsule wait for `FMeshShapes` to make them. There is no built-in material: a mesh without one is drawn white.

**Testing.** That each shape loads without a content directory, knows its path, and has the bounds of one metre; that the three are one package, loaded once; the errors for a shape that does not exist, for the file without a key and for the wrong class; that the shapes survive a change of the content directory and that a file of the same name does not replace them; that a scene with a cube is saved with the path and loaded again without a content directory; and the two commands, without a project and with one.

## D-061: The toolbar's Add menu makes entities out of the commands that exist

**Status:** Accepted.

**Context.** The specification's main toolbar has Add, to create entities quickly. With the shapes built into the engine (D-060) there is something to add that needs no file. A scene could so far be filled only through the outliner's empty entities and the Details panel's Add Component, or by an agent.

**Decision.**

- **The toolbar is a bar across the main window, below the menu bar** (`FMainToolbar`). It has what works: Add. Save, the buttons that play the game and the one that exports it join it as those exist.
- **Add lists an empty entity, the three shapes and the three lights.** Each line is an entry of a table: the name of the entity, the class of its component, the mesh a mesh component shows, and how the entity is turned.
- **What a line does is commands that exist:** `Entity.Create`, `Object.SetProperty` for its transform, `Component.Add`, and `Object.SetProperty` for the mesh, as one step named "Add Cube" (`FUiCommands::AddEntity`), after which the entity is selected. There is no command that adds a cube: an agent gives the same four, and a command for every line of a menu would be a second list to keep.
- **What a line makes is written in the code, not by the user.** If the editor refuses one of its commands, the table is wrong, which is a mistake in the code and a fatal error, not something to clean up after: every line is added in a test.
- **The new entity goes where the viewport's camera looks,** at its focus point (D-059), rounded to the centimetre. There it is in sight wherever the camera has been flown, and its place reads as a number that someone might have typed.
- **A sun that is added shines down at a slant** (a pitch of -50 degrees and a yaw of -30), so that what it lights has a bright side, a dim side and a shadow. Everything else is added unturned.
- **The default layout is built for the room that the bars leave,** and for no less than a pixel each way: a window too small to have room below its bars would otherwise ask the library for a layout of no size, which it refuses.

**Alternatives considered.** Placing new things at the origin is predictable and, after the first few, out of sight. Placing them on whatever is under the middle of the picture needs a way to find what that is, which the viewport does not have yet. A separate command per kind of thing was weighed above.

**Limits.** A shape is put with its middle at the focus point, not standing on what is there. The sky light is added without an environment, which it needs to light anything. There are no icons yet: the buttons say what they do in words.

**Testing.** With the mouse of `FEditorUiTest`: the cube, with its name, its component and mesh, its place to the centimetre, the selection, the name of the step and one undo; every other line of the list; the sun's direction; that a cube that was added is in the picture of the viewport, in the middle of it; several things added one after another; and the user interface on a surface too small for anything, which must still cover it.

## D-062: Scene files are called .wwscene, and are saved and opened in the editor's window

**Status:** Accepted.

**Context.** A scene could be saved and opened only by a command, to and from a file of any name (D-024, D-045). The editor's window had no way to save what was made in it, and nothing could say which files of a project are scenes.

**Decision.**

- **A scene file ends in `.wwscene`** (`FSceneSerializer::FileExtension`), as a project file ends in `.wwproject`. It is the same JSON text as before. The ending is how scenes are told from the other files of a content directory; `Scene.Save` and `Scene.Open` still take a file of any name.
- **`Scene.List` returns the scene files of the project,** as paths that `Scene.Open` takes, so that an agent and the Open Scene window find them in the same way.
- **Ctrl+S, the File menu and the Save button of the toolbar save the scene** to its file. A scene that has none yet, or that cannot be saved to the one it has, has the user asked where: a window with a field for the file, which with a project open is a place in its content (`FEditor::MakeProjectPath` is how a file is shown there). A name without the ending is given it. Saving over another file is said in the window before it is done.
- **Ctrl+O and the File menu open a scene:** a window with the project's scenes to choose from and a field for the file. A double click opens.
- **A window that asks stays open until what was asked for has worked.** If the command fails, the window shows the command's sentence in the colour for errors and keeps what was typed. A scene that could not be saved does not look saved.
- **The windows are laid out with Dear ImGui like everything else,** not the operating system's own file windows: they work in a window in memory, so they are tested like the rest, and they show a project's content as the paths that commands take.
- **What the operating system says about an error is worded for a sentence** (`FFileHelper::DescribeError`): some systems end their messages with a full stop, which made the editor's sentences end in two.

**Alternatives considered.** The operating system's file windows are what users know, and can go anywhere on the disk; they cannot be opened or answered in a test, and a scene belongs in the content of its project, where a field and a list are enough. Recognising scene files by reading every `.json` file of a project is slow and finds what merely looks like one. Leaving the ending to the user makes files that `Scene.List` cannot find.

**Limits.** Opening a scene, making a new one or closing the editor does not ask about changes that were not saved. The Open window has no folders to go into and no search: it lists every scene of the project. Outside a project a whole path has to be typed. There is no list of the scenes that were opened last.

**Testing.** `Scene.List` with scenes in folders, with a scene of another ending and without a project; `MakeProjectPath` inside the content, beside it, outside it and without a project, and back through `ResolveFilePath`; the wording of operating system errors. In the user interface, with a project in a temporary directory: Ctrl+S on a scene without a file, with one, and when the file cannot be written; Save As to a folder that does not exist yet; Cancel and Escape; a whole path without a project; the toolbar's button and the entries of the File menu; that keys do nothing to the scene while a window asks; Ctrl+O with a click and Open, with a double click, with a typed file, with a file that is not there, cancelled, without a project, and after another scene was saved.

## D-063: The editor remembers the projects that were opened last, in a file of the user's own

**Status:** Accepted.

**Context.** Nothing the editor knew outlived the editor: every run began without a project, and the user or the agent had to say again where the project is. What an editor remembers for a person, whichever project is being worked on, needs a place that is neither the project nor the engine's build directory.

**Decision.**

- **`FEditorSettings` is what the editor remembers from one run to the next:** for now the project files that were opened last, the most recent first and ten at most, each by its whole path. It is JSON text with a format name and a version, like a project file and a scene file.
- **Whichever way a project is opened, it becomes the first of them.** `Project.New`, `Project.Open` and anything else go through `FEditor::SetProject`, which is where it is done. A project is in the list once, however its path is written: paths are compared as they lead straight to the file, and two files that both exist as the file system knows them, which sees through capitals on Windows and through links.
- **`FEditor::FindRecentProjects` is the recent projects that can be opened now,** found by opening their files, which also gives each its name. A project that was moved or deleted is not offered, and is not forgotten either: a project on a disk that is not connected is back when the disk is.
- **An editor keeps its settings in a file only if it is given one** (`FEditor::SetSettingsFile`). Without one it remembers for as long as it exists. The editor in its window on the desktop is given the file of the person who is logged in; a test, an agent's session (`--mcp`) and a screenshot are given none, so they neither depend on who runs them nor leave anything behind.
- **The user's file is `WhiteWolfEngine/Editor.json` in the directory that the system has for a program's settings** (`FEditorSettings::GetUserFile`). `FUserDirectories::GetSettingsDirectory`, in the engine, says where that is: the local application data on Windows, which the shell is asked for, and `$XDG_CONFIG_HOME` or `~/.config` elsewhere.
- **The file is read again before the settings change, and then written.** Two editors that run at the same time each add their project to what the other has written, instead of the one that writes last undoing the other. The file is written as every text file is, to a temporary file that then replaces it.
- **A file that cannot be read or written is a warning in the log, not an error.** The editor goes on with what it remembers, and a file that could not be read is replaced the next time a project is opened.

**Alternatives considered.** The roaming application data follows a person from one machine to another, where the paths of the projects mean nothing; the local one stays with the disk the projects are on. The registry of Windows is not a file that a person can read, copy or delete, and would need other code on every other system. A file beside the executable is the same for everyone who uses the machine, may not be writable, and is gone when the build directory is. Giving every editor the user's file would let a project that an agent made show up in the editor's window; it would also make every test run and every agent's session write to the settings of whoever starts it, and behave differently on each machine. An option that names the file can do the first without the second, when it is wanted. Keeping each project's name in the settings saves reading the project files, and shows a name that is out of date as soon as a project file is changed. Dropping a project from the list as soon as it cannot be opened would forget every project on a disk that is not connected.

**Limits.** Only the editor's window remembers: an agent cannot ask for the projects that were opened last, and what it opens is not added to them. There is nothing but the projects in the settings yet: not the layout of the panels, not the speed of the camera, not the scene that was worked on last. An entry is removed only by ten newer ones, or by changing the file by hand. A settings file of a later version cannot be read by this build, which says so in the log and then replaces it.

**Testing.** `FUserDirectories`: a whole path or none, the same every time, and on Windows that both directories are there. `FEditorSettings`: the order of projects that are added; a project added again by the same path, by one that goes somewhere else first, and by another path to the same file; projects whose files are gone; a relative path; more projects than are remembered; a project without a file as a fatal error; saving and reading back, with a space and a letter outside ASCII in a path; what a saved file holds; a directory that is made and a file that is replaced; a file that cannot be written; every way a file can fail to be a settings file; a file that does not mention projects, and one with more of them than are read. `FEditor`: a project that is created and one that is opened, by its directory too; one that could not be opened; that an editor without a file leaves the user's own alone, looked at before and after; a file that is written when a project is opened, read by a later editor, and added to by another editor in between; a file that cannot be read, and one that cannot be written; an editor whose file is taken away again; `FindRecentProjects` with a project that was moved away and came back, and with a file that is no longer a project file.

## D-064: The editor asks for a project when it starts, and makes and opens projects in its window

**Status:** Accepted.

**Context.** A project could be made and opened only by a command (D-045). The editor's window started without one and had no way to get one, so nothing could be imported, and a scene could be saved only by typing the whole path of a file.

**Decision.**

- **File > Open Project asks which project to work on:** a window with the projects that were opened last (D-063), each with its name and below it the directory it is in, a field for the project file or the directory of any other, and Open, New Project and Cancel. A click chooses a project and a double click opens it.
- **File > New Project asks for a project to make:** its name, and the directory to make the project's own directory in, which is called like the project. The window says where that puts the project before it is made. The place it suggests is beside the project that was opened last, so that a person's projects end up side by side, and for a first project `White Wolf Projects` in the person's documents (`FProject::GetDefaultLocation`, from `FUserDirectories::GetDocumentsDirectory`).
- **The editor asks for a project when it starts** (`FEditorUi::AskForProject`, which opens the first of the two windows). A game is made in a project, and most runs go on with the one of last time, which is then a double click away. The user may cancel and work without one. `--screenshot` does not ask: its picture is of the editor before a project is opened, the same on every machine.
- **Both windows give the commands that exist,** `Project.Open` and `Project.New`, and behave like the windows for scenes (D-062): they close when the command has worked, and otherwise stay open and show its sentence.
- **A path that does not fit is shown from its end,** after three dots (`FUiText::FitEnd`): the end says which directory it is, and the beginning only where the disk begins. The fields of all such windows are wider than they were, for the whole paths that these two show.

**Alternatives considered.** Opening the project of last time without asking saves a double click when it is the right one; when it is not, or cannot be opened any more, the user has to find the way out of it first, and a person with two projects is asked nothing at the moment they would choose. Unreal, Unity and Godot all ask. A program of its own that lists the projects and then starts the editor is more to build and to keep working than a window in the editor. The operating system's window for choosing a directory cannot be opened or answered in a test (D-062).

**Limits.** The directory for a new project has to be typed, or left as it is suggested: there is nothing to browse the disk with. A project starts empty; there are no templates. Opening or making a project does not ask about changes to the scene that were not saved, and begins with an empty scene, not with the one that was worked on last. A project cannot be named on the command line. That the window asks when the editor starts on the desktop is not covered by the tests, which cannot open a window there; what it calls is.

**Testing.** In the user interface, with projects in a temporary directory: Open Project with a click and Open, with a double click, with the file and with the directory typed, with a project that is not there, cancelled with the button and with the key, with a project that was moved away and came back, and before any project was opened; New Project from it; `AskForProject`, and that it does nothing while another window has to be answered. New Project with a name and a place, with the place it suggests, with Enter in either field, with a name that cannot be a file's, with a place that holds a project already, without a name, without a place, cancelled, and for a first project. `FProject::GetDefaultLocation`. `FUiText::FitEnd` with a text that fits, the longest end that fits, no room at all, a character of several bytes, and a measure under which the dots are wider than letters.

## D-065: A mirrored mesh is drawn by pipelines of its own

**Status:** Accepted.

**Context.** An entity whose scale is below zero along one axis, or along all three, shows the mirror image of its mesh: a left glove made of a right one. The renderer drew such a mesh inside out. A mirror turns the order of a triangle's corners around, as the viewer sees them, and the one pipeline there was took counter-clockwise for the front and left the rest out: of a mirrored mesh, what it left out was the outside.

**Decision.**

- **Each way of drawing meshes has two pipelines** (`FMeshPipelines`): the lit one, the unlit one and the pass ahead of ambient occlusion. One takes corners that run counter-clockwise for the front, for meshes as they are made, and the other corners that run clockwise, for mirrored ones.
- **The renderer chooses for each mesh by the scale that the entity has in the world:** below zero along one axis or along all three is a mirror; below zero along two is a half turn, and no mirror. The scales of the entities above count, as they do for everything else: a mirror above a mirror makes a mesh as it was.
- **Nothing else had to change.** Normals are scaled by the inverse of the scale, sign and all, so a mirrored surface was lit from the right side already. Shadow maps are drawn without leaving a side out.

**Alternatives considered.** Drawing both sides of a mirrored mesh shows its outside, and also its inside wherever the outside does not hide it, lit as if it faced the other way. A second list of indices for each mirrored mesh, with the corners the other way round, is more memory on the graphics card and more to keep in step with the mesh. Vulkan can change which order is the front from one draw to the next without another pipeline; NVRHI does not offer that.

**Limits.** A scale of zero along an axis flattens a mesh into a sheet that is seen from one side, as before.

**Testing.** A square two metres ahead, with every combination of signs that matters, seen from the front and from behind in the unlit view mode: mirrored along the axis it faces along or along all three, it faces away, and otherwise it faces the camera as before; a square below a mirrored entity, and mirrored once more itself. In the lit view mode, a mirrored surface is as bright as the surface it mirrors, and is not seen where it faces away. A corner of mirrored surfaces is as dark in its crease as any other. Each of these fails when mirrored meshes are given the pipeline of the others.

## D-066: What a ray meets in a scene is worked out on the processor, in the space of each mesh

**Status:** Accepted.

**Context.** The editor has to know what the user clicks on in the viewport. Dragging a gizmo, and putting a thing down on a surface, will need to know where in the world the mouse points as well.

**Decision.**

- **`FRay` is a ray in the world:** where it begins, in double precision, and a direction of unit length, so that a distance along it is in metres. `FCameraView::GetRay` is the ray along which a camera sees a point of its picture. The point is given as the projection counts, from -1 to 1 across and up, which leaves pixels to whoever has a picture; the ray begins on the near plane, because nothing closer is drawn.
- **`FSceneRayCast::FindFirstHit` returns what a ray meets first:** the static mesh component, and the distance. It goes through the visible static mesh components of the scene, which are what is drawn.
- **The ray is taken to the mesh, not the mesh to the ray.** Where the ray begins is measured from the entity in double precision, as everything is that goes from the world to something smaller (D-028), and what is left is tried in single precision against the triangles of the mesh, with the method of Moeller and Trumbore, after the box of the mesh has said that the ray comes near it at all (`FIntersection`). The direction is turned and scaled like any other vector and not made of unit length again; a multiple of it in the space of the mesh is then that many metres in the world, however the entity is scaled.
- **Only the fronts of triangles are met,** as only they are drawn. For a mirrored mesh that is the side which is its front in the mirror, the side that is drawn (D-065).
- **`FTransform` can take a vector back to local space** (`InverseTransformVector`) and says whether it can be undone at all (`IsInvertible`), which a transform with a scale of zero cannot.

**Alternatives considered.** Drawing the identifier of each entity into a buffer on the graphics card, and reading back the pixel under the mouse, finds exactly what is drawn, whatever a shader does to it. It costs a pass over the scene and a wait for the graphics card, and gives no distance and no place in the world, which a gizmo and snapping need. A tree of boxes over the triangles of each mesh makes a ray much faster; a click is one ray, and a million triangles are gone through in a few thousandths of a second. The physics engine will cast rays against the shapes that things collide with (milestone 6), which are not the shapes that are seen, and which a thing has only once somebody has given it one.

**Limits.** Lights, and entities that show no mesh, are met by nothing. Every triangle of a mesh whose box the ray meets is looked at: this is not for many rays in every frame. A mesh that is scaled to nothing along an axis is not met. The renderer has no materials yet that are see-through or cut out, so nothing has to be seen through.

**Testing.** `FRay`: a point along it, also far from the origin of the world. `FIntersection`: a triangle met squarely and at a slant, with directions of other lengths, outside each edge, on its edges and corners, from behind, along its plane, behind where the ray begins and on it, without an area and with a corner that is no number; a box met, passed, pointed away from, begun inside, touched, without thickness and empty. `FTransform`: a difference of positions taken back, a scale of zero, what can be undone. `WStaticMesh`: the distance in multiples of the direction, a miss and a hit from behind, the nearest of several sheets whatever their order, the inside of a closed box, no geometry. `WStaticMeshComponent`: where the entity puts the mesh, scaled, turned, below a parent, a million kilometres out, mirrored, and when it shows nothing. `FSceneRayCast`: an empty scene, the nearest of several meshes whichever was made first, a mesh behind the ray, one that is not visible, one below a parent, two on one entity. `FCameraView`: the ray through the middle and through the edges, that what lies along a ray is drawn at its point of the picture, with the projection, far from the origin, and an aspect ratio that is none.

## D-067: A click in the viewport selects what is seen under the mouse

**Status:** Accepted.

**Context.** Entities could be selected only by their rows in the outliner. To work on a thing that is seen in the viewport, the user had to know what it is called.

**Decision.**

- **A click with the left mouse button on the picture selects the entity whose mesh is seen there:** the one that the ray through that point meets first (D-066). A click on nothing selects nothing. With Ctrl held, a click adds the entity to what is selected or takes it out, and a click on nothing changes nothing: the same as in the outliner, and through the same function (`FUiCommands::ToggleSelection`).
- **A click is the button going down and coming up without the mouse having gone anywhere.** A press that drags is none, and neither is one that steers the camera: the right and the middle button, and the left one with Alt (D-059).
- **The entity that shows the mesh is the one that is selected,** not an entity above it.
- **It selects by the command there is,** `Selection.Set`, and is no step of the history, as selecting is none anywhere (D-047).

**Alternatives considered.** Selecting the topmost entity above the mesh first, and what is below it with further clicks, is what suits a model that was imported as a tree of entities; it needs to know which trees belong together, which prefabs will say (milestone 7). A command that returns what is seen at a point of the viewport would let an agent click; an agent has the identifiers of the entities already, and what the viewport's size is belongs to the window.

**Limits.** Lights and entities without a mesh cannot be clicked on: nothing of them is drawn yet. There is no rectangle to drag around several things. What is selected is not marked in the picture yet; the outline around it is to come.

**Testing.** In the user interface, with boxes where the viewport's camera looks: a click on a box and beside it, with the history and the camera as they were; two boxes behind one another, and the nearer one made invisible; a box below another entity; Ctrl with a click on a second box, on the first again and on nothing; presses that drag from nothing onto a box, from a box onto nothing and from one box onto another; the right button, the middle button and the left one with Alt held.
