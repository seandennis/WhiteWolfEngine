# White Wolf Engine

A simple, production-grade 3D game engine for Windows, written in C++20.

"Simple" means a small, focused feature set done properly. The full specification is in [`WhiteWolfEngine.md`](WhiteWolfEngine.md).

## Status

The engine is built in the milestones defined by the specification. Each one must build, pass every test and be committed before the next starts.

| # | Milestone | Status |
| --- | --- | --- |
| 1 | Repository, CMake build, test framework, `AGENTS.md`, code style tooling, first push to GitHub | Complete |
| 2 | Core: logging, math, window, input, reflection, `WObject` garbage collection, ECS, scene serialization | Complete |
| 3 | Renderer: Vulkan via NVRHI, camera-relative rendering, reversed-Z, glTF, PBR, HDR, screenshot tests | Complete |
| 4 | Renderer: image-based lighting, soft shadows, SSAO | Complete |
| 5 | Editor: viewport, hierarchy, property panels, gizmo, command system, agent interface | In progress. Done: the `Editor` module with the command system (named commands with typed arguments and structured results) and the commands for scenes, entities, components and reflected properties; undo and redo of every change to the scene; the agent interface, a Model Context Protocol server in the editor's executable; assets that scenes refer to by path, so that a scene with imported models can be saved; a game project with a content directory, and commands that import assets into it and place models in the scene; the selection of entities, and the camera and view mode of the viewport; the editor's window, with a menu bar, the viewport and the outliner in a layout of dockable panels; in the outliner, entities are selected, created, renamed, copied, deleted and dragged onto one another; in the Details panel, the name, the transform and the components of the selected entity are changed, with a widget for every type of property, and components are added and removed; the viewport's camera is steered with the mouse and the keyboard as in Unreal, and a click in the viewport selects what is seen there, which then has an orange line around it and a gizmo on it, whose handles are dragged to move, turn and size it; a cube, a sphere and a plane are built into the engine, and the toolbar adds them and the lights to the scene; scenes are saved and opened from the File menu; projects are made and opened in the editor's window, which asks for one when it starts and remembers the ones that were opened last. Still to do: the buttons and shortcuts that set what the gizmo does, the other panels (content browser, output log), the status bar, and the commands for play mode, screenshots and the log |
| 6 | Plugin system, ChronoPhysics plugin, audio components | Not started |
| 7 | C++ scripting module with hot reload, prefabs, full test scene | Not started |
| 8 | Game export, and the Tetris acceptance test through the agent interface | Not started |
| 9 | Blueprint visual scripting, and Tetris rebuilt with Blueprints only | Not started |
| 10 | ChronoVehicle plugin | Not started |

What exists today: the build system, the `Engine` module (`WhiteWolfEngine.dll`) with versioning (`FVersion`, `FEngineVersion`), logging (`WW_LOG`, categories, console, file and in-memory sinks) assertions (`WW_CHECK`), math types (`FVector`, `FQuat`, `FTransform`) the object system (`WObject`, `WClass`, `NewObject`, `TObjectPtr`, `TWeakObjectPtr`, garbage collection), JSON reading and writing (`FJsonValue`) and reflection (`WCLASS()`, `WPROPERTY()`, `WFUNCTION()`, with `WhiteWolfHeaderTool` generating the registration code), scenes (`WScene`, `WEntity`, `WComponent`) that are saved and loaded as JSON text (`FSceneSerializer`), windows and input (`FPlatform`, `FWindow`, `FInputState`, with a headless mode for unattended runs), the renderer (`FRenderDevice`, `FSceneRenderer`: Vulkan through NVRHI, camera-relative rendering with reversed depth, glTF import, assets that scenes refer to by path (`FAssetManager`), physically based materials and lights in high dynamic range, image-based lighting from HDRIs, soft cascaded shadows and screen-space ambient occlusion, drawn without a window and compared with a reference image, and shown in a window through a swap chain, `FSwapChain`), the `Editor` module (`FEditor`, `FCommandRegistry`: the editor's commands for scenes, entities, components and properties, which tests and agents give without a window; `FEditorUi`: the editor's user interface, which gives the same commands), the unit test harness, formatting tooling and continuous integration.

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

## Starting the editor

After a build, the editor is `Build\windows\Binaries\Release\WhiteWolfEditor.exe` (or `Debug`):

```
Build\windows\Binaries\Release\WhiteWolfEditor.exe
```

It opens its window and asks which project to work on: a game is made in a project, a directory with the game's assets and scenes. Choose one of the projects that were opened last, type where another is, or make one with New Project, which asks for a name and for the directory to put it in. Both windows are in the File menu too.

The window shows the scene being edited in the viewport and its entities in the outliner. Add, in the toolbar, puts a cube, a sphere, a plane or a light where the viewport's camera looks. Ctrl+S saves the scene, asking for a file the first time, and Ctrl+O opens one of the project's scenes; both are in the File menu. A right click in the outliner creates an entity; a click on its row, or on the thing itself in the viewport, selects one, and with Ctrl held adds it to what is selected or takes it out; what is selected has an orange line around it in the viewport. F2 renames it, Ctrl+D copies it, Delete deletes it, and dragging its row onto another makes it that entity's child. The Details panel below the outliner shows the entity that is selected: its name, where it is, and its components with everything they hold. A number is dragged, or typed after a click with Ctrl held; the arrow beside a value puts it back as it is in a new component; Add Component lists the components there are, and the button at the end of a component's heading removes it. The viewport is steered as in Unreal: hold the right mouse button to look around and fly with W, A, S, D, Q and E, at a speed that the mouse wheel changes; hold Alt and drag with the left button to go around what the camera looks at, or with the right button to move closer or away; drag with the middle button to move sideways and up; turn the wheel to move closer or away; and press F to bring what is selected into view. Ctrl+Z and Ctrl+Y undo and redo, and Window > Load Default Layout puts the panels back where they were. The editor is being built (see the status above): what it cannot do with the mouse yet, an agent can do through its commands.

`WhiteWolfEditor --screenshot Editor.png` draws the same window without opening one and saves the picture, which needs no display. The picture is of the editor before a project is opened.

## Giving the editor commands from an AI agent

Everything the editor can do is a command, and `WhiteWolfEditor --mcp` offers every command as a tool to an agent: it is a [Model Context Protocol](https://modelcontextprotocol.io) server on its standard input and output. An agent's program starts it and needs no window. For a program that reads its servers from a `.mcp.json` file, such as Claude Code:

```json
{
  "mcpServers": {
    "white-wolf-editor": {
      "command": "Build/windows/Binaries/Release/WhiteWolfEditor.exe",
      "args": ["--mcp"]
    }
  }
}
```

The tool `Editor.ListCommands` describes all the others. A session usually begins with `Project.New` or `Project.Open`; `Asset.Import` then brings models, pictures and environments into the project and `Asset.Place` puts a model into the scene. `Scene.Save` writes the scene to a file of the project, such as `Scenes/Level1.wwscene`, and `Scene.List` and `Scene.Open` find it again. Entities and components are referred to by the identifiers that `Scene.GetHierarchy` lists, and `Edit.Undo` undoes any change to the scene.

## Repository layout

| Path | Contents |
| --- | --- |
| `Engine/` | The core engine module, `WhiteWolfEngine.dll`: `Public/` headers, `Private/` sources, `Tests/` unit tests |
| `Editor/` | The editor module, `WhiteWolfEditorCore.dll`: the editor's commands, which need no window, and its user interface |
| `CMake/` | Build helpers: compiler settings, module declaration, pinned dependencies |
| `Tools/` | Development tools: the reflection header tool, the shader compiler, source formatting, git hooks |
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
| [stb](https://github.com/nothings/stb) | master, 2026-11-21 | Public domain or MIT | Reading PNG, JPEG and Radiance HDR images, writing PNG and Radiance HDR images |
| [cgltf](https://github.com/jkuhlmann/cgltf) | 1.15 | MIT | Reading glTF 2.0 files |
| [Dear ImGui](https://github.com/ocornut/imgui) | 1.93.0, docking branch | MIT | Laying out the editor's user interface (not part of exported games) |
| [glslang](https://github.com/KhronosGroup/glslang) | 16.6.0 | BSD-3-Clause, Apache-2.0, MIT | Compiles shaders to SPIR-V during the build (not shipped) |
| [doctest](https://github.com/doctest/doctest) | 2.5.3 | MIT | Unit tests (not shipped) |

Dependencies are downloaded at configure time, pinned to exact commits. See [`docs/DECISIONS.md`](docs/DECISIONS.md) for why each was chosen.

## Contributing

Read [`AGENTS.md`](AGENTS.md) first. It applies to people as well as agents: follow the code style, add tests with every feature, and run the pre-commit review checklist before each commit.
