# Requirement 11: Postprocessing — Complete Study Guide

---

## 🧠 What is Postprocessing?

**Postprocessing** means applying an image-space effect to the completed rendered scene **as a whole**, rather than per-object.

Think of it like Instagram filters — the scene is rendered first into an image, and then that image is passed through a filter shader before being shown on screen.

**Examples:**
- **Vignette** — darken the corners (draws the eye to the center)
- **Chromatic Aberration** — simulate camera lens distortion (RGB channels shift slightly)
- **Grayscale** — desaturate colors
- **Radial Blur** — motion blur from center outward

---

## 🏗️ How Postprocessing Works

Without postprocessing:
```
[Render Scene] → [Default Framebuffer] → Screen
```

With postprocessing:
```
[Render Scene] → [Custom Framebuffer (texture)] → [Postprocess Shader] → [Default Framebuffer] → Screen
```

**Step 1:** Render the entire scene into a **custom framebuffer** that has a texture as its color attachment. The scene pixels are now stored in `colorTarget` texture.

**Step 2:** Bind the default framebuffer (the screen). Draw a fullscreen "triangle" using the scene texture and a postprocessing fragment shader that modifies the image.

---

## 📁 Relevant Files

| File | Purpose |
|---|---|
| `source/common/systems/forward-renderer.cpp` | Framebuffer setup in `initialize()`, apply in `render()` |
| `source/common/texture/texture-utils.cpp` | The `empty()` function — creates textures for framebuffer |
| `assets/shaders/fullscreen.vert` | Fullscreen triangle vertex shader |
| `assets/shaders/postprocess/vignette.frag` | Vignette effect shader |
| `assets/shaders/postprocess/chromatic-aberration.frag` | Chromatic aberration effect shader |

---

## 📖 The Framebuffer — OpenGL's Off-Screen Render Target

### What is a Framebuffer?

A **framebuffer** is an object that holds **render targets** — textures that OpenGL can draw into. Instead of drawing to the screen, you draw to these textures.

A framebuffer has **attachments**:
- **Color attachment** — where color pixels are written (usually a `GL_RGBA8` texture)
- **Depth attachment** — where depth values are written (a `GL_DEPTH_COMPONENT24` texture)

If there's no depth attachment, depth testing won't work!

---

## 📖 Framebuffer Setup in `initialize()`

```cpp
if(config.contains("postprocess")){

    // 1. Create the framebuffer object
    glGenFramebuffers(1, &postprocessFrameBuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, postprocessFrameBuffer);

    // 2. Create color target texture (RGBA, 8 bits per channel, same size as window)
    colorTarget = texture_utils::empty(GL_RGBA8, windowSize);

    // 3. Create depth target texture (24-bit depth)
    depthTarget = texture_utils::empty(GL_DEPTH_COMPONENT24, windowSize);

    // 4. Attach both textures to the framebuffer
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,         // The framebuffer we're configuring
        GL_COLOR_ATTACHMENT0,   // Attach as color buffer 0
        GL_TEXTURE_2D,          // The texture type
        colorTarget->getOpenGLName(), // The texture's OpenGL ID
        0                       // Mipmap level (0 = base)
    );
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT,    // Attach as depth buffer
        GL_TEXTURE_2D,
        depthTarget->getOpenGLName(),
        0
    );

    // 5. Unbind the framebuffer (return to default)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 6. Create an empty VAO for fullscreen drawing
    glGenVertexArrays(1, &postProcessVertexArray);

    // 7. Create the postprocess material
    Sampler* postprocessSampler = new Sampler();
    postprocessSampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    postprocessSampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    postprocessSampler->set(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    postprocessSampler->set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    ShaderProgram* postprocessShader = new ShaderProgram();
    postprocessShader->attach("assets/shaders/fullscreen.vert", GL_VERTEX_SHADER);
    postprocessShader->attach(config.value<std::string>("postprocess", ""), GL_FRAGMENT_SHADER);
    postprocessShader->link();

    postprocessMaterial = new TexturedMaterial();
    postprocessMaterial->shader = postprocessShader;
    postprocessMaterial->texture = colorTarget;      // ← the scene's color texture!
    postprocessMaterial->sampler = postprocessSampler;
    postprocessMaterial->tint = glm::vec4(1.0f);
    postprocessMaterial->alphaThreshold = 0.0f;      // 0 = never discard
    postprocessMaterial->transparent = false;
    postprocessMaterial->pipelineState.depthMask = false;  // Don't write depth
}
```

---

### What Does `texture_utils::empty()` Do Again?

It creates an OpenGL texture with GPU memory allocated but no data filled in:

```cpp
our::Texture2D* our::texture_utils::empty(GLenum format, glm::ivec2 size){
    our::Texture2D* texture = new our::Texture2D();
    texture->bind();

    GLenum baseFormat = format;
    GLenum type = GL_UNSIGNED_BYTE;
    if(format == GL_RGBA8){
        baseFormat = GL_RGBA;
    } else if(format == GL_DEPTH_COMPONENT24){
        baseFormat = GL_DEPTH_COMPONENT;
        type = GL_UNSIGNED_INT;
    }

    // Upload NULL → just allocate memory, no pixel data
    glTexImage2D(GL_TEXTURE_2D, 0, format, size.x, size.y, 0, baseFormat, type, nullptr);

    // No mipmaps needed (it's a render target)
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    our::Texture2D::unbind();
    return texture;
}
```

The key is `nullptr` as the pixel data — we're just reserving space. OpenGL will fill it when we render into the framebuffer.

---

## 📖 Applying Postprocessing in `render()`

### At the Start of Rendering: Bind the Custom Framebuffer

```cpp
if(postprocessMaterial){
    glBindFramebuffer(GL_FRAMEBUFFER, postprocessFrameBuffer);
    // ↑ Now all rendering goes into colorTarget and depthTarget textures!
}
```

Everything after this (clearing, opaque objects, sky, transparent objects) is rendered into the framebuffer texture — not the screen.

---

### At the End of Rendering: Apply the Effect

```cpp
if(postprocessMaterial){
    // 1. Return to the default framebuffer (the screen!)
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // 2. Set up the postprocess material (binds the scene texture as input)
    postprocessMaterial->setup();

    // 3. Bind the empty VAO (no vertex data needed — see below why)
    glBindVertexArray(postProcessVertexArray);

    // 4. Draw 3 vertices (making a fullscreen triangle)
    glDrawArrays(GL_TRIANGLES, 0, 3);
}
```

The postprocess effect shader reads from `colorTarget` (the rendered scene) and outputs the modified image to the screen.

---

## 📖 The Fullscreen Triangle Trick

### Why draw a triangle instead of a quad?

You might think: "I want to draw a full-screen rectangle — use 2 triangles (a quad)." But there's a smarter way.

```
Traditional quad (2 triangles):     Fullscreen triangle trick:
┌─────────────┐                     
│ \           │                        2 (−1, 3)
│   \         │                        |\
│     \       │          (−1,1)────────(1,1)
│       \     │           |   screen   |
│     ____\   │          (−1,−1)──────(1,−1)
│    |    |   │                       |
│    |    |   │                       |
└─────────────┘              0 (−1,−1)──3 (3,−1)
  Uses 6 vertices             Uses 3 vertices, 1 triangle
```

The fullscreen triangle has vertices:
```
(-1, -1)  (3, -1)  (-1, 3)
```

This single huge triangle covers the entire `[-1,1] × [-1,1]` NDC square (the screen rectangle). The parts outside the screen are clipped by OpenGL.

**Benefit:** Drawing 1 triangle is faster than 2 because of how GPU rasterization works — with 2 triangles, the diagonal edge causes some pixels to be processed twice.

---

## 📖 The Fullscreen Vertex Shader (`fullscreen.vert`)

```glsl
#version 330

out vec2 tex_coord;  // UV coordinate passed to the fragment shader

void main(){
    // Hardcoded positions of the fullscreen triangle
    vec2 positions[] = vec2[](
        vec2(-1.0, -1.0),   // Bottom-left
        vec2( 3.0, -1.0),   // Far right (outside screen)
        vec2(-1.0,  3.0)    // Far top (outside screen)
    );

    // UV coordinates: map screen area to [0,1]×[0,1]
    vec2 tex_coords[] = vec2[](
        vec2(0.0, 0.0),   // Bottom-left of texture
        vec2(2.0, 0.0),   // Far right (won't be used, clipped)
        vec2(0.0, 2.0)    // Far top (won't be used, clipped)
    );

    gl_Position = vec4(positions[gl_VertexID], 0.0, 1.0);
    tex_coord = tex_coords[gl_VertexID];
}
```

No VBO, no uniforms, no `in` attributes — all data is **hardcoded inside the shader**. The VAO is empty (`postProcessVertexArray`) — it exists just to satisfy OpenGL's requirement of having a bound VAO. The shader uses `gl_VertexID` (0, 1, 2) to pick positions.

UV coordinates are set up so that at the bottom-left corner of the screen (`NDC = (-1,-1)`), `tex_coord = (0,0)`, and at the top-right (`NDC = (1,1)`), `tex_coord = (1,1)`.

---

## 📖 The Postprocessing Fragment Shaders

### `vignette.frag` — Darkening the Corners

```glsl
#version 330

uniform sampler2D tex;   // The rendered scene texture
in vec2 tex_coord;       // UV from fullscreen.vert

out vec4 frag_color;

void main(){
    // Convert UV [0,1] to NDC [-1,1]
    vec2 ndc = tex_coord * 2.0 - 1.0;

    // Sample the scene
    frag_color = texture(tex, tex_coord);

    // Darken: divide by (1 + distance²)
    // At center: ndc=(0,0) → dot=0 → divide by 1 → no darkening
    // At corner: ndc=(1,1) → dot=2 → divide by 3 → 33% brightness
    frag_color.rgb /= 1.0 + dot(ndc, ndc);
}
```

**How the math works:**
- `dot(ndc, ndc)` = `ndc.x² + ndc.y²` = squared distance from center.
- At center: `dot = 0`, divide by `1` → full brightness.
- At corners: `dot = 2`, divide by `3` → 33% brightness.
- Smoothly darkens toward the edges.

### `chromatic-aberration.frag` — RGB Color Fringing

```glsl
#version 330

uniform sampler2D tex;
in vec2 tex_coord;
out vec4 frag_color;

// How many texture units to shift red vs blue channels
#define STRENGTH 0.005

void main(){
    // Sample the full color at the center position
    vec4 center = texture(tex, tex_coord);

    // Sample red slightly to the LEFT
    float red  = texture(tex, tex_coord + vec2(-STRENGTH, 0.0)).r;

    // Sample blue slightly to the RIGHT
    float blue = texture(tex, tex_coord + vec2( STRENGTH, 0.0)).b;

    // Combine: red from left, green from center, blue from right
    frag_color = vec4(red, center.g, blue, center.a);
}
```

**What this simulates:** Cheap camera lens dispersion — different colors focus at slightly different positions, causing color fringing on edges.

- Red channel is shifted left by `STRENGTH` UV units.
- Blue channel is shifted right by `STRENGTH` UV units.
- Green channel stays centered.

On sharp color edges (e.g., the edge of a bright object), this creates a visible color halo — red on one side, blue on the other.

---

## 🔄 The Complete Postprocessing Flow

```
Frame begins:
  ╔═══════════════════════════════╗
  ║ glBindFramebuffer(custom FBO) ║  ← all rendering goes to texture
  ╠═══════════════════════════════╣
  ║ glClear(COLOR | DEPTH)        ║  ← clear the texture
  ║ Draw opaque objects           ║  → pixels written to colorTarget
  ║ Draw sky                      ║  → pixels written to colorTarget
  ║ Draw transparent objects      ║  → pixels written to colorTarget
  ╚═══════════════════════════════╝
  
  ╔═══════════════════════════════╗
  ║ glBindFramebuffer(0)          ║  ← now render to screen
  ╠═══════════════════════════════╣
  ║ postprocessMaterial->setup()  ║  binds colorTarget as texture
  ║ glBindVertexArray(emptyVAO)   ║
  ║ glDrawArrays(GL_TRIANGLES, 3) ║  fullscreen triangle
  ║  ↓                            ║
  ║ Fragment shader runs:         ║
  ║   texture(tex, tex_coord)     ║  ← reads colorTarget
  ║   → applies vignette          ║  ← modifies colors
  ║   → writes to screen          ║
  ╚═══════════════════════════════╝
Frame ends → user sees the postprocessed image!
```

---

## 🔑 Important Details

### Why `depthMask = false` for postprocess material?
```cpp
postprocessMaterial->pipelineState.depthMask = false;
```
The postprocess pass is just a 2D image operation. We don't want to write depth values because it would corrupt the depth buffer. Setting `depthMask = false` prevents this.

### Why `alphaThreshold = 0.0f` for postprocess?
With `alphaThreshold = 0.0f`: `if(alpha < 0.0f) discard` — this is never true (alpha is always ≥ 0), so no pixels are ever discarded. We want to show the full rendered image.

### Why `GL_CLAMP_TO_EDGE` for postprocess sampler?
The fullscreen triangle's UV coordinates go from exactly (0,0) to (1,1). `GL_CLAMP_TO_EDGE` ensures no border artifacts at the exact edges of the texture (UV = 0.0 or 1.0 might round to neighboring texels with REPEAT).

---

## ✅ Key Things to Remember

| Concept | Details |
|---|---|
| Framebuffer | Off-screen render target. Attach color + depth textures. |
| `glGenFramebuffers(1, &fbo)` | Create a framebuffer |
| `glBindFramebuffer(GL_FRAMEBUFFER, fbo)` | Make it active (rendering goes here) |
| `glBindFramebuffer(GL_FRAMEBUFFER, 0)` | Return to the screen (default framebuffer) |
| `glFramebufferTexture2D(...)` | Attach a texture to the framebuffer |
| `texture_utils::empty(format, size)` | Create an empty texture for rendering into |
| The fullscreen triangle | 3 vertices, 1 triangle, covers entire screen |
| `gl_VertexID` | Used in fullscreen.vert to pick hardcoded positions |
| The postprocess pass | Reads scene from `colorTarget`, draws with effect to screen |
| Vignette | `frag_color.rgb /= 1.0 + dot(ndc, ndc)` — darkens corners |
| Chromatic aberration | Samples R, G, B at slightly different UV offsets |
| `depthMask = false` | Don't write depth during the postprocess quad draw |
| `alphaThreshold = 0.0f` | Never discard pixels in the postprocess pass |

---

## 🧪 How to Test

```powershell
./bin/GAME_APPLICATION.exe -c='config/postprocess-test/test-1.jsonc' -f=10
```

Compare output in `screenshots/postprocess-test/` with `expected/postprocess-test/`.
