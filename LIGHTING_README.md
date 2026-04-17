# Requirement 1: Lighting — Implementation Guide

## Overview

This document explains how lighting was implemented in the ECS-based forward renderer, what each new file does, how everything connects together, and how to verify the implementation is correct.

---

## Files Created / Modified

### New Files

| File | Purpose |
|------|---------|
| `source/common/components/light.hpp` | `LightComponent` class declaration |
| `source/common/components/light.cpp` | `LightComponent::deserialize()` implementation |
| `assets/shaders/lit.vert` | Lit vertex shader |
| `assets/shaders/lit.frag` | Lit fragment shader — includes `light_common.glsl` |
| `assets/shaders/light_common.glsl` | Shared GLSL include: Light struct, TexturedMaterial struct, `calculateLighting()` function, global uniforms |
| `config/lighting-test/test-0.jsonc` | All 3 light types + all 5 texture maps |
| `config/lighting-test/test-1.jsonc` | Emissive only — no lights |
| `config/lighting-test/test-2.jsonc` | Normal correctness under non-uniform scale |
| `config/lighting-test/test-3.jsonc` | RGB color mixing (3 colored point lights) |
| `config/lighting-test/test-4.jsonc` | Spot cone edges — tight vs wide |
| `config/lighting-test/test-5.jsonc` | Point light attenuation over distance |
| `config/lighting-test/test-6.jsonc` | Directional light in isolation |

### Modified Files

| File | Change |
|------|--------|
| `source/common/material/material.hpp` | Added `LitMaterial` class + `"lit"` factory entry |
| `source/common/material/material.cpp` | `LitMaterial::setup()` and `::deserialize()` |
| `source/common/shader/shader.cpp` | Added `processIncludes()` — GLSL `#include` preprocessor |
| `source/common/components/component-deserializer.hpp` | Registered `LightComponent` with `"Light"` type string |
| `source/common/systems/forward-renderer.hpp` | Added `#include "light.hpp"` |
| `source/common/systems/forward-renderer.cpp` | Light collection loop + `sendLitUniforms` lambda |
| `CMakeLists.txt` | Added `light.cpp` to the build |
| `source/main.cpp` | Registered `"lighting-test"` state |

---

## Part 1 — LightComponent

**Files:** `light.hpp`, `light.cpp`

### What it stores

```cpp
enum class LightType { DIRECTIONAL, POINT, SPOT };

class LightComponent : public Component {
    LightType lightType;        // directional / point / spot
    glm::vec3 diffuse;          // diffuse color contribution
    glm::vec3 specular;         // specular color contribution
    glm::vec3 ambient;          // ambient color contribution
    float attenuationConstant;  // point/spot: Kc
    float attenuationLinear;    // point/spot: Kl
    float attenuationQuadratic; // point/spot: Kq
    float innerConeAngle;       // spot: inner cutoff (radians)
    float outerConeAngle;       // spot: outer cutoff (radians)
};
```

### Why position and direction are NOT stored here

The component does **not** store position or direction. These are computed at render time from the entity's transform:

- **Position** = `localToWorld * vec4(0,0,0,1)` — the entity's origin in world space  
- **Direction** = `normalize(localToWorld * vec4(0,0,-1,0))` — the entity's -Z axis in world space

This means you can move, rotate, or parent a light entity exactly like any other object in the scene.

### JSON format

```jsonc
{
    "type": "Light",
    "lightType": "point",          // "directional" | "point" | "spot"
    "diffuse":  [1.0, 1.0, 1.0],
    "specular": [1.0, 1.0, 1.0],
    "ambient":  [0.05, 0.05, 0.05],
    "attenuationConstant":  1.0,
    "attenuationLinear":    0.09,
    "attenuationQuadratic": 0.032,
    "innerConeAngle": 15.0,        // degrees — spot only
    "outerConeAngle": 30.0         // degrees — spot only
}
```

---

### Deep dive: every field explained

#### `lightType`

Controls which branch of the lighting equation is taken in the fragment shader.

| Value | Meaning | Has position? | Has direction? | Has attenuation? | Has cone? |
|-------|---------|:---:|:---:|:---:|:---:|
| `directional` | Models a far-away source (e.g. sun). All rays are parallel. | No | Yes | No | No |
| `point` | Radiates in all directions from one point (e.g. light bulb). | Yes | No | Yes | No |
| `spot` | Like a point light but restricted to a cone (e.g. flashlight, stage light). | Yes | Yes | Yes | Yes |

---

#### `diffuse` — the main color of the light

```
diffuse_contribution = light.diffuse × albedo × max(dot(N, L), 0)
```

- `light.diffuse` is the RGB color of the light's diffuse output.  
  A white sun: `[1, 1, 1]`. A warm lantern: `[1.0, 0.8, 0.5]`. A cold fluorescent: `[0.7, 0.8, 1.0]`.
- It is multiplied by the **surface albedo** (the diffuse texture color) and by `NdotL`.
- `NdotL = max(dot(N, L), 0)` — N is the surface normal, L is the direction toward the light.  
  When the surface faces the light directly (angle = 0°) → `NdotL = 1.0` → full diffuse.  
  When the surface is perpendicular to the light (angle = 90°) → `NdotL = 0.0` → no diffuse.  
  Negative values are clamped to 0 (surface facing away from the light gets nothing).

This is **Lambertian diffuse** — the physical rule that a tilted surface receives less energy per unit area.

---

#### `specular` — the highlight color of the light

```
specular_contribution = light.specular × specular_map × pow(max(dot(R, V), 0), shininess)
```

- `R = reflect(-L, N)` — the mirror-reflection direction of the light off the surface.
- `V = normalize(camera_position - world_position)` — the direction toward the viewer.
- When `R ≈ V` (camera is near the reflection direction) → `dot(R,V) ≈ 1` → strong highlight.
- `shininess` controls the size of the highlight (derived from roughness).  
  High shininess → tiny, sharp highlight (mirror-like surface).  
  Low shininess → wide, soft highlight (rough surface).
- `specular_map` tints or masks the highlight per pixel (e.g. metal areas shine, fabric areas do not).

The specular color of the light is often set to the same value as `diffuse`, but can be different.  
Example: a candle flame → warm `diffuse [1.0, 0.6, 0.2]` but almost white `specular [1.0, 0.95, 0.9]`.

---

#### `ambient` — the baseline, always-present light

```
ambient_contribution = light.ambient × albedo × ao
```

- Approximates indirect light that has bounced off other surfaces in the environment.
- In reality, a surface in shadow still receives some light — `ambient` models this cheaply.
- It is **not** affected by `NdotL` — it is added regardless of which way the surface faces.
- It is **not** attenuated by distance — indirect light fills the scene globally.
- Multiplied by `ao` (ambient occlusion map): crevices and corners that receive less indirect  
  light are darkened by a factor in the AO texture (0 = fully occluded, 1 = fully open).
- Typical values are small: `[0.05, 0.05, 0.05]` to `[0.1, 0.1, 0.1]`.  
  Too high → scene looks flat and washed-out. Too low → shadowed areas look completely black.

**Important:** ambient is accumulated from every light in the loop, so if you have 4 lights each  
with `ambient = [0.1, 0.1, 0.1]`, the total ambient base is `[0.4, 0.4, 0.4]`. Keep it small.

**Important:** the accumulation formula is `result += ambient + attenuation * (diffuse + specular)`,  
NOT `result += attenuation * (ambient + diffuse + specular)`. The ambient term must NOT be inside  
the attenuation multiplication — indirect light does not decay with distance.

---

#### `attenuationConstant`, `attenuationLinear`, `attenuationQuadratic` — light falloff

Only used by **Point** and **Spot** lights. Directional lights have no falloff (they model infinite-distance sources).

```
attenuation = 1.0 / (Kc + Kl × d + Kq × d²)
```

Where `d` is the distance from the light to the surface fragment.

| Coefficient | Symbol | Effect |
|-------------|--------|--------|
| `attenuationConstant` | Kc | Minimum denominator — prevents division by zero when `d = 0`. Always ≥ 1. |
| `attenuationLinear` | Kl | Linear falloff component — doubles the denominator every 1/Kl units. |
| `attenuationQuadratic` | Kq | Quadratic falloff — physically correct (energy spreads over a sphere of area 4πd²). |

**Physically correct** light uses only Kq (Kc=1, Kl=0, Kq=small).  
**Game-friendly** light uses all three tuned to a desired radius:

| Desired range | Kc | Kl | Kq |
|---|---|---|---|
| 7 units | 1.0 | 0.7 | 1.8 |
| 13 units | 1.0 | 0.35 | 0.44 |
| 50 units | 1.0 | 0.09 | 0.032 |
| 100 units | 1.0 | 0.045 | 0.0075 |

Example: `Kc=1, Kl=0.09, Kq=0.032` → light fades to near-zero at ~50 units.

---

#### `innerConeAngle` and `outerConeAngle` — spotlight softness

Only used by **Spot** lights.

```
cosTheta   = dot(L, normalize(-light.direction))   // how far fragment is from cone center
spotFactor = smoothstep(cos(outerAngle), cos(innerAngle), cosTheta)
```

- `innerConeAngle`: fragments within this angle from the cone axis receive **full** intensity.
- `outerConeAngle`: fragments beyond this angle from the cone axis receive **zero** intensity.
- Between inner and outer → `smoothstep` gives a smooth gradient (no hard edge).

The comparison is done in cosine space (not angles directly) because `dot(L, -direction)` already produces a cosine. Larger angle → smaller cosine, which is why the `smoothstep` arguments are in reversed order: `smoothstep(cos(outer), cos(inner), cosTheta)`.

Visual effect of different angle pairs:

| innerConeAngle | outerConeAngle | Result |
|---|---|---|
| 2° | 5° | Near-laser beam, very sharp hard edge |
| 15° | 25° | Stage spotlight, visible soft edge |
| 40° | 60° | Floodlight, very wide with gradual fade |
| 25° | 25.5° | Hard cutoff (inner ≈ outer, smoothstep range ≈ 0) |

---

#### Spot light rotation — critical note

The engine uses `glm::yawPitchRoll(rotation.y, rotation.x, rotation.z)` to build the entity's rotation matrix. With this convention, **pitch = +90°** rotates the local -Z forward axis to point **UP** (+Y), not down. To make a spot light point **downward**, use `"rotation": [-90, 0, 0]`.

```
glm::yawPitchRoll with pitch=+90°:  local -Z → world +Y  (UP)    ✗
glm::yawPitchRoll with pitch=-90°:  local -Z → world -Y  (DOWN)  ✓
```

In JSON:
```jsonc
// Spot pointing straight DOWN:
"rotation": [-90, 0, 0]

// Spot pointing straight UP:
"rotation": [90, 0, 0]
```

---

### Deep dive: how position and direction are extracted from the entity transform

The `LightComponent` stores **no** position or direction. They are computed every frame inside `sendLitUniforms` in `forward-renderer.cpp`:

```cpp
glm::mat4 ltw = lc->getOwner()->getLocalToWorldMatrix();

// Position: transform the local origin (0,0,0) into world space
glm::vec3 position  = glm::vec3(ltw * glm::vec4(0, 0, 0, 1));

// Direction: transform the local -Z axis into world space (w=0 = vector, not point)
glm::vec3 direction = glm::normalize(glm::vec3(ltw * glm::vec4(0, 0, -1, 0)));
```

**Why `vec4(0,0,0,1)` for position?**  
A homogeneous point has `w = 1`. The 4th column of `localToWorld` is the translation. Setting `w = 1` includes the translation, so you get the actual world-space location of the entity's origin.

**Why `vec4(0,0,-1,0)` for direction?**  
A direction vector has `w = 0`. Setting `w = 0` cancels the translation column — only rotation and scale affect it. We use `-Z` because in OpenGL/GLM, a camera (and by convention a light) "looks" along its negative Z axis. After rotating the entity, its local `-Z` axis points wherever you want the light to shine.

**Why this design is better than storing position/direction:**  
- You can **animate** the light by adding a `MovementComponent` to its entity — no code changes needed.  
- You can **parent** a light to a moving vehicle entity — the light automatically follows.  
- The light's position/direction is **always consistent** with what you see in the scene graph.  
- Zero duplication — the entity's transform already knows where it is; no need to keep a second copy.

---

## Part 2 — Shader Architecture

### `light_common.glsl` — the shared lighting library

All lighting logic lives in `assets/shaders/light_common.glsl`. Any shader that needs lighting simply adds `#include "light_common.glsl"` at the top and gets the complete implementation.

```
light_common.glsl provides:
  ├── #define constants (MAX_LIGHT_COUNT, TYPE_DIRECTIONAL/POINT/SPOT)
  ├── struct Light { ... }              — mirrors LightComponent fields sent by renderer
  ├── struct TexturedMaterial { ... }   — 5 sampler2D fields for the 5 texture maps
  ├── uniform Light lights[16]          — global light array
  ├── uniform int   light_count         — how many entries are valid this frame
  └── vec3 calculateLighting(...)       — full Phong model function
```

The `#include` directive is resolved at load time by `processIncludes()` in `shader.cpp` — not by the GPU driver. Paths are resolved relative to the including shader's directory.

#### Why a shared include file?

| Without `light_common.glsl` | With `light_common.glsl` |
|---|---|
| Duplicate `Light` struct in every shader | Single definition |
| Risk of struct field mismatches between shaders | Impossible — one source of truth |
| Adding a new shader requires copying 100+ lines | Just `#include "light_common.glsl"` |
| Fixing a lighting bug requires editing multiple files | Fix once, all shaders update |

### `lit.vert`

Compared to `textured.vert`, this shader adds:

- **Input** `layout(location = 3) in vec3 normal` — reads per-vertex normals (the VAO already sets this up via `ATTRIB_LOC_NORMAL = 3` in `mesh.hpp`)
- **Uniforms** `object_to_world` (model matrix) and `object_to_world_inv_transpose` (for correct normal transformation)
- **Outputs** `world_position` and `world_normal` in addition to `color` and `tex_coord`

```glsl
vs_out.world_position = (object_to_world * vec4(position, 1.0)).xyz;
vs_out.world_normal   = normalize((object_to_world_inv_transpose * vec4(normal, 0.0)).xyz);
```

The **inverse-transpose** of the model matrix is required so that normals remain perpendicular to surfaces under non-uniform scaling.  
The regular `transform` uniform (MVP) is still used for `gl_Position`, keeping compatibility with the existing renderer code.

### `lit.vert` — fully annotated

```glsl
#version 330 core

layout(location = 0) in vec3 position;   // object-space vertex position
layout(location = 1) in vec4 color;      // per-vertex color (RGBA)
layout(location = 2) in vec2 tex_coord;  // UV texture coordinate
layout(location = 3) in vec3 normal;     // object-space surface normal

out Varyings {
    vec4 color;           // passed through unchanged — used to tint albedo
    vec2 tex_coord;       // passed through unchanged — used to sample textures
    vec3 world_position;  // fragment position in WORLD space
    vec3 world_normal;    // fragment normal  in WORLD space
} vs_out;

uniform mat4 transform;                    // MVP matrix  — used only for gl_Position
uniform mat4 object_to_world;              // Model matrix — transforms positions to world space
uniform mat4 object_to_world_inv_transpose;// Inverse-transpose of model — transforms normals correctly

void main(){
    gl_Position = transform * vec4(position, 1.0);
    vs_out.color     = color;
    vs_out.tex_coord = tex_coord;
    vs_out.world_position = (object_to_world * vec4(position, 1.0)).xyz;
    vs_out.world_normal   = normalize((object_to_world_inv_transpose * vec4(normal, 0.0)).xyz);
}
```

**Why two separate matrix uniforms instead of one?**  
`transform` (MVP) puts the vertex in the right screen position. `object_to_world` (M only) gives us the world-space values we need to do lighting math.

**Why is the inverse-transpose needed for normals?**  
Normals must stay perpendicular to surfaces. Under non-uniform scaling (e.g. scale X by 3), a plain matrix multiply distorts normals toward the surface. The inverse-transpose negates the scaling effect while preserving the rotation — keeping normals perpendicular even after non-uniform scaling. Verified visually by test-2.

### `lit.frag`

The fragment shader includes `light_common.glsl` for all shared definitions, then calls `calculateLighting()`:

```glsl
#version 330 core
#include "light_common.glsl"

in Varyings { vec4 color; vec2 tex_coord; vec3 world_position; vec3 world_normal; } fs_in;

uniform TexturedMaterial material;  // struct with 5 sampler2D fields from light_common.glsl
uniform vec3 camera_position;
out vec4 frag_color;

void main(){
    vec3  albedo   = texture(material.albedo_tex,            fs_in.tex_coord).rgb * fs_in.color.rgb;
    vec3  specTex  = texture(material.specular_tex,          fs_in.tex_coord).rgb;
    float ao       = texture(material.ambient_occlusion_tex, fs_in.tex_coord).r;
    float rough    = texture(material.roughness_tex,         fs_in.tex_coord).r;
    vec3  emissive = texture(material.emissive_tex,          fs_in.tex_coord).rgb;

    float shininess = 2.0 / pow(clamp(rough, 0.001, 0.999), 4.0) - 2.0;

    vec3 N = normalize(fs_in.world_normal);
    vec3 V = normalize(camera_position - fs_in.world_position);

    frag_color = vec4(calculateLighting(albedo, specTex, ao, shininess, emissive,
                                        fs_in.world_position, N, V), 1.0);
}
```

#### Texture maps (5 units)

The samplers live inside a `TexturedMaterial` struct (defined in `light_common.glsl`). The C++ side sends them as `material.albedo_tex`, `material.specular_tex`, etc.

| Unit | Uniform | Meaning | Default (1×1 pixel) |
|------|---------|---------|---------------------|
| 0 | `material.albedo_tex` | Surface base color | White (1,1,1) |
| 1 | `material.specular_tex` | Per-pixel specular tint | White (1,1,1) |
| 2 | `material.ambient_occlusion_tex` | AO — scales ambient term | White (1,1,1) = no occlusion |
| 3 | `material.roughness_tex` | Surface roughness (r channel) | Mid-gray (0.5) |
| 4 | `material.emissive_tex` | Self-emitted light | Black (0,0,0) = no emission |

#### Roughness → Shininess conversion

```glsl
float shininess = 2.0 / pow(clamp(rough, 0.001, 0.999), 4.0) - 2.0;
```

| rough | shininess | Surface type |
|-------|-----------|--------------|
| 0.1 | ~1998 | Mirror-like |
| 0.5 | ~30 | Painted surface |
| 0.9 | ~1.7 | Very rough/matte |

#### `calculateLighting()` in `light_common.glsl`

```glsl
vec3 result = emissive;   // start with emissive — glows even with zero lights

for(int i = 0; i < min(light_count, MAX_LIGHT_COUNT); i++){
    // compute L, attenuation based on light.type
    // ambient   = light.ambient  * albedo * ao
    // diffuse   = light.diffuse  * albedo * max(dot(N,L), 0)
    // specular  = light.specular * specTex * pow(max(dot(R,V), 0), shininess) [guarded by NdotL > 0]
    result += ambient_contrib + attenuation * (diffuse_contrib + specular_contrib);
    //         ↑ NOT attenuated      ↑ attenuated (distance + cone falloff)
}
```

#### Light type behaviour

| Type | L vector | Attenuation |
|------|----------|-------------|
| Directional | `-light.direction` (constant) | 1.0 (no falloff) |
| Point | `normalize(light.position - worldPos)` | `1 / (Kc + Kl·d + Kq·d²)` |
| Spot | same as Point | Point attenuation × `smoothstep(outerCutoff, innerCutoff, cosTheta)` |

---

## Part 3 — LitMaterial

**Class:** `LitMaterial` in `material.hpp` / `material.cpp`

Inherits directly from `Material` (not `TintedMaterial`/`TexturedMaterial`).

### `setup()`

Uses a `bindMap` lambda to bind all 5 texture maps to texture units 0–4 and send their unit indices to the shader. The uniform names match the `TexturedMaterial` struct fields in `light_common.glsl`.

```cpp
auto bindMap = [&](int unit, Texture2D* tex, Texture2D* fallback, const std::string& name){
    glActiveTexture(GL_TEXTURE0 + unit);
    (tex ? tex : fallback)->bind();
    if(sampler) sampler->bind(unit);
    shader->set(name, unit);
};

bindMap(0, albedo_map,            white,   "material.albedo_tex");
bindMap(1, specular_map,          white,   "material.specular_tex");
bindMap(2, ambient_occlusion_map, white,   "material.ambient_occlusion_tex");
bindMap(3, roughness_map,         midgray, "material.roughness_tex");
bindMap(4, emissive_map,          black,   "material.emissive_tex");
```

For any map that is `nullptr` (not configured in JSON), a pre-allocated 1×1 fallback texture is used — so the shader always has valid samplers and no GL errors occur.

### `deserialize()`

```jsonc
{
    "type": "lit",
    "shader": "lit",
    "pipelineState": { "depthTesting": { "enabled": true } },
    "albedo_map":            "my-texture-name",
    "specular_map":          "my-spec-name",
    "ambient_occlusion_map": "my-ao-name",
    "roughness_map":         "my-rough-name",
    "emissive_map":          "my-emit-name",
    "sampler":               "default"
}
```

All texture keys are optional — missing ones use the 1×1 fallback.

### Deep dive: 1×1 fallback textures

```cpp
static Texture2D* white   = makeDefaultTexture(255, 255, 255, 255);
static Texture2D* black   = makeDefaultTexture(0,   0,   0,   255);
static Texture2D* midgray = makeDefaultTexture(128, 128, 128, 255);
```

| Map | Fallback | Why that value |
|-----|----------|----------------|
| `albedo_tex` | white | Surface color = 1 → light color comes through unmodified |
| `specular_tex` | white | Full specular on all areas (no masking) |
| `ambient_occlusion_tex` | white | No occlusion → `ao = 1.0` → ambient not reduced |
| `roughness_tex` | mid-gray (0.5) | Medium shininess — visible but not mirror-sharp |
| `emissive_tex` | black | `emissive = 0` → no glow |

These are `static` so they are created once on the first `setup()` call and reused every frame.

---

## Part 4 — GLSL `#include` Preprocessor

**File:** `source/common/shader/shader.cpp`

The GLSL 3.30 spec does not support `#include`. The function `processIncludes()` was added to `shader.cpp` to resolve `#include "file"` directives before passing source to the GPU driver:

```cpp
static std::string processIncludes(const std::string& source, const std::string& directory) {
    // scan source line by line
    // when "#include \"file\"" found, load file from directory + "/" + file
    // recursively process includes in loaded content
    // replace the #include line with the file contents
}
// In attach(): sourceString = processIncludes(sourceString, getDirectory(filename));
```

- Paths are resolved **relative to the including shader's directory**  
  (`assets/shaders/lit.frag` includes `light_common.glsl` → resolves to `assets/shaders/light_common.glsl`)
- Supports **recursive includes** (an include can itself include other files)
- Handles **leading whitespace** before `#include`

---

## Part 5 — Forward Renderer Changes

### Light collection

At the start of each `render()` call, all entities with a `LightComponent` are collected into a local `std::vector<LightComponent*> lights`. This happens in the same loop that collects mesh renderer commands.

### `sendLitUniforms` lambda

After `command.material->setup()`, this lambda is called for every draw command:

```cpp
LitMaterial* lit = dynamic_cast<LitMaterial*>(command.material);
if(!lit) return;  // non-lit materials are skipped — zero overhead
```

If the material is a `LitMaterial`, it sends:

1. `camera_position` — for specular computation (view vector V)
2. `object_to_world` — model matrix for transforming positions
3. `object_to_world_inv_transpose` — for correct normal transformation
4. `light_count` — how many lights to process
5. For each light: `type`, `position`, `direction`, `diffuse`, `specular`, `ambient`, attenuation coefficients, `innerCutoff`/`outerCutoff` (as cosines)

Position and direction are derived from the light entity's transform at draw time:

```cpp
glm::mat4 ltw = lc->getOwner()->getLocalToWorldMatrix();
lit->shader->set(base + "position",
    glm::vec3(ltw * glm::vec4(0, 0, 0, 1)));
lit->shader->set(base + "direction",
    glm::normalize(glm::vec3(ltw * glm::vec4(0, 0, -1, 0))));

// Cone angles are sent as cosines so the shader avoids calling acos() per fragment
lit->shader->set(base + "innerCutoff", glm::cos(lc->innerConeAngle));
lit->shader->set(base + "outerCutoff", glm::cos(lc->outerConeAngle));
```

### Why the light loop is inside `render()` and not pre-built once

The light array is rebuilt **every frame** because:
- Lights can be on moving entities (their world position changes every frame)
- Lights can be added or removed from the world at runtime
- The cost is proportional to `light_count × mesh_count`, but with the `dynamic_cast` early-exit, non-lit meshes cost only one failed cast per draw call

---

## How to Verify the Implementation

### Dedicated test scenes (run from project root)

```bash
# All 3 types + all 5 texture maps (spot now correctly points DOWN)
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-0.jsonc

# Emissive only — no lights, sphere glows from emissive map alone
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-1.jsonc

# Normal correctness — 3 spheres at non-uniform scales
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-2.jsonc

# RGB color mixing — red/green/blue point lights, verify additive accumulation
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-3.jsonc

# Spot cone edges — tight (10°/20°) vs wide (30°/50°) spot on floor + spheres
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-4.jsonc

# Attenuation — 4 identical spheres at increasing distances from one point light
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-5.jsonc

# Directional only — 3 spheres at different depths must look identical (no falloff)
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-6.jsonc
```

See `LIGHTING_MANUAL_TESTING.md` for detailed pass criteria for each test.

---

## Checklist Against the Requirements

| Requirement | Implemented | Where |
|-------------|-------------|-------|
| Light component holds color, type, cone angles | ✅ | `light.hpp` |
| Position & direction derived from entity transform | ✅ | `forward-renderer.cpp` `sendLitUniforms` |
| Shader supports lighting (Phong model) | ✅ | `lit.vert` + `lit.frag` + `light_common.glsl` |
| Shared lighting library for scalable multi-shader use | ✅ | `light_common.glsl` via `#include` |
| GLSL `#include` preprocessor | ✅ | `shader.cpp` `processIncludes()` |
| Lit material with albedo, specular, roughness, AO, emissive maps | ✅ | `LitMaterial` in `material.hpp/cpp` |
| Forward renderer detects and sends lights | ✅ | `forward-renderer.cpp` |
| Multiple lights with different types/parameters | ✅ | `lights[]` uniform array, loop in `calculateLighting()` |
| Supports up to 16 lights simultaneously | ✅ | `MAX_LIGHT_COUNT 16` in `light_common.glsl` |
| Ambient NOT attenuated by distance | ✅ | `result += ambient + attenuation * (diffuse + specular)` |
| Specular guarded by NdotL > 0 | ✅ | `(NdotL > 0.0) ? pow(...) : 0.0` |
| Non-lit materials unaffected | ✅ | `dynamic_cast` guard in renderer |
