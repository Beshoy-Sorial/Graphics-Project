# Requirement 10: Sky Sphere Rendering — Complete Study Guide

---

## 🧠 What is Sky Rendering?

A solid black background looks extremely boring and unrealistic. A **sky** gives a scene depth, atmosphere, and context.

In this engine, the sky is rendered as a **sphere** centered on the camera. As the camera moves, the sphere moves with it — so you can never "reach" the sky or see its edges. A texture mapped onto the inside of the sphere creates the illusion of a distant environment (clouds, stars, mountains, etc.).

This is sometimes called a **sky sphere** or **sky dome**.

---

## 📁 Relevant Files

| File | Purpose |
|---|---|
| `source/common/systems/forward-renderer.hpp` | `skySphere`, `skyMaterial` member variables |
| `source/common/systems/forward-renderer.cpp` | Sky setup in `initialize()`, sky drawing in `render()` |

---

## 🔑 The Three Key Challenges

### Challenge 1: The Sky Should Always Follow the Camera

If the sky sphere is centered at the world origin and you move, you walk away from it and see the sphere. Instead, we want the sphere to always be centered at the camera position.

**Solution:** The sky sphere's model matrix translates it to the camera's current position every frame.

```cpp
glm::vec3 cameraPosition = glm::vec3(cameraLocalToWorld * glm::vec4(0, 0, 0, 1));
glm::mat4 model = glm::translate(glm::mat4(1.0f), cameraPosition);
```

Each frame, we translate the sphere to where the camera is. The camera is always inside the sphere.

---

### Challenge 2: The Sky Must Be Behind Everything Else

What if a wall is in front of the sky? Depth testing must ensure the wall wins. But the sky sphere is a real mesh with real depth values — if its depth value ends up in between objects, it could incorrectly occlude some of them.

**The trick:** Force every pixel of the sky to have `z = 1.0` in NDC (Normalized Device Coordinates) — the maximum depth. This way it is always "behind" everything.

How? Normally after the vertex shader:
- `gl_Position = vec4(x, y, z, w)` → after perspective divide → `NDC.z = z/w`
- We want `NDC.z = 1.0` for every sky pixel → we want `z/w = 1.0` → we want `z = w`.

**Solution:** After computing the full transform `VP × model × localPos`, we modify the result so that `z = w`:

```cpp
glm::mat4 alwaysBehindTransform = glm::mat4(
    1.0f, 0.0f, 0.0f, 0.0f,   // column 0
    0.0f, 1.0f, 0.0f, 0.0f,   // column 1
    0.0f, 0.0f, 0.0f, 0.0f,   // column 2: z output = 0
    0.0f, 0.0f, 1.0f, 1.0f    // column 3: z += w → z = w
);
```

When this matrix is applied **after** the projection matrix:
- It takes the `w` component and copies it into `z`.
- Result: `gl_Position.z = gl_Position.w` → `NDC.z = z/w = w/w = 1.0` ✓

```cpp
skyMaterial->shader->set("transform", alwaysBehindTransform * VP * model);
```

Now every vertex of the sky sphere, regardless of its position, maps to depth 1.0 in NDC.

---

### Challenge 3: The Sky Uses The Depth Function `GL_LEQUAL`

Because the sky is drawn after opaque objects, any pixel that already has an opaque object drawn will have a depth value < 1.0. The sky's depth is exactly 1.0.

- Depth test function: `GL_LEQUAL` → pass if `newDepth ≤ storedDepth`.
- Empty pixels have depth 1.0 (from `glClearDepth(1.0f)`). Sky depth = 1.0. → `1.0 ≤ 1.0` = passes ✓
- Pixels with opaque objects have depth < 1.0. Sky depth = 1.0. → `1.0 ≤ 0.7` = fails → Sky is not drawn over opaque objects ✓

This is also why we use `GL_LEQUAL` instead of `GL_LESS` — the sky's depth equals the cleared depth, not strictly less.

---

### Challenge 4: We Are Viewing the Sphere from the Inside

Normally face culling discards **back faces** (`GL_BACK`). For a sphere, the back faces are the **outside** — but we are viewing from the **inside**, where the "back faces" are actually the visible ones!

**Solution:** Cull `GL_FRONT` instead of `GL_BACK`:

```cpp
PipelineState skyPipelineState{};
skyPipelineState.faceCulling.enabled = true;
skyPipelineState.faceCulling.culledFace = GL_FRONT;  // Cull front faces (outside of sphere)
```

This discards the outward-facing triangles and keeps the inward-facing ones — which is what we see from inside the sphere.

---

## 📖 Sky Setup Code in `initialize()`

```cpp
if(config.contains("sky")){
    // Create a sphere mesh (16 × 16 subdivisions)
    this->skySphere = mesh_utils::sphere(glm::ivec2(16, 16));

    // Create a shader (same as textured objects work!)
    ShaderProgram* skyShader = new ShaderProgram();
    skyShader->attach("assets/shaders/textured.vert", GL_VERTEX_SHADER);
    skyShader->attach("assets/shaders/textured.frag", GL_FRAGMENT_SHADER);
    skyShader->link();

    // Configure pipeline:
    PipelineState skyPipelineState{};
    skyPipelineState.depthTesting.enabled = true;
    skyPipelineState.depthTesting.function = GL_LEQUAL;  // ≤ for sky at z=1.0
    skyPipelineState.faceCulling.enabled = true;
    skyPipelineState.faceCulling.culledFace = GL_FRONT;  // We're inside!

    // Load the sky texture (no mipmaps — avoids blurring at seams)
    std::string skyTextureFile = config.value<std::string>("sky", "");
    Texture2D* skyTexture = texture_utils::loadImage(skyTextureFile, false);

    // Create a sampler
    Sampler* skySampler = new Sampler();
    skySampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    skySampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    skySampler->set(GL_TEXTURE_WRAP_S, GL_REPEAT);         // Horizontal: tile
    skySampler->set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);  // Vertical: clamp (avoid seam at poles)

    // Combine into a TexturedMaterial
    this->skyMaterial = new TexturedMaterial();
    this->skyMaterial->shader = skyShader;
    this->skyMaterial->texture = skyTexture;
    this->skyMaterial->sampler = skySampler;
    this->skyMaterial->pipelineState = skyPipelineState;
    this->skyMaterial->tint = glm::vec4(1.0f);     // No tint
    this->skyMaterial->alphaThreshold = 1.0f;      // Never discard (fully opaque sky)
    this->skyMaterial->transparent = false;
}
```

Key decisions:
- `loadImage(file, false)` — no mipmaps. Mipmaps on the sky texture can cause visible blurring at extreme viewing angles without benefit.
- `GL_CLAMP_TO_EDGE` for T (vertical wrap) — prevents a seam artifact at the top and bottom poles of the sphere.
- `alphaThreshold = 1.0f` — since alphaThreshold means "discard if alpha < threshold", using 1.0f would normally discard everything! Wait — looking at the textured.frag: `if(textureColor.a < alphaThreshold) discard;`. With `alphaThreshold = 1.0f` and a fully opaque texture (alpha = 1.0), the condition is `1.0 < 1.0` which is **false** → nothing is discarded. ✓

---

## 📖 Sky Drawing Code in `render()`

```cpp
if(this->skyMaterial){
    // 1. Activate sky material (sets pipeline, shader, texture, sampler)
    skyMaterial->setup();

    // 2. Move the sky sphere to the camera's current position
    glm::mat4 model = glm::translate(glm::mat4(1.0f), cameraPosition);

    // 3. The magic matrix that forces NDC.z = 1.0 for all sky pixels
    glm::mat4 alwaysBehindTransform = glm::mat4(
        1.0f, 0.0f, 0.0f, 0.0f,
        0.0f, 1.0f, 0.0f, 0.0f,
        0.0f, 0.0f, 0.0f, 0.0f,   // z output = 0
        0.0f, 0.0f, 1.0f, 1.0f    // z += w, w unchanged → NDC.z = 1
    );

    // 4. Combine: alwaysBehindTransform × Projection × View × Model
    skyMaterial->shader->set("transform", alwaysBehindTransform * VP * model);

    // 5. Draw the sphere
    skySphere->draw();
}
```

---

## 🔢 Understanding the `alwaysBehindTransform` Matrix

OpenGL matrices are **column-major** in GLM, so:

```
glm::mat4 M = glm::mat4(
    // column 0  column 1  column 2  column 3
    1, 0, 0, 0,  // row 0
    0, 1, 0, 0,  // row 1
    0, 0, 0, 0,  // row 2
    0, 0, 1, 1   // row 3
);
```

Wait — actually GLM stores `glm::mat4(c0, c1, c2, c3)` as 4 column vectors. The constructor `glm::mat4(a,b,c,d,  e,f,g,h,  i,j,k,l,  m,n,o,p)` fills **column by column**:

```
Column 0: (a, b, c, d) = (1, 0, 0, 0)
Column 1: (e, f, g, h) = (0, 1, 0, 0)
Column 2: (i, j, k, l) = (0, 0, 0, 0)   ← the z column is zeroed
Column 3: (m, n, o, p) = (0, 0, 1, 1)   ← z gets contribution from w
```

As a math matrix (row × column):
```
| 1  0  0  0 |
| 0  1  0  0 |
| 0  0  0  1 |  ← new_z = 0*old_z + 1*old_w = old_w
| 0  0  0  1 |
```

Applied to `vec4(x, y, z, w)`:
- New x = x (unchanged)
- New y = y (unchanged)
- New z = w (copied from w!)
- New w = w (unchanged)

Result: `z/w = w/w = 1.0` → NDC z = 1.0 = farthest possible depth ✓

---

## 📍 Why the Sky Is Drawn Between Opaque and Transparent Objects

```
Render Pass:
  1. OPAQUE OBJECTS
     → Fill the scene with solid objects
     → Depth buffer now has values < 1.0 where objects are
  ↓
  2. SKY SPHERE  ← HERE
     → Depth test: only draws where depth = 1.0 (no object yet)
     → Efficient: no sky pixels are wasted on areas covered by objects
  ↓
  3. TRANSPARENT OBJECTS
     → Blended over the scene which now includes the sky background
```

**Why not draw sky before opaque?**
We could, but then the opaque objects would have to draw over every sky pixel — wasting GPU work (the sky pixels under opaque objects are computed and then overwritten).

**Why not draw sky after transparent?**
Because transparent objects need to blend with whatever is already in the framebuffer. If the sky is behind them, the sky must be drawn first so transparent objects blend with the sky color.

---

## ✅ Key Things to Remember

| Concept | Solution |
|---|---|
| Sky always around camera | `model = translate(identity, cameraPosition)` |
| Sky always behind everything | `alwaysBehindTransform * VP * model` → forces NDC.z = 1 |
| Viewing sphere from inside | `faceCulling.culledFace = GL_FRONT` |
| Correct depth test | `depthTesting.function = GL_LEQUAL` (1.0 ≤ 1.0 passes) |
| No mipmaps on sky | `loadImage(file, false)` |
| Vertical seam prevention | `GL_CLAMP_TO_EDGE` for T wrapping |
| Drawing order | Opaque → Sky → Transparent |
| `NDC.z = z/w` | Force z = w → z/w = 1 = max depth |

---

## 🧪 How to Test

```powershell
./bin/GAME_APPLICATION.exe -c='config/sky-test/test-1.jsonc' -f=10
```

Compare output in `screenshots/sky-test/` with `expected/sky-test/`.
