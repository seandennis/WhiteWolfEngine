# White Wolf Engine

A simple, production-grade 3D game engine for Windows, written in C++20.

"Simple" means a small, focused feature set done properly. The full specification is in [`WhiteWolfEngine.md`](WhiteWolfEngine.md).

## Status

The engine is built in the milestones defined by the specification. Each one must build, pass every test and be committed before the next starts.

| # | Milestone | Status |
| --- | --- | --- |
| 1 | Repository, CMake build, test framework, `AGENTS.md`, code style tooling, first push to GitHub | In progress: everything is in place and passes on the Linux cross-check presets. Still to do: the first MSVC build and test run, and the first push |
| 2 | Core: logging, math, window, input, reflection, `WObject` garbage collection, ECS, scene serialization | In progress. Done: logging, assertions, math types, `WObject`/`WClass` with incremental mark-and-sweep garbage collection, `Destroy`/`IsValid`, weak references, leak reporting. Still to do: reflection of value types and functions with the `WCLASS`/`WPROPERTY`/`WFUNCTION` header tool, ECS, scene serialization, window, input. Not yet built with MSVC |
| 3 | Renderer: Vulkan via NVRHI, camera-relative rendering, reversed-Z, glTF, PBR, HDR, screenshot tests | Not started |
| 4 | Renderer: image-based lighting, soft shadows, SSAO | Not started |
| 5 | Editor: viewport, hierarchy, property panels, gizmo, command system, agent interface | Not started |
| 6 | Plugin system, ChronoPhysics plugin, audio components | Not started |
| 7 | C++ scripting module with hot reload, prefabs, full test scene | Not started |
| 8 | Game export, and the Tetris acceptance test through the agent interface | Not started |
| 9 | Blueprint visual scripting, and Tetris rebuilt with Blueprints only | Not started |
| 10 | ChronoVehicle plugin | Not started |

What exists today: the build system, the `Engine` module (`WhiteWolfEngine.dll`) with versioning (`FVersion`, `FEngineVersion`), logging (`WW_LOG`, categories, console, file and in-memory sinks) assertions (`WW_CHECK`), math types (`FVector`, `FQuat`, `FTransform`) and the object system (`WObject`, `WClass`, `NewObject`, `TObjectPtr`, `TWeakObjectPtr`, garbage collection), the unit test harness, formatting tooling and continuous integration.

## Requirements

- Windows 10 or 11, x64
- Visual Studio 2022, version 17.6 or later, with the **Desktop development with C++** workload (provides MSVC, the Windows SDK and CMake)
- Git
- Internet access for the first configure, which downloads the pinned third-party sources

## Build and test

From a **Developer PowerShell for VS 2022**, in the repository root:

```
cmake --workflow --preset windows-debug
```

That one command configures the project, builds it and runs every test. Use `windows-release` for an optimised build.

To work in Visual Studio, open `Build/windows/WhiteWolfEngine.sln` after the first configure. Everything else (individual steps, running single tests, the Linux cross-check presets) is described in [`AGENTS.md`](AGENTS.md).

## Repository layout

| Path | Contents |
| --- | --- |
| `Engine/` | The core engine module, `WhiteWolfEngine.dll`: `Public/` headers, `Private/` sources, `Tests/` unit tests |
| `CMake/` | Build helpers: compiler settings, module declaration, pinned dependencies |
| `Tools/` | Development tools: source formatting, git hooks |
| `docs/DECISIONS.md` | Every design and tooling decision, with its reasons |
| `AGENTS.md` | Build and test commands, code style, directory layout and the pre-commit review checklist |
| `.claude/skills/` | Step-by-step procedures for agents working on the engine |
| `.github/workflows/ci.yml` | Continuous integration |

## Third-party software

| Library | Version | Licence | Used for |
| --- | --- | --- | --- |
| [glm](https://github.com/g-truc/glm) | 1.0.3 | MIT | Vector, quaternion and matrix math |
| [doctest](https://github.com/doctest/doctest) | 2.5.3 | MIT | Unit tests (not shipped) |

Dependencies are downloaded at configure time, pinned to exact commits. See [`docs/DECISIONS.md`](docs/DECISIONS.md) for why each was chosen.

## Contributing

Read [`AGENTS.md`](AGENTS.md) first. It applies to people as well as agents: follow the code style, add tests with every feature, and run the pre-commit review checklist before each commit.
