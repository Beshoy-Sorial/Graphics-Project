# CMP3060 Project - Architecture Overview

This repository is a small C++/OpenGL game framework with a playable scene, a menu, and several lab-style test states. It is organized like a lightweight engine layer plus a set of states that demonstrate or exercise different rendering and ECS features.

The project builds one executable, `GAME_APPLICATION`, using CMake. At runtime, the app loads `config/app.jsonc`, creates a window with GLFW, initializes OpenGL/GLAD/ImGui, registers all available states, then switches to the configured start state.

## Big Picture

The runtime flow is:

1. `source/main.cpp` parses command-line flags and loads `config/app.jsonc`.
2. `our::Application` is created with that JSON config.
3. The app registers named states such as `menu`, `play`, `shader-test`, and `entity-test`.
4. The configured start state is activated.
5. The application loop handles input, ImGui, frame timing, drawing, screenshots, and deferred state changes.

That means the project is split into two main layers:

- `source/common`: reusable engine/framework code.
- `source/states`: the actual app states and test scenes built on top of that framework.

## Architecture

### 1. Application Layer

Core entry points:

- [source/main.cpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/main.cpp)
- [source/common/application.hpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/common/application.hpp)
- [source/common/application.cpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/common/application.cpp)

`our::Application` is the top-level runtime manager. It is responsible for:

- Creating and destroying the GLFW window and OpenGL context.
- Initializing GLAD and ImGui.
- Owning keyboard and mouse input wrappers.
- Registering and switching between named states.
- Running the main loop and forwarding lifecycle calls to the active state.
- Handling screenshots and frame-limited runs for testing.

Each state inherits from `our::State` and can override:

- `onInitialize()`
- `onImmediateGui()`
- `onDraw(deltaTime)`
- `onDestroy()`
- input callbacks like `onKeyEvent(...)` and `onMouseButtonEvent(...)`

This gives the project a simple scene/state machine architecture.

### 2. State Layer

Main state files:

- [source/states/menu-state.hpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/states/menu-state.hpp)
- [source/states/play-state.hpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/states/play-state.hpp)

The important states are:

- `Menustate`: draws a full-screen textured menu, handles hover/click logic, and switches to `play`.
- `Playstate`: loads assets and a world from JSON, runs ECS systems, and renders the game scene.
- `*-test-state.hpp`: isolated learning/demo states for shaders, meshes, transforms, materials, ECS, renderer behavior, and more.

In practice:

- `menu` is the UI/front door.
- `play` is the actual gameplay-style ECS scene.
- test states are focused sandboxes for course requirements and graphics experiments.

### 3. Data-Driven Scene Setup

Primary config:

- [config/app.jsonc](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/config/app.jsonc)

The app config defines:

- window title, size, and fullscreen mode
- the `start-scene`
- renderer options such as sky and postprocess shader
- asset declarations
- world/entity hierarchy

`Playstate` reads `scene.assets` and `scene.world` from JSON, so most scene content is data-driven rather than hardcoded.

This is one of the most important design choices in the project:

- assets are named in JSON
- entities reference those named assets
- systems and renderer consume the deserialized world

## Engine Modules

### Asset System

Key files:

- [source/common/asset-loader.hpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/common/asset-loader.hpp)
- [source/common/asset-loader.cpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/common/asset-loader.cpp)

The asset loader is a static, type-specialized registry. It loads and stores shared assets by string name:

- `ShaderProgram`
- `Texture2D`
- `Sampler`
- `Mesh`
- `Material`

`deserializeAllAssets(...)` loads them in dependency order, then gameplay/rendering code retrieves them by name. This keeps meshes/materials/textures reusable across many entities.

### ECS Layer

Key files:

- [source/common/ecs/world.hpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/common/ecs/world.hpp)
- [source/common/ecs/world.cpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/common/ecs/world.cpp)
- [source/common/ecs/entity.hpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/common/ecs/entity.hpp)
- [source/common/ecs/entity.cpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/common/ecs/entity.cpp)
- [source/common/ecs/transform.hpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/common/ecs/transform.hpp)

This project uses a lightweight ECS-style model:

- `World` owns entities.
- `Entity` owns components.
- `Transform` stores local position/rotation/scale.
- parent/child relationships create transform hierarchies.

Important components include:

- [source/common/components/camera.hpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/common/components/camera.hpp)
- [source/common/components/mesh-renderer.hpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/common/components/mesh-renderer.hpp)
- [source/common/components/free-camera-controller.hpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/common/components/free-camera-controller.hpp)
- [source/common/components/movement.hpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/common/components/movement.hpp)
- [source/common/components/component-deserializer.hpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/common/components/component-deserializer.hpp)

Typical flow:

1. JSON is deserialized into entities.
2. Components are attached based on each JSON object's `"type"`.
3. Systems iterate the world and act on matching components.
4. The renderer looks for `CameraComponent` and `MeshRendererComponent` instances.

### Systems Layer

Key files:

- [source/common/systems/movement.hpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/common/systems/movement.hpp)
- [source/common/systems/free-camera-controller.hpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/common/systems/free-camera-controller.hpp)
- [source/common/systems/forward-renderer.hpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/common/systems/forward-renderer.hpp)
- [source/common/systems/forward-renderer.cpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/common/systems/forward-renderer.cpp)

Systems are plain classes that operate on world data:

- `MovementSystem` updates transforms using linear/angular velocity.
- `FreeCameraControllerSystem` turns input into camera motion.
- `ForwardRenderer` gathers render commands and draws the world.

This keeps behavior outside entity classes and makes scene updates easier to reason about.

### Rendering and Materials

Supporting files live under:

- `source/common/shader`
- `source/common/material`
- `source/common/mesh`
- `source/common/texture`

The rendering stack is roughly:

1. A `MeshRendererComponent` points to a `Mesh` and a `Material`.
2. The renderer builds render commands from visible world data.
3. Materials configure shader programs, textures, samplers, tinting, transparency, and OpenGL pipeline state.
4. Meshes submit geometry.

`PipelineState` is especially important because it centralizes low-level GL state like:

- face culling
- depth testing
- blending
- depth write mask

That makes material behavior mostly declarative and JSON-friendly.

## Folder Guide

### `source/`

- `main.cpp`: app bootstrap and state registration.
- `common/`: reusable engine/framework code.
- `states/`: concrete runtime states and tests.

### `assets/`

Runtime content such as:

- shaders
- textures
- models

### `config/`

JSON/JSONC scene and test configurations. Different subfolders correspond to different test states.

### `vendor/`

Third-party dependencies, including GLFW, GLAD, ImGui, GLM, and utility code.

### `docs/`

Reference images and supporting documentation artifacts.

## Current Status of the Student Version

This repository is not just a finished engine; it is also a coursework scaffold. Several core files still contain guided `TODO` sections, especially around:

- `World` entity ownership and deletion
- `Entity` component management and transform composition
- component deserialization coverage
- transparent object sorting
- forward renderer setup
- sky rendering
- post-processing framebuffer setup

Examples:

- [source/common/ecs/world.hpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/common/ecs/world.hpp)
- [source/common/ecs/entity.hpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/common/ecs/entity.hpp)
- [source/common/systems/forward-renderer.cpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/common/systems/forward-renderer.cpp)

So the architecture is already defined, but some implementation checkpoints are intentionally left for the student to complete.

## Best Files to Read First

If you want to understand the project quickly, read in this order:

1. [source/main.cpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/main.cpp)
2. [source/common/application.hpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/common/application.hpp)
3. [source/states/menu-state.hpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/states/menu-state.hpp)
4. [source/states/play-state.hpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/states/play-state.hpp)
5. [config/app.jsonc](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/config/app.jsonc)
6. [source/common/ecs/world.hpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/common/ecs/world.hpp)
7. [source/common/systems/forward-renderer.hpp](/e:/Downloads/Edge/CMP3060%20Project%20-%20Student%20Version/source/common/systems/forward-renderer.hpp)

That path shows the full chain from startup, to state switching, to data loading, to ECS, to rendering.

## Build and Run

This project uses CMake and produces the `GAME_APPLICATION` executable.

Typical local workflow:

```powershell
cmake -S . -B build
cmake --build build
.\bin\GAME_APPLICATION.exe
```

Useful command-line options from `main.cpp`:

```powershell
.\bin\GAME_APPLICATION.exe -c config/app.jsonc
.\bin\GAME_APPLICATION.exe -f 300
```

- `-c` selects the config file.
- `-f` runs for a fixed number of frames, which is helpful for automated testing/screenshots.

## Short Architecture Summary

The project is a small state-driven OpenGL app. `Application` owns the window, input, and state machine. `Playstate` loads a JSON-defined world into a simple ECS. Systems update camera and motion, while `ForwardRenderer` turns `MeshRendererComponent` data into draw calls using shared assets and material-defined pipeline state.
