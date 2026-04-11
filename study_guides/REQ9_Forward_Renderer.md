# Requirement 9: Forward Renderer System — Complete Study Guide

---

## 🧠 What is a Renderer?

A **renderer** is the system that takes all objects in a scene (World with Entities) and draws them to the screen every frame. It coordinates all the knowledge from Requirements 1–8:

1. Finds the camera (Req 8 — ECS).
2. For each entity with a MeshRenderer (Req 8), creates a draw command.
3. Draws opaque objects first (Req 2 — Mesh, Req 7 — Material).
4. Sorts and draws transparent objects back-to-front (using Req 3 transforms + Req 4 pipeline state).

The **Forward Renderer** gets its name from **forward rendering** — the shader directly computes the final output color in one pass. (Contrast with *deferred rendering* which is more complex — outside scope.)

---

## 📁 Relevant Files

| File | Purpose |
|---|---|
| `source/common/systems/forward-renderer.hpp` | Class declaration: data members, function signatures |
| `source/common/systems/forward-renderer.cpp` | Full implementation: `initialize()`, `render()`, `destroy()` |

---

## 📖 Understanding the Header — Data Members

```cpp
class ForwardRenderer {
    glm::ivec2 windowSize;   // Window width + height (pixels)

    // Separate lists for opaque vs transparent objects (optimization)
    std::vector<RenderCommand> opaqueCommands;
    std::vector<RenderCommand> transparentCommands;

    // Sky sphere objects (Req 10):
    Mesh* skySphere = nullptr;
    TexturedMaterial* skyMaterial = nullptr;

    // Postprocessing objects (Req 11):
    GLuint postprocessFrameBuffer = 0;
    GLuint postProcessVertexArray = 0;
    Texture2D *colorTarget = nullptr, *depthTarget = nullptr;
    TexturedMaterial* postprocessMaterial = nullptr;

public:
    void initialize(glm::ivec2 windowSize, const nlohmann::json& config);
    void destroy();
    void render(World* world);
};
```

### The `RenderCommand` Struct

```cpp
struct RenderCommand {
    glm::mat4 localToWorld;  // The entity's world transform matrix
    glm::vec3 center;        // Center of the object in world space (for sorting)
    Mesh* mesh;              // Which mesh to draw
    Material* material;      // How to draw it
};
```

A `RenderCommand` is a **pre-packaged draw call** — everything OpenGL needs to draw one object. Instead of drawing immediately when we find a mesh renderer, we collect all commands first, sort them if needed, then draw.

---

## 📖 Understanding `render()` — The Main Function

This function is called **every frame**. Let's walk through it step by step.

### Step 1: Collect All Render Commands

```cpp
CameraComponent* camera = nullptr;
opaqueCommands.clear();
transparentCommands.clear();

for(auto entity : world->getEntities()){
    // Find the camera (look for CameraComponent)
    if(!camera) camera = entity->getComponent<CameraComponent>();

    // If entity has a MeshRendererComponent, build a command
    if(auto meshRenderer = entity->getComponent<MeshRendererComponent>(); meshRenderer){
        RenderCommand command;
        command.localToWorld = meshRenderer->getOwner()->getLocalToWorldMatrix();
        command.center = glm::vec3(command.localToWorld * glm::vec4(0, 0, 0, 1));
        command.mesh = meshRenderer->mesh;
        command.material = meshRenderer->material;

        // Sort by transparency:
        if(command.material->transparent){
            transparentCommands.push_back(command);  // Transparent → separate list
        } else {
            opaqueCommands.push_back(command);       // Opaque → other list
        }
    }
}
```

- `command.center`: The object's world-space center. Computed by transforming the local origin `(0,0,0,1)` by the world matrix.
- We separate opaque and transparent because transparent objects need **sorting**, opaque ones don't.

---

### Step 2: Exit if No Camera

```cpp
if(camera == nullptr) return;  // Can't render without a camera!
```

---

### Step 3: Compute Camera Direction and Sort Transparent Objects

This is important! Transparent objects must be drawn **from far to near** to look correct. We measure "distance" along the camera's forward axis.

```cpp
glm::mat4 cameraLocalToWorld = camera->getOwner()->getLocalToWorldMatrix();
glm::vec3 cameraPosition = glm::vec3(cameraLocalToWorld * glm::vec4(0, 0, 0, 1));
glm::vec3 cameraForward  = glm::vec3(cameraLocalToWorld * glm::vec4(0, 0, -1, 0));
//                                                          ↑ direction, w=0 (no translation)
```

**Camera forward direction in OpenGL local space = (0, 0, -1)**
We transform it to world space to get the actual world-space forward direction.

```cpp
std::sort(transparentCommands.begin(), transparentCommands.end(),
    [cameraForward, cameraPosition](const RenderCommand& first, const RenderCommand& second){
        // Project each object's center onto the camera forward axis
        // Larger value = farther along the camera direction = farther away
        float depthFirst  = glm::dot(first.center  - cameraPosition, cameraForward);
        float depthSecond = glm::dot(second.center - cameraPosition, cameraForward);
        return depthFirst > depthSecond;  // Sort: largest (farthest) first
    }
);
```

**Why use `dot` product for depth?**

`glm::dot(objectPos - cameraPos, cameraForward)` = projection of the vector from camera to object onto the camera's forward axis = **signed depth** (positive = in front of camera).

Comparison `>` means: *"first should come before second if first is farther away"*. This sorts from far to near (**painter's algorithm**).

---

### Step 4: Compute the View-Projection Matrix

```cpp
glm::mat4 VP = camera->getProjectionMatrix(windowSize) * camera->getViewMatrix();
```

This is the combined matrix:
- **View matrix** (`getViewMatrix()`) → transforms world space to camera space.
- **Projection matrix** (`getProjectionMatrix()`) → transforms camera space to clip space (applies perspective/orthographic).

```
World Space → [View Matrix] → Camera Space → [Projection Matrix] → Clip Space → NDC
```

**The final transform per object:**
```
gl_Position = VP × localToWorld × localPosition
           = Projection × View × Model × position
```

---

### Step 5: Set Up OpenGL State

```cpp
glViewport(0, 0, windowSize.x, windowSize.y);  // Tell OpenGL the screen size
glClearColor(0.0f, 0.0f, 0.0f, 1.0f);          // Clear to black
glClearDepth(1.0f);                              // Far plane depth = 1.0 (farthest)
glColorMask(true, true, true, true);             // Allow all color channels to be written
glDepthMask(true);                               // Allow depth to be written
glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Actually clear!
```

**`glViewport(x, y, w, h)`:** Tells OpenGL which part of the window to render to. NDC coordinates map to this rectangle.

**`glClearColor(r,g,b,a)`:** Sets the color OpenGL uses when clearing the color buffer.

**`glClearDepth(d)`:** Sets what value is written to the depth buffer on clear (1.0 = maximum depth = "nothing drawn yet").

**`glClear(mask)`:** Actually does the clearing. |  uses bitwise OR to combine flags.

---

### Step 6: Draw Opaque Objects

```cpp
for(const auto& command : opaqueCommands){
    command.material->setup();  // Apply pipeline state + bind shader + set uniforms
    command.material->shader->set("transform", VP * command.localToWorld);  // MVP matrix
    command.mesh->draw();       // Draw the mesh triangles
}
```

For each opaque object:
1. **`material->setup()`** — sets up OpenGL state (depth, culling, blending), activates shader, binds textures, and sends tint/other uniforms.
2. **`shader->set("transform", VP * localToWorld)`** — sends the combined MVP (Model-View-Projection) matrix. `VP` is the combined View×Projection from the camera. `localToWorld` is the model matrix from the entity.
3. **`mesh->draw()`** — executes `glDrawElements`, which runs the shader pipeline for all triangles.

---

### Step 7: (Sky — Req 10, see REQ10 guide)

After opaque, before transparent — sky is drawn here.

---

### Step 8: Draw Transparent Objects

```cpp
for(const auto& command : transparentCommands){
    command.material->setup();
    command.material->shader->set("transform", VP * command.localToWorld);
    command.mesh->draw();
}
```

Same as opaque, but now they are already sorted far-to-near (from Step 3), so blending occurs correctly.

---

### Step 9: (Postprocessing — Req 11, see REQ11 guide)

After all objects are drawn, if postprocessing is enabled, it applies the effect.

---

## 🗺️ Rendering Order — Why It Matters

```
Frame starts:
  ┌─────────────────────────────────────────────┐
  │  1. Clear color + depth buffers             │
  │  2. Draw OPAQUE objects (any order)         │
  │     - Depth testing prevents wrong overlap  │
  │  3. Draw SKY SPHERE (Req 10)                │
  │     - Only where nothing was drawn (z=1)   │
  │  4. Draw TRANSPARENT objects (FAR → NEAR)  │
  │     - Painter's algorithm with sorting     │
  └─────────────────────────────────────────────┘
Frame ends → present to screen
```

**Why draw opaque first?**
Opaque objects have depth testing enabled. Drawing them first fills the depth buffer, so the sky sphere (drawn second) only appears in empty areas. Drawing transparent objects last means they blend correctly over the scene.

---

## 🔢 The MVP Transform Chain

```
Vertex local position: (1, 0, 0)
         ↓  × Model Matrix (localToWorld)
World position: (5, 2, -3)  ← entity was at (4, 2, -3) with scale 1
         ↓  × View Matrix
Camera space: (-0.5, -1, -8)
         ↓  × Projection Matrix
Clip space: (0.1, 0.3, 0.9, 1.0)
         ÷ w (perspective divide)
NDC: (0.1, 0.3, 0.9)   ← in [-1, 1] range
         ↓
Viewport transform
Screen pixel: (640, 400)  ← drawn here!
```

---

## ✅ Key Things to Remember

| Concept | Details |
|---|---|
| `RenderCommand` | Holds `localToWorld`, `center`, `mesh`, `material` for one draw call |
| Draw order | Opaque first, then sky, then transparent (far-to-near) |
| `VP = Proj × View` | Camera transforms: View brings world to camera space, Proj adds perspective |
| `transform = VP × localToWorld` | The final MVP matrix sent to the shader |
| `cameraForward = mat × (0,0,-1,0)` | Camera looks in -Z in local space; w=0 for direction |
| `dot(pos - camPos, forward)` | Signed depth of an object along camera axis |
| Sort transparent descending | Largest depth (farthest) should be drawn first |
| `glViewport(0,0,w,h)` | Defines the rectangle in the window to render to |
| `glClear(COLOR | DEPTH)` | Clears both buffers before drawing a new frame |
| `glClearColor` / `glClearDepth` | Sets values used when clearing |

---

## 🧪 How to Test

```powershell
./bin/GAME_APPLICATION.exe -c='config/renderer-test/test-1.jsonc' -f=10
```

Compare output in `screenshots/renderer-test/` with `expected/renderer-test/`.
