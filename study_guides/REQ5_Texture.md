# Requirement 5: Texture — Complete Study Guide

---

## 🧠 What is a Texture?

A **texture** is an image stored on the GPU that can be "painted" onto the surface of a 3D object. Instead of giving every vertex a single color, you wrap an image around the mesh — like putting a sticker on a box.

**Common uses:**
- Brick walls, wooden floors, character skins.
- Normal maps for fake lighting detail.
- UI elements drawn in 3D space.

---

## 🗺️ How Textures Map to 3D Objects

Each vertex has **UV coordinates** (also called texture coordinates), stored in `tex_coord` (a `vec2`):
- `U` = horizontal position in the texture (0.0 = left, 1.0 = right)
- `V` = vertical position in the texture (0.0 = bottom, 1.0 = top)

The vertex shader passes these to the fragment shader, which **samples** (reads) the texture color at that coordinate.

```
Texture (2D image):           3D Triangle:
┌──────────────────┐          v2 (UV: 0.5, 1.0)
│ (0,1)    (1,1)   │         /\
│                  │        /  \
│       IMAGE      │       /    \
│                  │      /      \
│ (0,0)    (1,0)   │    v0--------v1
└──────────────────┘  (UV: 0,0)  (UV: 1, 0)
```

---

## 📁 Relevant Files

| File | Purpose |
|---|---|
| `source/common/texture/texture2d.hpp` | The `Texture2D` class — wraps an OpenGL texture object |
| `source/common/texture/texture-utils.hpp` | Header for utility functions |
| `source/common/texture/texture-utils.cpp` | `loadImage()` and `empty()` implementations |
| `assets/shaders/texture-test.frag` | Fragment shader that samples a texture |

---

## 📖 Understanding `texture2d.hpp` — The Texture Class

```cpp
class Texture2D {
    GLuint name = 0;  // The OpenGL ID for this texture (just an integer)
public:
    // Constructor: Ask OpenGL to create a texture, save its ID
    Texture2D() {
        glGenTextures(1, &name);
    }

    // Destructor: Tell OpenGL to free this texture's GPU memory
    ~Texture2D() {
        glDeleteTextures(1, &name);
    }

    // Return the raw OpenGL ID (needed for framebuffers)
    GLuint getOpenGLName() {
        return name;
    }

    // Bind this texture to GL_TEXTURE_2D (make it the "active" texture)
    void bind() const {
        glBindTexture(GL_TEXTURE_2D, name);
    }

    // Unbind: no texture active
    static void unbind(){
        glBindTexture(GL_TEXTURE_2D, 0);
    }
};
```

**Just like buffers and shaders**, textures are identified by integer IDs in OpenGL. The `Texture2D` class is a RAII wrapper that manages the lifetime of that ID.

> **RAII** = Resource Acquisition Is Initialization. The resource (GPU texture) is created in the constructor and freed in the destructor. No manual cleanup needed.

---

## 📖 Understanding `texture-utils.cpp` — Loading Images

### The `loadImage()` Function

This function loads a PNG/JPG/etc. from disk and uploads it to the GPU as a texture.

```cpp
our::Texture2D* our::texture_utils::loadImage(const std::string& filename, bool generate_mipmap) {
    glm::ivec2 size;    // Will hold width and height
    int channels;       // Will hold original number of channels (RGB=3, RGBA=4, etc.)

    // Step 1: Tell stb_image to flip images vertically
    // Why? OpenGL puts (0,0) at the BOTTOM-LEFT, but images put (0,0) at the TOP-LEFT.
    stbi_set_flip_vertically_on_load(true);

    // Step 2: Load the image from disk
    // The '4' at the end means "force RGBA format (4 channels)"
    unsigned char* pixels = stbi_load(filename.c_str(), &size.x, &size.y, &channels, 4);

    if(pixels == nullptr){
        std::cerr << "Failed to load image: " << filename << std::endl;
        return nullptr;
    }

    // Step 3: Create an empty OpenGL texture object
    our::Texture2D* texture = new our::Texture2D();

    // Step 4: Bind the texture (make it the current target)
    texture->bind();

    // Step 5: Upload the pixel data from RAM to GPU
    glTexImage2D(
        GL_TEXTURE_2D,    // Target
        0,                // Mipmap level (0 = base/full resolution)
        GL_RGBA8,         // Internal format: 8 bits per RGBA channel (on GPU)
        size.x, size.y,   // Width and height
        0,                // Border: always 0 (legacy parameter)
        GL_RGBA,          // Format of data we're giving OpenGL
        GL_UNSIGNED_BYTE, // Type of each value (0–255 per channel)
        pixels            // Pointer to the pixel data
    );

    // Step 6: Generate mipmaps (or set filters manually)
    if(generate_mipmap){
        glGenerateMipmap(GL_TEXTURE_2D);
        // glGenerateMipmap automatically sets MIN filter to use mipmaps
    } else {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    // Step 7: Unbind and free CPU-side memory
    our::Texture2D::unbind();
    stbi_image_free(pixels);

    return texture;
}
```

---

### What is `glTexImage2D`?

This is the most important OpenGL call for textures. It uploads pixel data to the GPU.

| Parameter | Meaning |
|---|---|
| `GL_TEXTURE_2D` | We're uploading a 2D texture |
| `0` | Mipmap level 0 = the full-resolution base image |
| `GL_RGBA8` | **Internal format**: how OpenGL stores it on the GPU (RGBA, 8 bits each) |
| `width, height` | Image dimensions |
| `0` | Border width — always 0 |
| `GL_RGBA` | **Pixel format**: what format is the data we're giving it |
| `GL_UNSIGNED_BYTE` | **Pixel type**: each channel is an unsigned byte (0–255) |
| `pixels` | Pointer to the actual pixel data in RAM |

---

### What are Mipmaps?

**Mipmaps** are pre-generated smaller versions of a texture. OpenGL uses them when the object is far away or very small on screen — picking a smaller version avoids visual noise (aliasing).

```
Level 0: 512×512 (full resolution)
Level 1: 256×256
Level 2: 128×128
Level 3: 64×64
...
```

`glGenerateMipmap(GL_TEXTURE_2D)` automatically generates all these levels from level 0.

---

### The `empty()` Function — Creating an Empty Texture

Used in Requirement 11 (Postprocessing) to create a blank texture that framebuffers can render into.

```cpp
our::Texture2D* our::texture_utils::empty(GLenum format, glm::ivec2 size){
    our::Texture2D* texture = new our::Texture2D();
    texture->bind();

    // Pick the right base format and data type based on internal format
    GLenum baseFormat = format;
    GLenum type = GL_UNSIGNED_BYTE;
    if(format == GL_RGBA8){
        baseFormat = GL_RGBA;
    } else if(format == GL_DEPTH_COMPONENT24){
        baseFormat = GL_DEPTH_COMPONENT;
        type = GL_UNSIGNED_INT;  // Depth uses 4-byte values
    }

    // Upload NULL data → creates an empty texture of the given size
    glTexImage2D(GL_TEXTURE_2D, 0, format, size.x, size.y, 0, baseFormat, type, nullptr);

    // Set filtering (no mipmaps since this is a render target)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    our::Texture2D::unbind();
    return texture;
}
```

Key: passing `nullptr` as the pixel data to `glTexImage2D` creates an **empty** texture — GPU memory is allocated but not filled.

---

## 📖 Understanding `texture-test.frag` — Sampling in the Shader

```glsl
#version 330 core

in Varyings {
    vec3 position;
    vec4 color;
    vec2 tex_coord;   // ← This comes from the vertex shader (UV coordinates)
    vec3 normal;
} fs_in;

out vec4 frag_color;

uniform sampler2D tex;  // ← The texture unit bound to this shader

void main(){
    // Sample the texture at the UV coordinate for this pixel
    frag_color = texture(tex, fs_in.tex_coord);
}
```

### What is a `sampler2D`?

A `sampler2D` is a GLSL type representing a 2D texture. You can't bind a texture directly to a sampler — instead, you bind the texture to a **texture unit** (a numbered slot), and tell the sampler which unit to use.

### The `texture()` Function

```glsl
vec4 result = texture(sampler, uv_coords);
```

- `sampler` = the `sampler2D` uniform.
- `uv_coords` = a `vec2` with the (u, v) coordinate to sample at.
- Returns a `vec4` of the color (RGBA) at that position.

---

## 🔢 Texture Units — Binding Textures for Shaders

The GPU has multiple **texture units** (numbered slots, e.g., unit 0, 1, 2...). To use a texture in a shader:

1. **Activate a texture unit:**
```cpp
glActiveTexture(GL_TEXTURE0);  // Select unit 0 (GL_TEXTURE0 + N for unit N)
```

2. **Bind the texture to that unit:**
```cpp
texture->bind();  // Binds to GL_TEXTURE_2D of the currently active unit
```

3. **Tell the shader which unit to use:**
```cpp
shader->set("tex", 0);  // The sampler2D "tex" should read from unit 0
```

---

## ⚠️ Safety Tip: Null Pointers

When using pointers to textures (e.g., `Texture2D* texture`), always remember that they are not automatically null. If you forget to load a texture, it might point to garbage memory.

**Best Practice:** Always check for null before binding in your rendering code:
```cpp
if(texture) texture->bind();
```
This prevents hard-to-debug crashes or "Not Responding" hangs.

---

## 🔄 The Full Texture Pipeline

```
Disk (PNG file)
    ↓  stbi_load()
RAM (unsigned char* pixels)
    ↓  glTexImage2D()
GPU VRAM (OpenGL Texture, ID = "name")
    ↓  glActiveTexture() + bind()
Texture Unit (slot 0, 1, 2...)
    ↓  shader->set("tex", 0)
Shader (sampler2D tex)
    ↓  texture(tex, uv)
Fragment Color
```

---

## 🧮 Texture Filtering (Preview for Req 6)

When a texture is sampled at a non-integer coordinate, OpenGL needs to **interpolate**:
- **GL_NEAREST**: Pick the closest pixel. Fast but blocky.
- **GL_LINEAR**: Average the 4 nearest pixels. Smoother.

These are set via `glTexParameteri` or via a **Sampler object** (see Requirement 6).

---

## ✅ Key Things to Remember

| Concept | Key Function/Enum | Notes |
|---|---|---|
| Create texture | `glGenTextures(1, &name)` | Gets an ID |
| Delete texture | `glDeleteTextures(1, &name)` | Free GPU memory |
| Bind texture | `glBindTexture(GL_TEXTURE_2D, name)` | Make it active |
| Upload data | `glTexImage2D(...)` | Copy RAM → GPU |
| Generate mipmaps | `glGenerateMipmap(GL_TEXTURE_2D)` | Automatic downsampled versions |
| Activate texture unit | `glActiveTexture(GL_TEXTURE0)` | Select slot for binding |
| Load from file | `stbi_load(file, &w, &h, &ch, 4)` | Uses stb_image library |
| Flip for OpenGL | `stbi_set_flip_vertically_on_load(true)` | OpenGL's V origin is bottom |
| Shader sampling | `texture(sampler2D, vec2)` | Returns vec4 |

---

## 🧪 How to Test

```powershell
./bin/GAME_APPLICATION.exe -c='config/texture-test/test-0.jsonc' -f=10
```

Compare output in `screenshots/texture-test/` with `expected/texture-test/`.
