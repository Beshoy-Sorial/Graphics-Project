# Requirement 3: Transform — Complete Study Guide

---

## 🧠 What is a Transformation?

When you draw the same 3D mesh multiple times (e.g., 10 trees), you don't want them all stacked on top of each other. You want them at **different positions, rotations, and sizes**.

A **Transform** lets you:
- **Translate** (move) an object — change its position.
- **Rotate** an object — spin it around an axis.
- **Scale** an object — make it bigger or smaller.

In 3D graphics, all three operations are combined into a single **4×4 matrix** sent to the vertex shader.

---

## 📐 Coordinate Spaces

Before understanding transforms, it helps to know these coordinate spaces:

| Space | Description |
|---|---|
| **Local Space** | The object's own coordinate system. Origin = center of the object. |
| **World Space** | The global scene coordinate system. |
| **Camera Space** | The coordinate system relative to the camera. |
| **Clip Space** | A special space OpenGL uses internally after applying projection. |
| **NDC** | Normalized Device Coordinates — the final [-1,1] cube. |

The **transform matrix** converts a vertex from **Local Space → World Space**. The vertex shader applies it to every vertex position.

---

## 📁 Relevant Files

| File | Purpose |
|---|---|
| `source/common/ecs/transform.hpp` | Declares the `Transform` struct |
| `source/common/ecs/transform.cpp` | Implements `toMat4()` and `deserialize()` |
| `assets/shaders/transform-test.vert` | The vertex shader that applies the transform matrix |

---

## 📖 Understanding `transform.hpp` — The Struct

```cpp
struct Transform {
public:
    glm::vec3 position = glm::vec3(0, 0, 0); // Translation (default: no movement)
    glm::vec3 rotation = glm::vec3(0, 0, 0); // Euler angles in radians (default: no rotation)
    glm::vec3 scale    = glm::vec3(1, 1, 1); // Scale per axis (default: no scaling)

    glm::mat4 toMat4() const;              // Convert to a 4×4 matrix
    void deserialize(const nlohmann::json&); // Read from JSON config file
};
```

**Three human-friendly numbers instead of a 16-number matrix** — that's the whole point of this struct.

---

## 📐 The Three Transformation Matrices

### 1. Translation Matrix

Moves the object by `(tx, ty, tz)`:

```
| 1  0  0  tx |
| 0  1  0  ty |
| 0  0  1  tz |
| 0  0  0  1  |
```

Using GLM: `glm::translate(glm::mat4(1.0f), position)`

The `glm::mat4(1.0f)` creates the **identity matrix** (1s on diagonal, 0s elsewhere — acts like multiplying by 1).

---

### 2. Rotation Matrix (Euler Angles)

Euler angles describe rotation as three separate rotations around the X, Y, and Z axes.

In this project, the order is: **Roll (Z) → Pitch (X) → Yaw (Y)**

| Angle | Axis | Common Name |
|---|---|---|
| `rotation.z` | Z-axis | Roll (tilt left/right) |
| `rotation.x` | X-axis | Pitch (tilt up/down) |
| `rotation.y` | Y-axis | Yaw (turn left/right) |

Using GLM: `glm::yawPitchRoll(rotation.y, rotation.x, rotation.z)`

> **Important:** The config files store angles in **degrees**, but they are converted to **radians** during `deserialize()`. So `toMat4()` receives radians and doesn't need to convert.

---

### 3. Scale Matrix

Stretches the object by `(sx, sy, sz)`:

```
| sx  0   0   0 |
| 0   sy  0   0 |
| 0   0   sz  0 |
| 0   0   0   1 |
```

Using GLM: `glm::scale(glm::mat4(1.0f), scale)`

---

## 📖 Understanding `transform.cpp` — The Implementation

### `toMat4()` — Building the Combined Matrix

```cpp
glm::mat4 Transform::toMat4() const {
    glm::mat4 translation_matrix = glm::translate(glm::mat4(1.0f), position);
    glm::mat4 rotation_matrix    = glm::yawPitchRoll(rotation.y, rotation.x, rotation.z);
    glm::mat4 scale_matrix       = glm::scale(glm::mat4(1.0f), scale);
    return translation_matrix * rotation_matrix * scale_matrix;
}
```

**⚠️ Order Matters!**

The order of matrix multiplication is: **Scale → Rotate → Translate**

```
FinalMatrix = Translation × Rotation × Scale
```

When you multiply matrices right-to-left, the rightmost operation is applied first:
1. Scale first → object changes size around its origin.
2. Then rotate → object spins around its (now-scaled) local origin.
3. Then translate → object moves to its final position.

**Why this order?**
If you translated first and then scaled, the translation distance would also get scaled! You want the object to first be formed "locally" (scale + rotate) and then placed in the world (translate).

---

### `deserialize()` — Reading from JSON

```cpp
void Transform::deserialize(const nlohmann::json& data){
    position = data.value("position", position);  // Read "position", default to current
    rotation = glm::radians(data.value("rotation", glm::degrees(rotation))); // degrees → radians
    scale    = data.value("scale", scale);        // Read "scale", default to current
}
```

**What is `data.value("key", default)`?**
It reads a value from the JSON object. If the key doesn't exist, it returns the default value (the second argument). This is null-safe.

**Degrees ↔ Radians:**
- `glm::radians(x)` converts degrees to radians: `radians = degrees × π / 180`
- `glm::degrees(x)` converts radians to degrees: `degrees = radians × 180 / π`

The config stores `"rotation": [0, 90, 0]` in degrees.
We convert to radians for GLM's math functions.

---

### Example Config File

```json
{
    "position": [1.0, 0.0, -2.0],
    "rotation": [0.0, 45.0, 0.0],
    "scale":    [2.0, 2.0, 2.0]
}
```

This would:
- Scale the object to 2× size.
- Rotate it 45° around the Y-axis (yaw).
- Move it to position (1, 0, -2) in world space.

---

## 📖 Understanding `transform-test.vert` — Applying the Matrix

```glsl
#version 330 core

layout(location = 0) in vec3 position;  // Vertex position from VBO
layout(location = 1) in vec4 color;     // Vertex color from VBO
layout(location = 2) in vec2 tex_coord; // Texture coordinate from VBO
layout(location = 3) in vec3 normal;    // Normal from VBO

out Varyings {
    vec3 position;
    vec4 color;
    vec2 tex_coord;
    vec3 normal;
} vs_out;

uniform mat4 transform;  // The combined 4×4 matrix sent from CPU

void main(){
    gl_Position = transform * vec4(position, 1.0);  // Apply transform!
    
    vs_out.position = position;   // Pass through to fragment shader unchanged
    vs_out.color    = color;
    vs_out.tex_coord = tex_coord;
    vs_out.normal   = normal;
}
```

**The key line:**
```glsl
gl_Position = transform * vec4(position, 1.0);
```

- `position` is a `vec3` (3D). We extend it to `vec4` by adding `w=1.0`.
- The `w=1.0` allows the matrix's last column (translation) to have effect. 
  - If `w=0`, translation has NO effect (useful for direction vectors like normals).
  - If `w=1`, translation DOES have effect (useful for position points).
- `transform * vec4(...)` multiplies the 4×4 matrix by the 4D vector = transforms the vertex!

---

## 🔢 Matrix Multiplication Visualized

An object at local position (0, 0, 0), with:
- Scale: (2, 2, 2)
- Rotation: 0°
- Translation: (3, 0, 0)

```
Scale Matrix:         Rotation Matrix:      Translation Matrix:
| 2  0  0  0 |        | 1  0  0  0 |        | 1  0  0  3 |
| 0  2  0  0 |    ×   | 0  1  0  0 |    ×   | 0  1  0  0 |
| 0  0  2  0 |        | 0  0  1  0 |        | 0  0  1  0 |
| 0  0  0  1 |        | 0  0  0  1 |        | 0  0  0  1 |

Result (T × R × S):
| 2  0  0  3 |
| 0  2  0  0 |
| 0  0  2  0 |
| 0  0  0  1 |

Applied to vertex (0,0,0,1):
Result = (2×0+3, 2×0+0, 2×0+0, 1) = (3, 0, 0, 1) ✓
```

The vertex at origin ends up at (3, 0, 0) after translation. Scale doesn't affect a point at the origin!

---

## ✅ Key Things to Remember

| Concept | Explanation |
|---|---|
| `glm::translate(I, pos)` | Creates a translation matrix. `I` = identity matrix |
| `glm::scale(I, scale)` | Creates a scale matrix |
| `glm::yawPitchRoll(y, x, z)` | Creates a rotation matrix from Euler angles |
| `T × R × S` order | Scale first, then rotate, then translate |
| `vec4(pos, 1.0)` | Add `w=1` for points so translation has effect |
| `glm::radians(degrees)` | Convert degrees to radians for GLM functions |
| `data.value("key", default)` | JSON-safe read with fallback default |
| `glm::mat4(1.0f)` | Identity matrix (no transformation) |

---

## 🧪 How to Test

```powershell
./bin/GAME_APPLICATION.exe -c='config/transform-test/test-0.jsonc' -f=10
```

Compare output in `screenshots/transform-test/` with `expected/transform-test/`.
