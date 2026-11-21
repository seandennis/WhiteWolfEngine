# White Wolf Engine

A simple, production-grade 3D game engine for Windows, written in C++20.

"Simple" means a small, focused feature set done properly. The full specification is in [`WhiteWolfEngine.md`](WhiteWolfEngine.md).

## Status

The engine is built in the milestones defined by the specification. Each one must build, pass every test and be committed before the next starts.

| # | Milestone | Status |
| --- | --- | --- |
| 1 | Repository, CMake build, test framework, `AGENTS.md`, code style tooling, first push to GitHub | Complete |
| 2 | Core: logging, math, window, input, reflection, `WObject` garbage collection, ECS, scene serialization | Complete |
| 3 | Renderer: Vulkan via NVRHI, camera-relative rendering, reversed-Z, glTF, PBR, HDR, screenshot tests | In progress. Done: a headless Vulkan render device through NVRHI; render targets that can be read back as images; shaders compiled and embedded at build time; static meshes, materials and a scene renderer that draws camera-relative with reversed-Z and no far plane, tested to give the same picture at the origin and ten billion kilometres from it; directional and point lights with physically based shading (glTF's metallic-roughness model), lit in HDR with exposure and tone mapping. Still to do: textures, glTF import, screenshot tests against reference images, presenting to a window |
| 4 | Renderer: image-based lighting, soft shadows, SSAO | Not started |
| 5 | Editor: viewport, hierarchy, property panels, gizmo, command system, agent interface | Not started |
| 6 | Plugin system, ChronoPhysics plugin, audio components | Not started |
| 7 | C++ scripting module with hot reload, prefabs, full test scene | Not started |
| 8 | Game export, and the Tetris acceptance test through the agent interface | Not started |
| 9 | Blueprint visual scripting, and Tetris rebuilt with Blueprints only | Not started |
| 10 | ChronoVehicle plugin | Not started |

What exists today: the build system, the `Engine` module (`WhiteWolfEngine.dll`) with versioning (`FVersion`, `FEngineVersion`), logging (`WW_LOG`, categories, console, file and in-memory sinks) assertions (`WW_CHECK`), math types (`FVector`, `FQuat`, `FTransform`) the object system (`WObject`, `WClass`, `NewObject`, `TObjectPtr`, `TWeakObjectPtr`, garbage collection), JSON reading and writing (`FJsonValue`) and reflection (`WCLASS()`, `WPROPERTY()`, `WFUNCTION()`, with `WhiteWolfHeaderTool` generating the registration code), scenes (`WScene`, `WEntity`, `WComponent`) that are saved and loaded as JSON text (`FSceneSerializer`), windows and input (`FPlatform`, `FWindow`, `FInputState`, with a headless mode for unattended runs), the unit test harness, formatting tooling and continuous integration.

## Requirements

- Windows 10 or 11, x64
- Visual Studio 2022 (version 17.6 or later) or Visual Studio 2026, with the **Desktop development with C++** workload (provides MSVC, the Windows SDK and CMake)
- A graphics card with a Vulkan 1.3 driver, to run the renderer and its tests (building needs neither a graphics card nor the Vulkan SDK)
- Git, on the `PATH`
- Internet access for the first configure, which downloads the pinned third-party sources

## Build and test

From a **Developer PowerShell for VS**, in the repository root:

```
cmake --workflow --preset windows-debug
```

That one command configures the project, builds it and runs every test. Use `windows-release` for an optimised build.

To work in Visual Studio, open the solution in `Build/windows/` after the first configure: `WhiteWolfEngine.slnx` with Visual Studio 2026, `WhiteWolfEngine.sln` with Visual Studio 2022. Everything else (individual steps, running single tests, the Linux cross-check presets) is described in [`AGENTS.md`](AGENTS.md).

## Repository layout

| Path | Contents |
| --- | --- |
| `Engine/` | The core engine module, `WhiteWolfEngine.dll`: `Public/` headers, `Private/` sources, `Tests/` unit tests |
| `CMake/` | Build helpers: compiler settings, module declaration, pinned dependencies |
| `Tools/` | Development tools: the reflection header tool, source formatting, git hooks |
| `docs/DECISIONS.md` | Every design and tooling decision, with its reasons |
| `AGENTS.md` | Build and test commands, code style, directory layout and the pre-commit review checklist |
| `.claude/skills/` | Step-by-step procedures for agents working on the engine |
| `.github/workflows/ci.yml` | Continuous integration |

## Third-party software

| Library | Version | Licence | Used for |
| --- | --- | --- | --- |
| [glm](https://github.com/g-truc/glm) | 1.0.3 | MIT | Vector, quaternion and matrix math |
| [GLFW](https://github.com/glfw/glfw) | 3.4 | zlib | Windows, keyboard and mouse input |
| [NVRHI](https://github.com/NVIDIA-RTX/NVRHI) | main, 2026-11-21 | MIT | Rendering hardware interface, with its Vulkan backend |
| [Vulkan-Headers](https://github.com/KhronosGroup/Vulkan-Headers) | 1.4.352 | Apache-2.0 OR MIT | The declarations of the Vulkan API |
| [glslang](https://github.com/KhronosGroup/glslang) | 16.6.0 | BSD-3-Clause, Apache-2.0, MIT | Compiles shaders to SPIR-V during the build (not shipped) |
| [doctest](https://github.com/doctest/doctest) | 2.5.3 | MIT | Unit tests (not shipped) |

Dependencies are downloaded at configure time, pinned to exact commits. See [`docs/DECISIONS.md`](docs/DECISIONS.md) for why each was chosen.

## Contributing

Read [`AGENTS.md`](AGENTS.md) first. It applies to people as well as agents: follow the code style, add tests with every feature, and run the pre-commit review checklist before each commit.
