---
name: add-third-party-dependency
description: Add or update a pinned third-party library in White Wolf Engine. Use when a feature needs a new library (glTF loading, image loading, UI, audio, physics, testing) or when changing the version of an existing one.
---

# Add or update a third-party dependency

## 1. Choose

- The required stack is fixed by `WhiteWolfEngine.md` (GLFW, NVRHI with Vulkan, glm, miniaudio, Project Chrono). Do not substitute these.
- For anything else, prefer a small, widely used, actively maintained library with a permissive licence (MIT, BSD, zlib, Apache-2.0, BSL-1.0, public domain). No GPL or LGPL code.
- Check that the feature cannot reasonably be done with the C++20 standard library or a dependency the project already has.

## 2. Pin

Find the commit that the release tag points to:

```
git ls-remote --tags https://github.com/<owner>/<repository>
```

Use the peeled commit (the line ending in `^{}`) when the tag is annotated.

Declare the dependency in the right CMake file:

| Used by | Declare it in |
| --- | --- |
| Tests only | `CMake/WhiteWolfThirdPartyTesting.cmake` |
| A plugin | That plugin's `CMakeLists.txt`, inside its `WW_PLUGIN_<NAME>` option, so a disabled plugin fetches nothing |
| The engine, editor or runtime | A `CMake/WhiteWolfThirdParty<Area>.cmake` file included from the top-level `CMakeLists.txt` |

Follow this pattern exactly:

```cmake
# <name>: <what it is used for> (<licence>). Release <tag>.
set(<NAME>_BUILD_TESTS OFF CACHE BOOL "" FORCE)      # turn off the library's tests, examples and install rules
FetchContent_Declare(<name>
    GIT_REPOSITORY https://github.com/<owner>/<repository>.git
    GIT_TAG <full 40-character commit SHA>
    SYSTEM)
FetchContent_MakeAvailable(<name>)
```

- Always pin a full commit SHA, never a branch or a bare tag name.
- Always pass `SYSTEM` so the library's headers cannot break warnings-as-errors.
- Never copy third-party source files into the repository by hand.

## 3. Contain

- Link it `PRIVATE` to the module that uses it unless its types are part of that module's public API.
- Keep third-party types out of engine public headers unless the specification says otherwise (glm is exposed; Chrono and Eigen never are).
- Do not apply `ww_apply_compiler_settings()` to third-party targets.

## 4. Record

Add an entry to `docs/DECISIONS.md` with the library, version, commit, licence, what it is used for, the alternatives considered and why this one was chosen. Update the dependency list in `README.md`.

## 5. Verify

Delete `Build/<configure preset>/` and run the workflow presets from the `build-and-test` skill, so the fetch and a from-scratch build are both proven. Then follow the `pre-commit-review` skill.
