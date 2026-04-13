# Requirement 7: Material — Complete Study Guide

---

## 🧠 What is a Material?

So far we've learned about:
- **ShaderProgram** — the GPU program that determines appearance.
- **PipelineState** — OpenGL settings (depth, blending, culling).
- **Texture2D** — an image stored on the GPU.
- **Sampler** — how to sample a texture.

A **Material** is a single object that **combines all of these together** and answers:
1. **Which shader** draws this object?
2. **Which pipeline state** (blending? depth test?) is needed?
3. **What uniform data** (colors, textures) get sent to the shader?
4. **Is this object transparent?** (affects rendering order)

Think of it like a "paint can" — a material defines everything about how a surface looks and behaves when rendered.

---

## 📁 Relevant Files

| File | Purpose |
|---|---|
| `source/common/material/material.hpp` | Class hierarchy: `Material`, `TintedMaterial`, `TexturedMaterial` |
| `source/common/material/material.cpp` | `setup()` and `deserialize()` implementations |
| `assets/shaders/tinted.vert` & `tinted.frag` | Shaders for the `TintedMaterial` |
| `assets/shaders/textured.vert` & `textured.frag` | Shaders for the `TexturedMaterial` |

---

## 📖 The Class Hierarchy

```
Material           (Base class)
│  - shader
│  - pipelineState
│  - transparent
│
└── TintedMaterial     (adds a tint color uniform)
    │  - tint (vec4)
    │
    └── TexturedMaterial   (adds a texture + sampler + alpha threshold)
           - texture
           - sampler
           - alphaThreshold
```

Each child class **extends** the parent by adding more data and more uniform setup.

---

## 📖 Understanding the Base `Material` Class

### Declaration (`material.hpp`)

```cpp
class Material {
public:
    PipelineState pipelineState;  // Depth, culling, blending settings
    ShaderProgram* shader;        // Which GPU program to use
    bool transparent;             // Should this be rendered with transparency?

    virtual void setup() const;                       // Apply settings before drawing
    virtual void deserialize(const nlohmann::json&);  // Read from JSON
};
```

**Note:** `virtual` means child classes override these functions with more specific behavior. This is **polymorphism** in C++.

### Implementation (`material.cpp`)

```cpp
void Material::setup() const {
    pipelineState.setup();  // Apply depth/culling/blend settings to OpenGL
    shader->use();          // Activate the shader program
}

void Material::deserialize(const nlohmann::json& data){
    if(!data.is_object()) return;

    if(data.contains("pipelineState")){
        pipelineState.deserialize(data["pipelineState"]);  // Load pipeline config
    }
    // Get shader by name from the asset loader
    shader = AssetLoader<ShaderProgram>::get(data["shader"].get<std::string>());
    transparent = data.value("transparent", false);
}
```

**What is `AssetLoader`?**
It's a helper that loads and caches assets (shaders, textures, samplers) by name. When you say `AssetLoader<ShaderProgram>::get("tinted")`, it finds the already-loaded shader named `"tinted"` and returns a pointer to it. This prevents loading the same shader multiple times.

---

## 📖 Understanding `TintedMaterial`

### What it adds

A **tint** is a color (RGBA vec4) that gets multiplied with the object's own vertex color. 

- `tint = (1, 1, 1, 1)` → no change (white tint = identity).
- `tint = (1, 0, 0, 1)` → only red survives → entire object appears red.
- `tint = (1, 1, 1, 0.5)` → 50% transparent (use with blending enabled!).

### Declaration (`material.hpp`)

```cpp
class TintedMaterial : public Material {
public:
    glm::vec4 tint;  // RGBA color multiplier

    void setup() const override;
    void deserialize(const nlohmann::json& data) override;
};
```

### Implementation (`material.cpp`)

```cpp
void TintedMaterial::setup() const {
    Material::setup();           // Call parent's setup (pipeline + shader)
    shader->set("tint", tint);   // Send the tint to the shader as a uniform
}

void TintedMaterial::deserialize(const nlohmann::json& data){
    Material::deserialize(data);  // Read parent's data (shader, pipeline, transparent)
    if(!data.is_object()) return;
    tint = data.value("tint", glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));  // Default: white
}
```

---

### The Tinted Shaders

**`tinted.vert`:**
```glsl
#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;

out Varyings {
    vec4 color;
} vs_out;

uniform mat4 transform;

void main(){
    gl_Position = transform * vec4(position, 1.0);  // Transform vertex
    vs_out.color = color;                           // Pass vertex color through
}
```
Simple: apply transformation, pass vertex color to fragment shader.

**`tinted.frag`:**
```glsl
#version 330 core

in Varyings {
    vec4 color;
} fs_in;

out vec4 frag_color;

uniform vec4 tint;  // Received from TintedMaterial::setup()

void main(){
    frag_color = tint * fs_in.color;  // Multiply tint by vertex color
}
```

**How tint multiplication works (component-wise):**
```
tint  = (1.0, 0.5, 0.0, 1.0)   ← red-orange tint
color = (1.0, 1.0, 1.0, 1.0)   ← white vertex color
result = (1.0×1.0, 0.5×1.0, 0.0×1.0, 1.0×1.0) = (1.0, 0.5, 0.0, 1.0) ← orange
```

---

## 📖 Understanding `TexturedMaterial`

### What it adds

Extends `TintedMaterial` with:
- A **texture** (2D image).
- A **sampler** (how to read the texture).
- An **alphaThreshold** — pixels with alpha below this value are completely discarded (not drawn). Useful for transparent cutouts (e.g., leaves, chain-link fences) without needing blending.

### Declaration (`material.hpp`)

```cpp
class TexturedMaterial : public TintedMaterial {
public:
    Texture2D* texture;
    Sampler* sampler;
    float alphaThreshold;  // Discard pixels with alpha < this value

    void setup() const override;
    void deserialize(const nlohmann::json& data) override;
};
```

### Implementation (`material.cpp`)

```cpp
void TexturedMaterial::setup() const {
    TintedMaterial::setup();                         // Parent: pipeline + shader + tint
    shader->set("alphaThreshold", alphaThreshold);   // Send threshold to shader

    glActiveTexture(GL_TEXTURE0);   // Use texture unit 0
    if(texture) texture->bind();     // [SAFETY] Bind the texture if it exists
    if(sampler) sampler->bind(0);    // [SAFETY] Bind the sampler if it exists
    shader->set("tex", 0);          // Tell the shader: sampler2D "tex" = unit 0
}

void TexturedMaterial::deserialize(const nlohmann::json& data){
    TintedMaterial::deserialize(data);  // Read parent's config
    if(!data.is_object()) return;
    alphaThreshold = data.value("alphaThreshold", 0.0f);
    texture = AssetLoader<Texture2D>::get(data.value("texture", ""));
    sampler = AssetLoader<Sampler>::get(data.value("sampler", ""));
}
```

---

### The Textured Shaders

**`textured.vert`:**
```glsl
#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;
layout(location = 2) in vec2 tex_coord;

out Varyings {
    vec4 color;
    vec2 tex_coord;   // Pass UV coordinate to fragment shader
} vs_out;

uniform mat4 transform;

void main(){
    gl_Position = transform * vec4(position, 1.0);
    vs_out.color = color;
    vs_out.tex_coord = tex_coord;  // Pass UV through
}
```

**`textured.frag`:**
```glsl
#version 330 core

in Varyings {
    vec4 color;
    vec2 tex_coord;
} fs_in;

out vec4 frag_color;

uniform vec4 tint;              // From TintedMaterial
uniform sampler2D tex;          // The texture (from TexturedMaterial)
uniform float alphaThreshold;   // Alpha cutoff (from TexturedMaterial)

void main(){
    vec4 textureColor = texture(tex, fs_in.tex_coord);  // Sample the texture

    if(textureColor.a < alphaThreshold) discard;  // Cutout transparency

    frag_color = tint * fs_in.color * textureColor;  // Combine all three
}
```

**The `discard` keyword:**
When `discard` is called inside a fragment shader, the current pixel is **completely discarded** — it's as if this pixel was never drawn. No color write, no depth write. This creates hard-edged transparency (like a cutout mask).

**The final color formula:**
```
frag_color = tint × vertex_color × texture_color
```

Example:
```
tint          = (1.0, 1.0, 1.0, 1.0)   white (no tint effect)
vertex_color  = (1.0, 1.0, 1.0, 1.0)   white vertex
texture_color = (0.8, 0.6, 0.4, 1.0)   brown texture
result        = (0.8, 0.6, 0.4, 1.0)   ← just the texture color!
```

But if tint = (1, 0, 0, 1):
```
result = (0.8, 0.0, 0.0, 1.0)  ← texture appears fully red
```

---

## 📖 The `createMaterialFromType()` Factory Function

```cpp
inline Material* createMaterialFromType(const std::string& type){
    if(type == "tinted"){
        return new TintedMaterial();     // Create a TintedMaterial
    } else if(type == "textured"){
        return new TexturedMaterial();   // Create a TexturedMaterial
    } else {
        return new Material();           // Create the base Material
    }
}
```

This is a **factory function** — it creates the right type of material based on a string name. This makes it easy to create materials dynamically from config files without hardcoding types.

---

## 🔄 Complete Rendering Flow with Material

```
Before drawing object:

1. material->setup() is called
   ├─ pipelineState.setup()   → OpenGL state configured (depth, blend, etc.)
   ├─ shader->use()            → GPU program activated
   ├─ shader->set("tint", ...) → Tint uniform sent (if TintedMaterial)
   ├─ texture->bind()          → Texture bound to unit 0 (if TexturedMaterial)
   ├─ sampler->bind(0)         → Sampler bound to unit 0 (if TexturedMaterial)
   └─ shader->set("tex", 0)    → Shader knows to read from unit 0

2. shader->set("transform", matrix) → Per-object transform sent

3. mesh->draw()  → GPU draws the triangles using the active shader+state
```

---

## 🗺️ Example Material JSON Configs

**TintedMaterial:**
```json
{
    "type": "tinted",
    "shader": "tinted",
    "transparent": false,
    "tint": [1.0, 0.5, 0.0, 1.0],
    "pipelineState": {
        "depthTesting": { "enabled": true, "function": "GL_LEQUAL" },
        "faceCulling": { "enabled": true }
    }
}
```

**TexturedMaterial:**
```json
{
    "type": "textured",
    "shader": "textured",
    "transparent": true,
    "tint": [1.0, 1.0, 1.0, 0.8],
    "texture": "box",
    "sampler": "linear-mipmap",
    "alphaThreshold": 0.1,
    "pipelineState": {
        "depthTesting": { "enabled": true },
        "blending": {
            "enabled": true,
            "sourceFactor": "GL_SRC_ALPHA",
            "destinationFactor": "GL_ONE_MINUS_SRC_ALPHA"
        }
    }
}
```

---

## ⚠️ PRO-TIP: Safety & Troubleshooting

### 1. The "Null Pointer" Nightmare
In C++, pointers like `Texture2D* texture` or `Sampler* sampler` are **not** automatically set to `nullptr` when created. They contain garbage memory. 
- **The Bug:** If you create a `TexturedMaterial` and forget to assign a texture/sampler, calling `texture->bind()` will try to access that garbage memory.
- **The Symptom:** The application might crash immediately, or worse, it might run for a few seconds and then hang ("Not Responding") once the garbage address finally points to something illegal.
- **The Fix:** ALWAYS use null checks in `setup()` as shown above: `if(texture) texture->bind();`.

### 2. Synchronous Debugging & "Not Responding"
You might see `glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS)` in `application.cpp`. 
- **What it does:** It makes OpenGL wait for your debug callback to finish before it continues.
- **The Danger:** If your driver emits many notifications (like "Buffer will use VIDEO memory"), and your code prints them to `std::cout`, the application will spend all its time doing console I/O.
- **The Result:** The main loop slows to a crawl, and Windows will mark the window as "Not Responding". If you see this, try disabling synchronous debug output or filtering out "NOTIFICATION" level messages.

---

## ✅ Key Things to Remember

| Concept | Details |
|---|---|
| `Material` | Base class: holds shader + pipelineState + transparent flag |
| `TintedMaterial` | Adds a `vec4 tint` → multiplied with vertex color in shader |
| `TexturedMaterial` | Adds texture + sampler + alphaThreshold → texture is sampled and multiplied |
| `virtual setup()` | Called before drawing; applies all the material's settings to OpenGL |
| `Safety Checks` | **Crucial!** Always check if `texture` or `sampler` are null in `setup()` |
| `discard` | Fragment shader keyword — completely skips the current pixel |
| `alphaThreshold` | Pixels with `textureColor.a < threshold` are discarded |
| `frag_color = tint * vertex_color * texture_color` | The final color formula |
| `AssetLoader<T>::get(name)` | Gets a pre-loaded asset by its string name |
| `createMaterialFromType(str)` | Factory function — creates the right material subclass |

---

## 🧪 How to Test

```powershell
./bin/GAME_APPLICATION.exe -c='config/material-test/test-1.jsonc' -f=10
```

Compare output in `screenshots/material-test/` with `expected/material-test/`.
