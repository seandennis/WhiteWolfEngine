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

**Decision.** The engine ships for Windows x64 built with MSVC from Visual Studio 2022, as the specification requires, or from Visual Studio 2026 (D-019). The code is also kept compiling and passing its tests with GCC and Clang on Linux through the `linux-gcc`, `linux-clang` and `linux-sanitize` presets. Linux is not a supported platform for users of the engine.

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

**Decision.** `.github/workflows/ci.yml` runs on every push to `main` and every pull request: MSVC Debug and Release on `windows-2022` (the hosted image that carries Visual Studio 2022) and on `windows-2025` (which carries Visual Studio 2026; see D-019), the four Linux cross-check presets on `ubuntu-24.04`, and a formatting job. Actions are pinned to commit SHAs.

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

**Not yet decided.** The world's handedness, up axis and unit of length are fixed with the renderer and glTF import in milestone 3; nothing in the transform maths depends on them.

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

- **One description per value.** An `FProperty` describes a member variable, an array element, or a parameter or return value of a function. It knows the value's C++ type name, its flags and category, and how to convert a value to and from JSON. Plain value types (`bool`, the fixed-width integers, `float`, `double`, `std::string`, `FVector`, `FVector3f`, `FQuat`, `FTransform`) share one class, `TValueProperty<T>`, driven by a `TPropertyValueTraits<T>` specialization per type; adding a value type means adding one specialization.
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
