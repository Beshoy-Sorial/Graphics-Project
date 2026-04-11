# Requirement 4: Pipeline State — Complete Study Guide

---

## 🧠 What is the OpenGL Pipeline State?

OpenGL is a **state machine**. This means OpenGL always has a set of "current settings" that affect how it draws. When you change a setting, it stays changed until you change it again.

Imagine a printer with settings: color mode, paper size, duplex printing. Before printing a document, you configure the settings you want — they stay until you change them.

In OpenGL, the "settings" that matter for rendering include:
- Should we **discard back-facing triangles** (Face Culling)?
- Should we **check if something is behind another object** (Depth Testing)?
- Should we **blend transparent colors together** (Blending)?
- Which **color channels** should we write to (Color Mask)?
- Should we **write to the depth buffer** (Depth Mask)?

The `PipelineState` struct stores all these options and can apply them in one shot.

---

## 📁 Relevant Files

| File | Purpose |
|---|---|
| `source/common/material/pipeline-state.hpp` | Struct definition + `setup()` function |
| `source/common/material/pipeline-state.cpp` | JSON deserialization |

---

## 🔵 Feature 1: Face Culling

### What is it?

A triangle has **two sides** — a front face and a back face. For a closed 3D object (like a cube), you can never see the inside/back faces. Drawing them wastes GPU time.

**Face culling** tells OpenGL to skip (cull) triangles that face away from the camera.

### How does OpenGL know which face is "front"?

OpenGL uses **winding order** — the order in which you listed the 3 vertices:
- **Counter-clockwise (CCW)** when viewed from the front → front face.
- **Clockwise (CW)** when viewed from the front → back face.

```
Front face (CCW):        Back face (CW):
      2                        1
     / \                      / \
    /   \                    /   \
   0-----1                  0-----2
```

### The Code

```cpp
struct {
    bool enabled = false;         // Is face culling on? Default: OFF
    GLenum culledFace = GL_BACK;  // Which face to discard? Default: discard BACK faces
    GLenum frontFace = GL_CCW;    // What is "front"? Default: counter-clockwise
} faceCulling;
```

```cpp
// In setup():
if(faceCulling.enabled) glEnable(GL_CULL_FACE);   // Turn ON culling
else                    glDisable(GL_CULL_FACE);  // Turn OFF culling

glCullFace(faceCulling.culledFace);  // GL_BACK, GL_FRONT, or GL_FRONT_AND_BACK
glFrontFace(faceCulling.frontFace);  // GL_CCW or GL_CW
```

**Common Options:**
| Option | Meaning |
|---|---|
| `GL_BACK` | Cull (discard) back faces (most common) |
| `GL_FRONT` | Cull front faces (rarely used; can be useful for some effects) |
| `GL_CCW` | Counter-clockwise = front face (OpenGL default) |
| `GL_CW` | Clockwise = front face |

---

## 🔵 Feature 2: Depth Testing

### What is it?

When two objects overlap on screen, we want the **closer object** to appear in front. Without depth testing, the last-drawn object would always appear on top, regardless of depth.

OpenGL maintains a **depth buffer** (also called z-buffer) — a texture the same size as the screen where each pixel stores the depth (0.0 = near, 1.0 = far) of the closest drawn object so far.

**Algorithm:**
1. For each fragment (pixel), check: is this fragment's depth less than the stored depth?
2. If yes → draw it and update the depth buffer.
3. If no → discard it (something closer is already there).

### The Code

```cpp
struct {
    bool enabled = false;          // Is depth testing on? Default: OFF
    GLenum function = GL_LEQUAL;   // Comparison function: pass if new_depth ≤ stored_depth
} depthTesting;
```

```cpp
// In setup():
if(depthTesting.enabled) glEnable(GL_DEPTH_TEST);
else                     glDisable(GL_DEPTH_TEST);

glDepthFunc(depthTesting.function);  // The comparison function
```

**Comparison Functions:**
| Enum | Meaning | Use Case |
|---|---|---|
| `GL_LESS` | Pass if `new < stored` | Default-style depth test |
| `GL_LEQUAL` | Pass if `new ≤ stored` | Used here (allows redrawing at same depth) |
| `GL_ALWAYS` | Always pass | Useful to draw UI overlaid on everything |
| `GL_NEVER` | Never pass | Nothing gets drawn |
| `GL_EQUAL` | Pass if `new == stored` | Rare; used for multi-pass effects |

> ⚠️ Don't forget: for depth testing to work, you also need to clear the depth buffer at the start of each frame with `glClear(GL_DEPTH_BUFFER_BIT)`.

---

## 🔵 Feature 3: Blending

### What is it?

**Blending** is how OpenGL handles **transparent objects**. When a transparent Red object is drawn in front of a Green background, the result should be a mix of Red and Green.

The formula:
```
FinalColor = SourceFactor × SourceColor + DestFactor × DestinationColor
```

Where:
- **Source** = the new pixel being drawn (the transparent object).
- **Destination** = what's already in the framebuffer (background).

### The Code

```cpp
struct {
    bool enabled = false;                              // Is blending on?
    GLenum equation = GL_FUNC_ADD;                    // How to combine: Add
    GLenum sourceFactor = GL_SRC_ALPHA;               // Source factor
    GLenum destinationFactor = GL_ONE_MINUS_SRC_ALPHA;// Destination factor
    glm::vec4 constantColor = {0, 0, 0, 0};          // Optional constant color
} blending;
```

```cpp
// In setup():
if(blending.enabled) glEnable(GL_BLEND);
else                 glDisable(GL_BLEND);

glBlendEquation(blending.equation);  // GL_FUNC_ADD (most common)
glBlendFunc(blending.sourceFactor, blending.destinationFactor);
glBlendColor(blending.constantColor.r, blending.constantColor.g,
             blending.constantColor.b, blending.constantColor.a);
```

**The Standard Transparency Formula:**

With `sourceFactor = GL_SRC_ALPHA` and `destFactor = GL_ONE_MINUS_SRC_ALPHA`:

```
FinalColor = alpha × SourceColor + (1 - alpha) × DestinationColor
```

Example: source color = (1,0,0) with alpha=0.5 (50% transparent red), destination = (0,1,0) green:
```
FinalColor = 0.5 × (1,0,0) + 0.5 × (0,1,0) = (0.5, 0.5, 0)  → yellow-ish
```

**Common Factor Values:**
| Enum | Value |
|---|---|
| `GL_SRC_ALPHA` | The source fragment's alpha value |
| `GL_ONE_MINUS_SRC_ALPHA` | 1 − source alpha |
| `GL_ONE` | Constant 1.0 |
| `GL_ZERO` | Constant 0.0 |
| `GL_DST_ALPHA` | The destination's alpha value |

> ⚠️ **Transparency Requires Sorting!** Blending only works correctly if you draw transparent objects **back to front** (far → near). If you draw them front to back, the near object updates the depth buffer, hiding the far object.

---

## 🔵 Feature 4: Color Mask

### What is it?

The color mask controls **which color channels get written** to the framebuffer. You can independently enable/disable writing to Red, Green, Blue, and Alpha channels.

```cpp
glm::bvec4 colorMask = {true, true, true, true}; // Default: write all 4 channels
```

```cpp
// In setup():
glColorMask(colorMask.r, colorMask.g, colorMask.b, colorMask.a);
```

**Use Cases:**
- `{false, false, false, false}` → Draw nothing to the color buffer (but depth still writes).
- `{true, false, false, false}` → Only write the red channel.
- Useful for multi-pass rendering techniques.

---

## 🔵 Feature 5: Depth Mask

### What is it?

Controls whether depth values are **written to the depth buffer**.

```cpp
bool depthMask = true; // Default: write depth values
```

```cpp
// In setup():
glDepthMask(depthMask); // GL_TRUE or GL_FALSE
```

**Use Case:**
- `depthMask = false` → The sky! The sky sphere is drawn as if it's at the far plane, but we don't want it to actually update depth values (because then objects behind the sky would be culled). See Requirement 10.

---

## 📖 Understanding `setup()` — The Full Function

```cpp
void setup() const {
    // --- Face Culling ---
    if(faceCulling.enabled) glEnable(GL_CULL_FACE);
    else                    glDisable(GL_CULL_FACE);
    glCullFace(faceCulling.culledFace);
    glFrontFace(faceCulling.frontFace);

    // --- Depth Testing ---
    if(depthTesting.enabled) glEnable(GL_DEPTH_TEST);
    else                     glDisable(GL_DEPTH_TEST);
    glDepthFunc(depthTesting.function);

    // --- Blending ---
    if(blending.enabled) glEnable(GL_BLEND);
    else                 glDisable(GL_BLEND);
    glBlendEquation(blending.equation);
    glBlendFunc(blending.sourceFactor, blending.destinationFactor);
    glBlendColor(blending.constantColor.r, blending.constantColor.g,
                 blending.constantColor.b, blending.constantColor.a);

    // --- Masks ---
    glColorMask(colorMask.r, colorMask.g, colorMask.b, colorMask.a);
    glDepthMask(depthMask);
}
```

This function is called **before drawing each object**. It sets up the OpenGL state to match what this object needs.

---

## 📖 Understanding `deserialize()` — Reading from JSON

```cpp
void PipelineState::deserialize(const nlohmann::json& data){
    if(!data.is_object()) return;

    if(data.contains("faceCulling")){
        const auto& config = data["faceCulling"];
        faceCulling.enabled = config.value("enabled", faceCulling.enabled);
        // Look up "culledFace" string → GL enum:
        if(auto it = gl_enum_deserialize::facets.find(config.value("culledFace","")); 
           it != gl_enum_deserialize::facets.end())
            faceCulling.culledFace = it->second;
        // ... similar for frontFace
    }
    // ... similar blocks for depthTesting and blending
    colorMask = data.value("colorMask", colorMask);
    depthMask = data.value("depthMask", depthMask);
}
```

The `gl_enum_deserialize` maps convert string names (e.g. `"GL_BACK"`) to OpenGL enum values (e.g. `GL_BACK`). This lets config files use human-readable strings instead of raw numbers.

**Example JSON:**
```json
{
    "faceCulling": {
        "enabled": true,
        "culledFace": "GL_BACK",
        "frontFace": "GL_CCW"
    },
    "depthTesting": {
        "enabled": true,
        "function": "GL_LEQUAL"
    },
    "blending": {
        "enabled": false
    },
    "colorMask": [true, true, true, true],
    "depthMask": true
}
```

---

## 🗺️ Visual Summary of All Features

```
                      ┌─────────────────────────────┐
New Fragment (pixel)  │                             │
     ↓                │    Face Culling             │
  [Is triangle facing away?] ──YES──> DISCARD      │
     ↓ NO             │                             │
  [Depth Test: is there      │    Depth Testing     │
   something closer?]──YES──> DISCARD               │
     ↓ NO             │                             │
  [Blending: mix with        │    Blending          │
   background if enabled]    │                      │
     ↓                │                             │
  [Write to Color Buffer?]   │    Color Mask        │
  [Write to Depth Buffer?]   │    Depth Mask        │
                      └─────────────────────────────┘
```

---

## ✅ Key Things to Remember

| Concept | Key Function | Common Values |
|---|---|---|
| Face Culling | `glEnable/Disable(GL_CULL_FACE)`, `glCullFace()`, `glFrontFace()` | `GL_BACK`, `GL_CCW` |
| Depth Testing | `glEnable/Disable(GL_DEPTH_TEST)`, `glDepthFunc()` | `GL_LEQUAL`, `GL_LESS` |
| Blending | `glEnable/Disable(GL_BLEND)`, `glBlendFunc()`, `glBlendEquation()` | `GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA` |
| Color Mask | `glColorMask(r, g, b, a)` | `{true,true,true,true}` |
| Depth Mask | `glDepthMask(bool)` | `true` (write), `false` (read-only) |

---

## 🧪 How to Test

```powershell
./bin/GAME_APPLICATION.exe -c='config/pipeline-test/dt-1.jsonc' -f=10
```

Compare output in `screenshots/pipeline-test/` with `expected/pipeline-test/`.
