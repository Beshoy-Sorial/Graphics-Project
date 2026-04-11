# Requirement 6: Sampler — Complete Study Guide

---

## 🧠 What is a Sampler?

In Requirement 5, you learned how to create and upload textures. But when a shader **samples** (reads) a texture, it needs to know **how** to do the sampling:

- If the texture coordinate falls **between pixels**, which pixel(s) should it use?
- If the texture coordinate goes **outside the [0,1] range**, what happens?

These questions are answered by a **Sampler** — an OpenGL object that stores texture sampling configuration, separate from the texture itself.

**Why have samplers separate from textures?**
Because you might want to display the *same texture* in two different ways — one with linear filtering and one with nearest filtering. Instead of having two copies of the texture, you have one texture and two samplers!

---

## 📁 Relevant Files

| File | Purpose |
|---|---|
| `source/common/texture/sampler.hpp` | The `Sampler` class declaration |
| `source/common/texture/sampler.cpp` | The `deserialize()` function implementation |

---

## 📖 Understanding `sampler.hpp` — The Class

```cpp
class Sampler {
    GLuint name;   // OpenGL ID for this sampler object (just an integer)
public:
    // Create a sampler object, get its ID from OpenGL
    Sampler() {
        glGenSamplers(1, &name);
    }

    // Delete the sampler from GPU
    ~Sampler() {
        glDeleteSamplers(1, &name);
    }

    // Bind this sampler to a specific texture unit
    void bind(GLuint textureUnit) const {
        glBindSampler(textureUnit, name);
    }

    // Unbind: remove sampler from a texture unit
    static void unbind(GLuint textureUnit){
        glBindSampler(textureUnit, 0);
    }

    // Set an integer sampler parameter (min filter, mag filter, wrap mode)
    void set(GLenum parameter, GLint value) const {
        glSamplerParameteri(name, parameter, value);
    }

    // Set a float sampler parameter (anisotropy level)
    void set(GLenum parameter, GLfloat value) const {
        glSamplerParameterf(name, parameter, value);
    }

    // Set a vec4 sampler parameter (border color)
    void set(GLenum parameter, glm::vec4 value) const {
        glSamplerParameterfv(name, parameter, &(value.r));
    }

    void deserialize(const nlohmann::json& data);
};
```

The class is very simple — it's essentially just a wrapper around `glSamplerParameter*` calls.

**How to use it:**

```cpp
// To use texture unit 0 with this sampler:
glActiveTexture(GL_TEXTURE0);   // Activate unit 0
texture->bind();                // Bind texture to unit 0
sampler->bind(0);               // Bind sampler to unit 0 (overrides texture's own params)
shader->set("tex", 0);          // Tell shader to use unit 0
```

When a sampler is bound to a texture unit, it **overrides** any filtering/wrapping settings stored directly in the texture object.

---

## 📖 Understanding `sampler.cpp` — The `deserialize()` Function

```cpp
void Sampler::deserialize(const nlohmann::json& data){
    if(!data.is_object()) return;

    // Magnification filter: when texture is ZOOMED IN (pixel covers < 1 texel)
    set(GL_TEXTURE_MAG_FILTER, (GLint)gl_enum_deserialize::texture_magnification_filters
        .at(data.value("MAG_FILTER", "GL_LINEAR")));

    // Minification filter: when texture is SHRUNK (pixel covers > 1 texel)
    set(GL_TEXTURE_MIN_FILTER, (GLint)gl_enum_deserialize::texture_minification_filters
        .at(data.value("MIN_FILTER", "GL_LINEAR_MIPMAP_LINEAR")));

    // Wrapping on the S (horizontal, u) axis
    set(GL_TEXTURE_WRAP_S, (GLint)gl_enum_deserialize::texture_wrapping_modes
        .at(data.value("WRAP_S", "GL_REPEAT")));

    // Wrapping on the T (vertical, v) axis
    set(GL_TEXTURE_WRAP_T, (GLint)gl_enum_deserialize::texture_wrapping_modes
        .at(data.value("WRAP_T", "GL_REPEAT")));

    // Anisotropic filtering level (1.0 = no anisotropy, 16.0 = max)
    set(GL_TEXTURE_MAX_ANISOTROPY_EXT, data.value("MAX_ANISOTROPY", 1.0f));

    // Border color (used when WRAP = GL_CLAMP_TO_BORDER)
    set(GL_TEXTURE_BORDER_COLOR, data.value("BORDER_COLOR", glm::vec4(0, 0, 0, 0)));
}
```

---

## 🔵 Concept 1: Texture Filtering

Filtering answers: *"This pixel covers part of the texture — what color should I assign?"*

### Magnification (`GL_TEXTURE_MAG_FILTER`)

**Happens when:** The texture is zoomed in — one texel (texture pixel) covers multiple screen pixels.

| Filter | Result |
|---|---|
| `GL_NEAREST` | Pick the closest texel. Fast, blocky/pixelated look |
| `GL_LINEAR` | Bilinear interpolation — average the 4 nearest texels. Smooth |

```
Zoomed in with GL_NEAREST:   Zoomed in with GL_LINEAR:
┌──┬──┬──┐                   ╔══╦══╦══╗
│██│██│░░│                   ║██║▓▓║░░║
├──┼──┼──┤     →             ╠══╬══╬══╣
│██│░░│░░│                   ║▓▓║░░░░░║
└──┴──┴──┘                   ╚══╩══╩══╝
(Sharp edges, pixels visible)  (Smooth blending)
```

### Minification (`GL_TEXTURE_MIN_FILTER`)

**Happens when:** The texture is shrunk — many texels cover one screen pixel. This is where aliasing (visual noise) can occur.

| Filter | Result |
|---|---|
| `GL_NEAREST` | Pick the closest texel — very noisy at distance |
| `GL_LINEAR` | Bilinear — slightly smoother |
| `GL_NEAREST_MIPMAP_NEAREST` | Use the nearest mipmap level, nearest sampling |
| `GL_LINEAR_MIPMAP_NEAREST` | Use the nearest mipmap level, linear sampling |
| `GL_NEAREST_MIPMAP_LINEAR` | Blend between 2 mipmap levels, nearest sampling |
| `GL_LINEAR_MIPMAP_LINEAR` | **Trilinear filtering** — blend between 2 mipmap levels, linear sampling. Best quality |

> ⚠️ **Important:** Mipmap-based filters (`*_MIPMAP_*`) only work if the texture has mipmaps. If you used `generate_mipmap=false` in `loadImage`, don't use these filters!

---

## 🔵 Concept 2: Texture Wrapping

Wrapping answers: *"What happens when the UV coordinate goes outside [0, 1]?"*

For example, UV = (1.5, 0.5) is outside the texture. What color do we return?

| Mode | Enum | Behavior | Use Case |
|---|---|---|---|
| **Repeat** | `GL_REPEAT` | Tile the texture | Floors, walls |
| **Mirrored Repeat** | `GL_MIRRORED_REPEAT` | Tile but flip alternately | Seamless patterns |
| **Clamp to Edge** | `GL_CLAMP_TO_EDGE` | Stretch the edge pixels | Sprites with transparency |
| **Clamp to Border** | `GL_CLAMP_TO_BORDER` | Use a specified border color | Projective textures |

**Visual Examples:**

```
UV = 0.0 to 2.5

GL_REPEAT:              [img][img][img]
GL_MIRRORED_REPEAT:     [img][rev][img]
GL_CLAMP_TO_EDGE:       [img][===] (last column repeated)
GL_CLAMP_TO_BORDER:     [img][---] (border color: e.g. black)
```

Wrapping is set separately for S (horizontal) and T (vertical) axes:
- `GL_TEXTURE_WRAP_S` = horizontal (U direction)
- `GL_TEXTURE_WRAP_T` = vertical (V direction)

---

## 🔵 Concept 3: Anisotropic Filtering

**What is it?**

When you look at a textured surface at a steep angle (e.g., a floor), standard mipmaps look very blurry. Anisotropic filtering fixes this by sampling more texels along the direction of greatest stretching.

```cpp
set(GL_TEXTURE_MAX_ANISOTROPY_EXT, anisotropy_level);
// 1.0  = disabled (standard filtering)
// 2.0  = 2× anisotropy
// 4.0  = 4× anisotropy
// 16.0 = maximum (best quality, most GPU work)
```

This is an **extension** (`EXT`) not part of core OpenGL, but universally supported on modern hardware.

---

## 🔵 Concept 4: Border Color

Only used when wrap mode is `GL_CLAMP_TO_BORDER`. Pixels outside [0,1] UV range get this color.

```cpp
set(GL_TEXTURE_BORDER_COLOR, glm::vec4(0, 0, 0, 0)); // Black transparent border
```

---

## 🔄 The Sampler vs. Direct Texture Parameters

OpenGL allows you to set filtering and wrapping **directly on the texture** with `glTexParameteri`. But if a **sampler** is bound to the same texture unit, the sampler's settings **win** and override the texture's own settings.

```
No sampler bound: texture's own glTexParameteri settings are used.
Sampler bound:    sampler's glSamplerParameteri settings override everything.
```

This is why we have samplers — they can be created separately and swapped without touching the texture.

---

## 🗺️ Example JSON Config

```json
{
    "MAG_FILTER": "GL_LINEAR",
    "MIN_FILTER": "GL_LINEAR_MIPMAP_LINEAR",
    "WRAP_S": "GL_REPEAT",
    "WRAP_T": "GL_REPEAT",
    "MAX_ANISOTROPY": 4.0,
    "BORDER_COLOR": [0, 0, 0, 0]
}
```

This creates a sampler that:
- Uses smooth linear interpolation when zooming in.
- Uses trilinear filtering (best quality) when zooming out.
- Tiles the texture when UV coordinates go outside [0,1].
- Uses 4× anisotropic filtering for angled surfaces.

---

## 🔄 Complete Usage Flow

```cpp
// Setup:
auto texture = texture_utils::loadImage("assets/brick.png", true);
auto sampler = new Sampler();
sampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
sampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
sampler->set(GL_TEXTURE_WRAP_S, GL_REPEAT);
sampler->set(GL_TEXTURE_WRAP_T, GL_REPEAT);

// Every frame, before drawing:
glActiveTexture(GL_TEXTURE0);   // Use texture unit 0
texture->bind();                 // Bind texture to unit 0
sampler->bind(0);                // Bind sampler to unit 0
shader->set("tex", 0);          // Tell the sampler2D uniform to read from unit 0

// The texture(sampler, uv) call in the shader now uses the sampler's settings!
```

---

## ✅ Key Things to Remember

| Concept | Key Function | Common Options |
|---|---|---|
| Create sampler | `glGenSamplers(1, &name)` | — |
| Bind to unit | `glBindSampler(unit, name)` | — |
| Set integer param | `glSamplerParameteri(name, param, value)` | Filter, wrap enums |
| Set float param | `glSamplerParameterf(name, param, value)` | Anisotropy |
| Set vec4 param | `glSamplerParameterfv(name, param, ptr)` | Border color |
| Magnification filter | `GL_TEXTURE_MAG_FILTER` | `GL_LINEAR`, `GL_NEAREST` |
| Minification filter | `GL_TEXTURE_MIN_FILTER` | `GL_LINEAR_MIPMAP_LINEAR` (trilinear) |
| Wrapping | `GL_TEXTURE_WRAP_S/T` | `GL_REPEAT`, `GL_CLAMP_TO_EDGE` |
| Anisotropy | `GL_TEXTURE_MAX_ANISOTROPY_EXT` | 1.0 – 16.0 |
| Border color | `GL_TEXTURE_BORDER_COLOR` | `glm::vec4(r,g,b,a)` |

---

## 🧪 How to Test

```powershell
./bin/GAME_APPLICATION.exe -c='config/sampler-test/test-0.jsonc' -f=10
```

Compare output in `screenshots/sampler-test/` with `expected/sampler-test/`.
