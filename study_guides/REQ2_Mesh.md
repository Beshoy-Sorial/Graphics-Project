# Requirement 2: Mesh — Complete Study Guide

---

## 🧠 What is a Mesh?

A **mesh** is the 3D shape of an object. To define a mesh, we need:

1. **Vertices** — the corner points of the 3D shape (each vertex has a position, color, texture coordinate, and normal).
2. **Triangles** — every 3D face is broken into triangles. Each triangle is defined by three vertex indices.

> **Why triangles?** The GPU is extremely fast at drawing triangles. Every 3D shape — a cube, a character, a car — is made of many tiny triangles.

---

## 📦 How the GPU Stores Mesh Data

OpenGL uses **three objects** to store mesh data on the GPU (in VRAM — Video RAM):

| OpenGL Object | Full Name | What it Stores |
|---|---|---|
| **VBO** | Vertex Buffer Object | All vertex data (positions, colors, UVs, normals) |
| **EBO** | Element Buffer Object | Triangle indices (which 3 vertices form each triangle) |
| **VAO** | Vertex Array Object | The "recipe" that says how to read the VBO |

**Analogy:**
- VBO = a big array of ingredients (flour, eggs, sugar…)
- EBO = a recipe that says "use ingredient 0, 2, and 5 for this step"
- VAO = a saved configuration that says "for flour use bowl 1, for eggs use bowl 2…"

---

## 📁 Relevant Files

| File | Purpose |
|---|---|
| `source/common/mesh/mesh.hpp` | The `Mesh` class — constructor, draw, destructor |
| `source/common/mesh/vertex.hpp` | The `Vertex` struct — what data each vertex holds |

---

## 📖 Understanding `vertex.hpp` — The Vertex Struct

```cpp
struct Vertex {
    glm::vec3 position;   // (x, y, z) position in 3D space
    Color     color;      // RGBA color as 4 bytes (0–255 each)
    glm::vec2 tex_coord;  // (u, v) texture coordinate (0.0–1.0 range)
    glm::vec3 normal;     // Direction the surface is "facing" — used for lighting
};
```

### What is a `glm::vec3`?
A 3D vector: three floats `(x, y, z)`. Used for positions and normals.

### What is a `glm::vec2`?
A 2D vector: two floats `(u, v)`. Used for texture coordinates.

### What is `Color`?
```cpp
typedef glm::vec<4, glm::uint8, glm::defaultp> Color;
```
A 4-component vector of **bytes** (0–255), representing R, G, B, A. Using bytes instead of floats saves memory: 4 bytes vs 16 bytes per color.

### What is a normal?
A **normal** is a direction vector that points perpendicular to the surface. It will be used in Phase 2 for lighting calculations. For now, it's stored but not used heavily.

---

## 📖 Understanding `mesh.hpp` — The Mesh Class

### Member Variables

```cpp
class Mesh {
    unsigned int VBO, EBO;  // IDs for the vertex and element buffers (on GPU)
    unsigned int VAO;        // ID for the vertex array object
    GLsizei elementCount;    // Number of elements (indices) = 3 × number of triangles
```

These are all just integer IDs. OpenGL manages the actual memory on the GPU.

---

### The Constructor — Uploading Data to the GPU

```cpp
Mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& elements)
{
    elementCount = static_cast<GLsizei>(elements.size());
```

`elements` is a list of indices, e.g. `{0, 1, 2, 0, 2, 3}` = two triangles.
`elementCount` remembers how many there are (needed for `glDrawElements` later).

---

#### Step 1: Generate the GPU Objects

```cpp
    glGenVertexArrays(1, &VAO);  // Create 1 VAO, store its ID in VAO
    glGenBuffers(1, &VBO);       // Create 1 buffer, store ID in VBO
    glGenBuffers(1, &EBO);       // Create 1 buffer, store ID in EBO
```

`glGen*` functions ask OpenGL to create objects and give you their IDs. Think of it like calling `new` but for GPU memory.

---

#### Step 2: Bind the VAO (Start Recording)

```cpp
    glBindVertexArray(VAO);
```

Binding the VAO means: *"start recording what I'm about to set up"*. Everything that follows (binding VBO/EBO and setting attribute pointers) gets **saved inside this VAO**.

---

#### Step 3: Upload Vertex Data to VBO

```cpp
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER,              // Target: vertex buffer
                 vertices.size() * sizeof(Vertex), // Total bytes
                 vertices.data(),              // Pointer to the data in RAM
                 GL_STATIC_DRAW);             // Hint: we won't change this data
```

- `GL_ARRAY_BUFFER` = the slot for vertex data.
- `vertices.data()` = a raw pointer to the array of `Vertex` structs.
- `GL_STATIC_DRAW` = optimization hint: we upload once, draw many times.

This copies the vertex array from **RAM → VRAM (GPU memory)**.

---

#### Step 4: Upload Index Data to EBO

```cpp
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 elements.size() * sizeof(unsigned int),
                 elements.data(),
                 GL_STATIC_DRAW);
```

Same idea, but for the triangle index list. `GL_ELEMENT_ARRAY_BUFFER` = the slot for index data.

Note: because the VAO is currently bound, it **automatically remembers** which EBO you used.

---

#### Step 5: Tell OpenGL How to Read the VBO (Vertex Attributes)

This is the most complex step. OpenGL needs to know: *"In this big array of bytes, where does each piece of data (position, color, etc.) start?"*

```cpp
// Attribute 0: position (vec3 = 3 floats)
glEnableVertexAttribArray(ATTRIB_LOC_POSITION);  // ATTRIB_LOC_POSITION = 0
glVertexAttribPointer(
    ATTRIB_LOC_POSITION,         // location=0 in the vertex shader
    3,                           // 3 components (x, y, z)
    GL_FLOAT,                    // each component is a float
    GL_FALSE,                    // don't normalize
    sizeof(Vertex),              // stride: how many bytes to jump to the next vertex
    reinterpret_cast<void*>(offsetof(Vertex, position))  // offset: where in the vertex does position start?
);
```

Let's break this down:

- **Location** = which `layout(location=X)` in the vertex shader this maps to.
- **3** = number of components (vec3 → 3).
- **GL_FLOAT** = data type. 
- **GL_FALSE** = no normalization (for positions). For colors: GL_TRUE normalizes bytes 0–255 to 0.0–1.0.
- **stride** = `sizeof(Vertex)` = total size of one vertex struct. OpenGL jumps this many bytes to get to the next vertex.
- **offset** = `offsetof(Vertex, position)` = how many bytes from the start of the struct to this field.

The same pattern is repeated for all 4 attributes:

```cpp
// Attribute 1: color (4 unsigned bytes, normalized to 0.0–1.0)
glEnableVertexAttribArray(ATTRIB_LOC_COLOR);
glVertexAttribPointer(ATTRIB_LOC_COLOR, 4, GL_UNSIGNED_BYTE, GL_TRUE,
                      sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, color)));

// Attribute 2: texture coordinate (vec2 = 2 floats)
glEnableVertexAttribArray(ATTRIB_LOC_TEXCOORD);
glVertexAttribPointer(ATTRIB_LOC_TEXCOORD, 2, GL_FLOAT, GL_FALSE,
                      sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, tex_coord)));

// Attribute 3: normal (vec3 = 3 floats)
glEnableVertexAttribArray(ATTRIB_LOC_NORMAL);
glVertexAttribPointer(ATTRIB_LOC_NORMAL, 3, GL_FLOAT, GL_FALSE,
                      sizeof(Vertex), reinterpret_cast<void*>(offsetof(Vertex, normal)));
```

---

#### Step 6: Unbind Everything (Stop Recording)

```cpp
    glBindVertexArray(0);              // Unbind VAO
    glBindBuffer(GL_ARRAY_BUFFER, 0); // Unbind VBO
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0); // Unbind EBO
```

Passing `0` means "no object". This is done to prevent accidentally modifying the VAO/VBO later.

---

### The `draw()` Function — Actually Drawing the Mesh

```cpp
void draw() {
    glBindVertexArray(VAO);  // Restore the VAO (which remembers everything we set up)
    glDrawElements(
        GL_TRIANGLES,   // Draw triangles
        elementCount,   // How many indices to use
        GL_UNSIGNED_INT,// Type of each index
        nullptr         // Offset into the EBO (0 = start)
    );
}
```

**What happens internally:**
1. OpenGL reads indices from the EBO.
2. For each group of 3 indices (one triangle), it reads those 3 vertices from the VBO.
3. It runs the vertex shader for each vertex.
4. It fills the triangle with pixels and runs the fragment shader for each pixel.

---

### The Destructor — Cleaning Up GPU Memory

```cpp
~Mesh(){
    glDeleteBuffers(1, &VBO);       // Free VBO on GPU
    glDeleteBuffers(1, &EBO);       // Free EBO on GPU
    glDeleteVertexArrays(1, &VAO);  // Free VAO on GPU
}
```

Just like `delete` in C++, you must free GPU resources when done.

---

### Why No Copy Constructor?

```cpp
Mesh(Mesh const &) = delete;
Mesh &operator=(Mesh const &) = delete;
```

If you copied a `Mesh`, both copies would have the same OpenGL ID. When one is destroyed, it deletes the GPU object — then the other copy has a dangling ID. So copying is disabled.

---

## 🗺️ Memory Layout Visualization

For a vertex with position=(1,0,0), color=(255,0,0,255), tex_coord=(1,0), normal=(0,1,0):

```
Byte offset:  0    4    8    12   16   17   18   19   20   24   28   32   35
              |    |    |    |    |    |    |    |    |    |    |    |    |
              [pos.x][pos.y][pos.z][col.r][col.g][col.b][col.a][tc.u][tc.v][n.x][n.y][n.z]
              [  float ][  float ][  float ][ byte][ byte][ byte][ byte][float][float][flt][flt][flt]
```

`sizeof(Vertex)` = 36 bytes total.

---

## 🔄 The Full Pipeline

```
CPU (RAM)                                GPU (VRAM)
─────────────────────────────────────────────────────────────
std::vector<Vertex> vertices          →  VBO (vertex buffer)
std::vector<unsigned int> elements    →  EBO (element buffer)
glVertexAttribPointer(...)            →  VAO (the recipe)

At draw time:
glBindVertexArray(VAO)               →  Activate the recipe
glDrawElements(GL_TRIANGLES, ...)    →  GPU reads EBO → VBO → runs shaders
```

---

## ✅ Key Things to Remember

| Concept | Explanation |
|---|---|
| `glGenBuffers(n, &id)` | Create `n` buffer objects, write IDs into array |
| `glBindBuffer(target, id)` | Activate a buffer (`GL_ARRAY_BUFFER` or `GL_ELEMENT_ARRAY_BUFFER`) |
| `glBufferData(target, size, data, usage)` | Upload data from RAM to GPU |
| `glGenVertexArrays(n, &id)` | Create VAO |
| `glBindVertexArray(id)` | Start using a VAO (also starts/stops recording) |
| `glVertexAttribPointer(...)` | Tell VAO how to read one attribute from VBO |
| `glEnableVertexAttribArray(loc)` | Enable an attribute at a given location |
| `glDrawElements(mode, count, type, offset)` | Draw using the EBO indices |
| Stride | Bytes from one vertex to the next (`sizeof(Vertex)`) |
| Offset | Bytes from the start of a vertex to a specific field (`offsetof(Vertex, field)`) |

---

## 🧪 How to Test

```powershell
./bin/GAME_APPLICATION.exe -c='config/mesh-test/test-0.jsonc' -f=10
```

Compare output in `screenshots/mesh-test/` with `expected/mesh-test/`.
