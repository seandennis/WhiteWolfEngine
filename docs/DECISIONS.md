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

**Status:** Accepted. Not yet exercised: the workflow first runs when the repository is pushed.

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
- **Registration is written by hand for now** (`WW_DECLARE_CLASS`, `WW_IMPLEMENT_CLASS`, `RegisterClassMembers`). The header tool that generates the same calls from `WCLASS()`, `WPROPERTY()` and `WFUNCTION()` comes later in milestone 2; it emits calls to this same registration API.
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

**Status:** Partly implemented.

**Decision.**

- `FGarbageCollector::ReportLeakedObjects()` runs a full collection and logs an error for every object that still exists and is not a class. The test executable calls it after the tests and fails the run if anything is reported, so a test that leaves an object rooted cannot pass.
- Engine memory allocations other than objects are checked by LeakSanitizer in the `linux-sanitize` preset, which fails the test run on any leak.

**Still to do.** The specification asks debug builds on the product platform to report leaked engine allocations at shutdown. On MSVC that needs the debug heap of the C runtime (`_CrtSetDbgFlag` and a comparison of heap snapshots around the test run), together with making failed assertions report to stderr instead of opening a dialog. Both are Windows-specific code that belongs with the platform layer and has to be written with an MSVC build to verify it. Until then, allocation leaks are caught on Linux only.

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
