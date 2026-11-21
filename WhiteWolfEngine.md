# White Wolf Engine

We're building a production-grade, simple and straightforward 3D game engine for Windows. This is professional software that must be stable, fully tested, and real-world deployable. No hacks, no shortcuts - solid as a rock.

"Simple" means a small, focused feature set done properly. When simplicity and feature count conflict, cut features, never quality.

## Dev Workflow
- Work autonomously. Ask questions only when a decision is irreversible or blocks all progress; otherwise make the call and record it in `docs/DECISIONS.md`.
- IMPORTANT: Do NOT look outside the working directory or use other code on my computer as reference.
- Work in milestones (see Milestones below). Each milestone must build, pass all tests, and be committed before the next one starts.
- Do things properly. This is production-grade, not a hack project.
- Create `AGENTS.md` and relevant skills to support development. They must contain concrete guidelines: build commands, test commands, code style rules, directory layout, and the review checklist.

### Testing
- Testing is of the utmost importance. Every feature ships with unit tests (any framework, simple is good).
- All tests run from one command (`ctest`) and must pass before any commit.
- Maintain a test scene that exercises every engine feature: every component type and the entire scripting API. Adding a feature means adding it to the test scene.
- The engine must run headless (no window, no user input) so tests can run unattended.
- Renderer tests: render the test scene headless, save a screenshot, and compare against a stored reference image within a tolerance.
- Each plugin carries its own unit tests and its own test scene. The test suite must pass with all plugins enabled and with all plugins disabled.
- Treat warnings as errors (`/W4 /WX` on MSVC).

### Code Style
- Follow the Epic C++ Coding Standard: https://dev.epicgames.com/documentation/en-us/unreal-engine/epic-cplusplus-coding-standard-for-unreal-engine
  (The UnrealEngine GitHub repo is private, so use the public standard above as the reference.)
- Replace Unreal's `U` prefix with `W`: `WObject`, `WClass`, and the macros `WCLASS()`, `WPROPERTY()`, `WFUNCTION()`.
- Keep the other Unreal prefixes: `F` for structs and plain classes, `E` for enums, `I` for interfaces, `T` for templates, `b` for booleans.
- PascalCase for types, functions and variables. One class per header/source pair.
- Check a `.clang-format` file into the repo that enforces this style, and run it before every commit.

### Git
- Create a git repository and push to https://github.com/seandennis/WhiteWolfEngine
- Before every commit, do a code review of the diff and confirm that:
  1. It complies with the code style.
  2. It meets production-grade quality standards (no TODO hacks, no dead code, errors handled).
  3. New behaviour has unit tests, and the full test suite passes.
- Small, focused commits with descriptive messages. Never commit a broken build.

## Tech Stack
- C++20 and CMake for the core engine. Compiler: MSVC (Visual Studio 2022).
- [GLFW](https://github.com/glfw/glfw) for windowing and input.
- [NVRHI](https://github.com/NVIDIA-RTX/NVRHI) for rendering, with Vulkan as the backend.
- [glm](https://github.com/g-truc/glm) for math.
- [miniaudio](https://github.com/mackron/miniaudio) for audio.
- [Project Chrono](https://github.com/projectchrono/chrono) for physics, as optional plugins (brings Eigen as a dependency; only fetched when a Chrono plugin is enabled).
- C++ and Blueprint visual scripting for gameplay (see Behavior).
- Other libraries (glTF loading, image loading, editor UI, test framework) are your choice. Prefer small, widely used, permissively licensed libraries, and record each choice and its reason in `docs/DECISIONS.md`.
- Third-party code is pinned to exact versions (git submodules or CMake FetchContent with fixed tags). A fresh clone must build with one documented command.

## Basic Architecture
- Static library for the core engine; executables for the editor and the runtime.
- The editor is required to build games. It can be embedded in the runtime executable (and stripped from distribution builds) or be a standalone executable - your choice, recorded in `docs/DECISIONS.md`.
- A simple ECS for authoring scenes from entities and components.
- Scenes and prefabs are saved as human-readable text files (e.g. JSON) so they diff cleanly and agents can read them.
- `WPROPERTY()` reflection drives serialization and the editor's property panels, so a new component needs no hand-written editor or save/load code.
- "Export game": produces a folder with a runtime executable and packaged assets that runs the game with no editing ability, ready to distribute.

### Memory Management and Garbage Collection
Object lifetime is automatic, as in Unreal Engine. Game developers never write memory management or garbage collection code, and never call `delete` on engine objects.

- Every `WObject`-derived object (components, scripts, assets, prefab instances) is created through `NewObject<T>()` and owned by the garbage collector.
- The collector is mark-and-sweep. It starts from a root set (the loaded scene, the engine and editor singletons, and anything explicitly added with `AddToRoot()`) and follows `WPROPERTY()` references, including those inside reflected containers. Anything unreachable is destroyed.
- A `WObject` pointer that should keep its target alive must be a `WPROPERTY()`. Use `TWeakObjectPtr<T>` for references that should not keep the target alive.
- Destroying an entity or object marks it for destruction immediately: `IsValid()` returns false from that point, and remaining `WPROPERTY()` and weak references to it are nulled when it is collected, so scripts never hold dangling pointers.
- Collection runs automatically, on a timer and on scene change, and is incremental with a per-frame time budget so it does not cause frame hitches. `CollectGarbage()` exists for tests and tools but games never need to call it.
- Objects get a `BeginDestroy()` callback to release non-memory resources (GPU buffers, audio handles, physics bodies).
- Non-`WObject` types (`F` structs and plain classes) use RAII, value semantics and smart pointers. No raw `new`/`delete` outside low-level engine internals.
- Objects created by plugins and by the game script module use the same collector. On script hot reload and plugin unload, all objects of the affected classes are destroyed or re-instanced before the DLL is unloaded.
- Tests: unit tests for reachability, cycles (two objects referencing each other are still collected), weak pointers, nulling of references to destroyed objects, and hot reload. Debug builds report any leaked `WObject` or engine allocation at shutdown, and the test suite fails on any leak.

### Plugins
- Optional features live in plugins under `Plugins/<Name>/`, each with a descriptor file (name, version, dependencies) and its own CMake target, built as a DLL.
- A plugin can register components, systems, script API, asset importers and editor commands/panels. Everything it registers uses the same reflection as the core engine, so it appears in the editor and agent interface automatically.
- Two levels of enabling:
  1. Build: a CMake option per plugin (e.g. `WW_PLUGIN_CHRONO_PHYSICS`, `WW_PLUGIN_CHRONO_VEHICLE`). A disabled plugin and its third-party dependencies are not downloaded or compiled.
  2. Project: each game project file lists its enabled plugins. Enabling a plugin enables its dependencies; disabling one that others depend on is refused with a clear error.
- Opening a scene that uses components from a plugin that is not enabled must not crash or lose data: keep the unknown component data intact on save and report which plugin is missing.
- Exported games include only the plugins the project has enabled.
- Plugins can be listed, enabled and disabled through the agent interface.

### AI Control of the Editor
The editor must be fully controllable by AI agents. I should be able to ask for a game like Tetris and have you build it with no intervention from me.

- Every editor action is a named command with typed arguments. The UI and the agent interface both call the same commands.
- Commands are reachable without the GUI, through a command-line interface and/or a local server (e.g. an MCP server).
- Minimum command set: create/open/save scene; create/delete/rename/parent entities; add/remove components; get/set any reflected property; import assets; create and attach scripts; build scripts; create and edit Blueprints (add/remove nodes, connect/disconnect pins, set pin defaults, add variables and functions, compile, read the graph back as text); enter/exit play mode; step N frames; capture a screenshot; read the log; export the game.
- Commands return structured results (success or an error with a message) so an agent can verify its own work.
- Acceptance test: an agent builds, plays and exports a small complete game (e.g. Tetris) using only this interface.

## Editor
The editor's layout, workflow and controls follow Unreal Engine 5, so an Unreal user feels at home immediately. Match the design, not the assets: all icons, fonts and artwork must be original or permissively licensed, with no Epic or Unreal branding.

### Layout (default)
- Menu bar at the top: File, Edit, Window, Tools, Build, Help.
- Main toolbar below it: Save, Add (quick-create entities), Play / Pause / Stop / Step Frame, and Platforms (export game).
- Level Viewport in the centre, with its own overlay toolbar: gizmo mode, world/local space, snapping toggles and sizes, camera speed, and view mode (Lit, Unlit, Wireframe, plus buffer views such as SSAO and shadow cascades).
- Outliner at top right: the scene's entity tree with search, visibility toggles, drag to re-parent, and a right-click context menu.
- Details panel at bottom right: the selected entity's components and their `WPROPERTY()` fields in collapsible categories, with search, an Add Component button, and a reset-to-default arrow beside each changed value.
- Content Browser along the bottom: folder tree on the left, asset thumbnails on the right, search and filters, an Import button, drag and drop from Windows Explorer, and drag assets into the viewport to place them. Also available as a pop-up Content Drawer on Ctrl+Space.
- Output Log as a tab beside the Content Browser, with filtering by severity and category.
- Status bar at the bottom: background task progress and script compile status.

### Behaviour
- All panels are dockable tabs that can be dragged, split, floated and closed. The layout is saved per user, and Window > Load Default Layout restores it.
- Dark theme by default, in the style of UE5: dark grey panels, a blue accent for selection, and an orange outline on selected objects in the viewport.
- Viewport navigation matches Unreal: hold right mouse + WASD to fly (Q/E down/up, mouse wheel changes speed), Alt + left mouse to orbit, Alt + right mouse to dolly, middle mouse to pan, F to focus on the selection.
- Gizmo shortcuts match Unreal: W translate, E rotate, R scale, Space to cycle, with grid, rotation and scale snapping. Alt + drag duplicates.
- Standard shortcuts: Ctrl+S save, Ctrl+Z / Ctrl+Y undo and redo, Ctrl+D duplicate, Delete, Ctrl+C / Ctrl+V, F2 rename, Alt+P play, Esc stop.
- Every edit is undoable. Undo and redo go through the same command system the agent interface uses.
- Play In Editor runs the game inside the viewport. Stopping restores the scene exactly as it was before play.
- Double-clicking an asset opens its editor as a new tab in the main window. The Blueprint editor follows Unreal's layout: a My Blueprint panel on the left (variables, functions, events), the graph in the centre, Details on the right, a toolbar with Compile, Save and Play, and compiler results at the bottom. Right-click on the graph, or drag off a pin, opens a searchable, context-sensitive node menu.
- A project browser appears on launch to create or open a project.
- Terminology follows this spec, not Unreal, where they differ: "Entity" rather than "Actor", and "Scene" rather than "Level".

### Testing
- Every UI action calls a named command (see AI Control of the Editor), so editor behaviour is unit-tested through commands without driving the UI.
- Capture screenshots of the default layout and the Blueprint editor in the test run and compare them against reference images, to catch layout regressions.

## 3D Renderer
- Import glTF 2.0 meshes (.gltf and .glb) with materials and textures.
- Editor gizmo to translate, rotate and scale entities in the world.
- PBR material workflow (metallic-roughness, matching glTF).
- Image-based lighting from HDRIs. Source them from https://polyhaven.com/hdris (CC0 licensed).
- Soft shadow maps for directional lights (cascaded, with PCF or better filtering).
- Good-quality SSAO.
- HDR pipeline with tonemapping.

## Large Worlds
Worlds can be very large (e.g. a space game at solar-system scale) with no jitter or broken physics far from the origin. With 32-bit floats, precision degrades with distance from (0,0,0); the engine avoids this with large world coordinates, camera-relative rendering and origin rebasing. Developers do not handle any of this per game.

- Large world coordinates: all world-space positions (entity transforms, physics, scripting API, Blueprint vector pins for positions, saved scenes) are 64-bit doubles (`glm::dvec3`, exposed as `FVector`). Rotations, scales, directions and mesh-local data stay 32-bit float.
- Camera-relative rendering: the GPU never receives large world coordinates. Each frame, positions are made relative to the camera in double precision on the CPU, then converted to float. View matrices have no translation. Shadows, SSAO, IBL and the gizmo all work in this camera-relative space.
- Depth precision: use reversed-Z with a 32-bit float depth buffer and an infinite far plane, so near and very distant objects render together without z-fighting.
- Audio is computed relative to the listener.
- Origin rebasing: for worlds beyond what doubles handle well (galaxy scale), a scene has a world origin offset stored as 64-bit integer sector coordinates. `SetWorldOrigin()` shifts every entity, physics body and active effect in one consistent step, and an optional setting rebases automatically when the camera passes a distance threshold. Scripts can always query an entity's absolute position (sector plus local position).
- Tests: place the test scene at 0, at 1,000 km and at 10 billion km from the origin. Physics results must match within tolerance and screenshots must match the reference image at all three. Add a test that rebasing mid-simulation does not change the outcome.

## 3D Physics
Physics is provided by plugins (see Plugins), built on [Project Chrono](https://github.com/projectchrono/chrono). The core engine defines the physics interface and components; it contains no Chrono code and builds and runs with no physics plugin enabled.

### ChronoPhysics plugin (rigid body physics)
- Implements the engine's physics interface using the core Chrono module only.
- Uses a single `ChSystemNSC` (non-smooth contact) system per scene, with Chrono's built-in Bullet-based collision system.
- No Chrono or Eigen types appear in engine public headers or the scripting API; convert to and from glm at the boundary. Chrono is double precision throughout, so world positions pass through without precision loss (see Large Worlds). Origin rebasing shifts all Chrono bodies in the same step as the entities.

### ChronoVehicle plugin (vehicle physics)
- Optional, depends on ChronoPhysics, and builds the Chrono::Vehicle module.
- Adds its own components (e.g. wheeled vehicle, driver input) and script API for throttle, steering and braking, simulated in the same Chrono system as the rigid bodies.
- Vehicle definitions load from Chrono's JSON vehicle specification files, treated as engine assets.

Further Chrono modules (FSI, Sensor, etc.) follow the same pattern: one plugin per module, depending on ChronoPhysics, off by default. Do not build them until asked.

### Physics features (with ChronoPhysics enabled)
- Authored through components in the editor: rigid body (static, dynamic, kinematic) and colliders (box, sphere, capsule, mesh).
- Controllable from scripts: apply forces and impulses, set velocity, raycasts, and collision/trigger callbacks.
- Fixed timestep, so simulation is deterministic enough to test.

## Audio
- Audio source and listener components using miniaudio.
- Play, stop, loop and volume, with 3D positional audio, controllable from scripts.

## Behavior
Gameplay can be written in C++, in Blueprints (visual scripting), or a mix of both, as in Unreal Engine.

### C++ scripting
- Gameplay scripts are written in C++ and compiled into a game module (DLL) that the editor loads and can hot-reload without restarting.
- Scripts are components with lifecycle functions: `BeginPlay`, `Tick(DeltaTime)`, `EndPlay`.
- Scripts can read and modify any entity and component, read input, and create and destroy entities at runtime.
- Scripts can spawn new entities from prefabs.
- Exported games ship the compiled game module; no compiler is needed on the player's machine.

### Blueprint visual scripting
- A Blueprint is a node-graph asset that defines a script class, edited in a graph editor panel: nodes with execution pins (flow) and data pins (typed values), connected by wires.
- Nodes are generated from reflection, never hand-written per function. Specifiers on the macros control exposure, matching Unreal: `WFUNCTION(BlueprintCallable)`, `WFUNCTION(BlueprintPure)`, `WFUNCTION(BlueprintImplementableEvent)`, `WPROPERTY(BlueprintReadWrite)`, `WPROPERTY(BlueprintReadOnly)`. The engine's entire scripting API is exposed this way, so anything a C++ script can do, a Blueprint can do, and plugin APIs appear as nodes automatically.
- Built-in nodes: events (`BeginPlay`, `Tick`, `EndPlay`, input, collision/trigger), flow control (Branch, Sequence, For Loop, For Each, While, Delay), variables (get/set), math and comparison, type casts, spawn/destroy entity, and print/log.
- Blueprints can declare variables, functions with inputs and outputs, and custom events. Variables can be marked editable so they show in the property panel per entity.
- A Blueprint can derive from `WScript` directly or from a C++ script class, calling and overriding its exposed functions.
- Blueprints compile to bytecode run by a VM inside the engine. Compilation reports errors and warnings per node (unconnected required pins, type mismatches, unknown functions) as structured results. A Blueprint with errors does not run and never crashes the engine.
- Blueprints need no C++ compiler: they recompile and hot-reload in the editor instantly, and a Blueprint-only game exports without a C++ toolchain installed.
- Blueprint assets are saved as human-readable text with stable node and pin IDs, so they diff cleanly. If a C++ function is renamed or removed, affected nodes load as error nodes rather than disappearing.
- Blueprint objects are `WObject`s and use the same garbage collector.
- Debugging: breakpoints on nodes, step, inspect pin values while paused in play mode, and runaway-loop protection (an iteration limit that stops the script with an error instead of freezing the editor).
- Tests: unit tests for the compiler and VM (each built-in node, type checking, error cases), plus a Blueprint in the test scene that calls every Blueprint-exposed function, mirrored against the C++ script that does the same.

## Milestones
1. Repo, CMake build, test framework, `AGENTS.md`, code style tooling, first push to GitHub.
2. Core: logging, math, window, input, reflection (`WCLASS` / `WPROPERTY` / `WFUNCTION`), `WObject` garbage collection, ECS with double-precision world transforms, scene serialization.
3. Renderer: Vulkan via NVRHI, camera-relative rendering with reversed-Z, glTF import, PBR, HDR and tonemapping, headless screenshot tests.
4. Renderer: IBL, soft shadows, SSAO.
5. Editor: viewport, hierarchy, property panels, gizmo, command system and agent interface.
6. Plugin system, ChronoPhysics plugin, audio components.7. C++ scripting module with hot reload, prefabs, full test scene.
8. Game export, then the acceptance test: build and export Tetris through the agent interface only.
9. Blueprint visual scripting: compiler, VM, graph editor, agent commands. Acceptance test: rebuild Tetris using Blueprints only, with no C++ game code.
10. ChronoVehicle plugin.

## Definition of Done
- A fresh clone builds with one documented command on Windows.
- All unit tests and the test scene pass headless.
- The test scene covers every component and the entire scripting API.
- The Tetris acceptance test passes and the exported game runs on a machine without the engine installed.
- `AGENTS.md`, `README.md` and `docs/DECISIONS.md` are up to date.
