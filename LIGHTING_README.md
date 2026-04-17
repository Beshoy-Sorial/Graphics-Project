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
| `assets/shaders/lit.frag` | Lit fragment shader (Phong lighting) |
| `config/lighting-test/test-0.jsonc` | Test scene with lights and lit materials |

### Modified Files

| File | Change |
|------|--------|
| `source/common/material/material.hpp` | Added `LitMaterial` class + `"lit"` factory entry |
| `source/common/material/material.cpp` | `LitMaterial::setup()` and `::deserialize()` |
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
- Multiplied by `ao` (ambient occlusion map): crevices and corners that receive less indirect  
  light are darkened by a factor in the AO texture (0 = fully occluded, 1 = fully open).
- Typical values are small: `[0.05, 0.05, 0.05]` to `[0.1, 0.1, 0.1]`.  
  Too high → scene looks flat and washed-out. Too low → shadowed areas look completely black.

**Important:** ambient is accumulated from every light in the loop, so if you have 4 lights each  
with `ambient = [0.1, 0.1, 0.1]`, the total ambient base is `[0.4, 0.4, 0.4]`. Keep it small.

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

## Part 2 — Shaders

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

// ── Vertex inputs ──────────────────────────────────────────────────────
// These match the attribute locations defined in mesh.hpp:
//   ATTRIB_LOC_POSITION = 0, ATTRIB_LOC_COLOR = 1,
//   ATTRIB_LOC_TEXCOORD = 2, ATTRIB_LOC_NORMAL = 3
// The VAO was set up in Mesh::Mesh() to fill all 4 of these.

layout(location = 0) in vec3 position;   // object-space vertex position
layout(location = 1) in vec4 color;      // per-vertex color (RGBA)
layout(location = 2) in vec2 tex_coord;  // UV texture coordinate
layout(location = 3) in vec3 normal;     // object-space surface normal

// ── Outputs to the fragment shader ────────────────────────────────────
out Varyings {
    vec4 color;           // passed through unchanged — used to tint albedo
    vec2 tex_coord;       // passed through unchanged — used to sample textures
    vec3 world_position;  // fragment position in WORLD space — needed for light distance & direction
    vec3 world_normal;    // fragment normal  in WORLD space — needed for diffuse & specular angles
} vs_out;

// ── Uniforms set by the forward renderer ──────────────────────────────
uniform mat4 transform;                    // MVP matrix  — used only for gl_Position (clip space)
uniform mat4 object_to_world;              // Model matrix — transforms positions to world space
uniform mat4 object_to_world_inv_transpose;// Inverse-transpose of model — transforms normals correctly

void main(){
    // Place the vertex in clip space (required by OpenGL)
    // transform = Projection × View × Model
    gl_Position = transform * vec4(position, 1.0);

    // Pass color and UV through unchanged
    vs_out.color     = color;
    vs_out.tex_coord = tex_coord;

    // Transform the vertex position into world space.
    // w=1 means "this is a point", so the translation column of object_to_world is applied.
    // We need world_position to compute the vector from fragment to light (for point/spot lights).
    vs_out.world_position = (object_to_world * vec4(position, 1.0)).xyz;

    // Transform the normal into world space.
    // IMPORTANT: we use object_to_world_inv_transpose, NOT object_to_world.
    // Reason: under non-uniform scaling (e.g. scale [3,1,1]), the model matrix distorts
    // directions. The inverse-transpose undoes that distortion for normals.
    // w=0 means "this is a direction", so translation does NOT affect it (correct for normals).
    // We normalize because interpolation between vertices can shorten the vector.
    vs_out.world_normal = normalize((object_to_world_inv_transpose * vec4(normal, 0.0)).xyz);
}
```

**Why two separate matrix uniforms instead of one?**  
`transform` (MVP) puts the vertex in the right screen position. `object_to_world` (M only) gives us the world-space values we need to do lighting math. If we used only MVP, we would have no way to get back world-space coordinates from clip-space coordinates.

**Why is the inverse-transpose needed for normals?**  
Normals must stay perpendicular to surfaces. A regular matrix multiplication can rotate AND scale directions, but a scaling in X by factor 3 would push a normal that points in X toward the surface instead of away from it. The inverse-transpose negates the scaling effect while preserving the rotation, keeping normals correctly perpendicular even after non-uniform scaling.

### `lit.frag`

#### Texture maps (5 units)

| Unit | Uniform | Meaning | Default (1×1 pixel) |
|------|---------|---------|---------------------|
| 0 | `albedo_map` | Surface base color | White (1,1,1) |
| 1 | `specular_map` | Per-pixel specular tint | White (1,1,1) |
| 2 | `ambient_occlusion_map` | AO — scales ambient term | White (1,1,1) = no occlusion |
| 3 | `roughness_map` | Surface roughness (r channel) | Mid-gray (0.5) |
| 4 | `emissive_map` | Self-emitted light | Black (0,0,0) = no emission |

#### Roughness → Shininess conversion

```glsl
float shininess = 2.0 / pow(clamp(rough, 0.001, 0.999), 4.0) - 2.0;
```

High roughness → low shininess (broad highlight). Low roughness → high shininess (sharp highlight).

#### Light loop (up to 16 lights)

```glsl
vec3 result = emissive;   // start with emissive, then accumulate

for(int i = 0; i < min(light_count, MAX_LIGHT_COUNT); i++){
    // compute L, attenuation based on light.type
    // ambient  = light.ambient  * albedo * ao
    // diffuse  = light.diffuse  * albedo * max(dot(N,L), 0)
    // specular = light.specular * specTex * pow(max(dot(R,V), 0), shininess)
    // ambient is NOT attenuated — indirect light fills the scene regardless of distance
    result += ambient + attenuation * (diffuse + specular);
}
```

#### Light type behaviour

| Type | L vector | Attenuation |
|------|----------|-------------|
| Directional | `-light.direction` (constant) | 1.0 (no falloff) |
| Point | `normalize(light.position - worldPos)` | `1 / (Kc + Kl·d + Kq·d²)` |
| Spot | same as Point | Point attenuation × `smoothstep(outerCutoff, innerCutoff, cosTheta)` |

`smoothstep` gives a soft transition between the inner (full intensity) and outer (zero intensity) cone angles.

### `lit.frag` — fully annotated

```glsl
#version 330 core

// ── Constants ──────────────────────────────────────────────────────────
#define MAX_LIGHT_COUNT 16   // maximum number of lights the shader processes per frame
#define TYPE_DIRECTIONAL 0   // matches the int cast of LightType::DIRECTIONAL in C++
#define TYPE_POINT       1   // matches LightType::POINT
#define TYPE_SPOT        2   // matches LightType::SPOT

// ── Light struct ───────────────────────────────────────────────────────
// This mirrors the per-light data sent from forward-renderer.cpp.
// Every field must match the name + type used in shader->set("lights[i].fieldName", ...)
struct Light {
    int   type;                  // 0=directional, 1=point, 2=spot
    vec3  position;              // world-space position (point/spot only)
    vec3  direction;             // world-space direction the light shines toward (directional/spot)
    vec3  diffuse;               // RGB color of the diffuse contribution
    vec3  specular;              // RGB color of the specular contribution
    vec3  ambient;               // RGB color of the ambient contribution
    float attenuationConstant;   // Kc: minimum denominator term (prevents div/0)
    float attenuationLinear;     // Kl: linear distance falloff coefficient
    float attenuationQuadratic;  // Kq: quadratic distance falloff coefficient (physically correct)
    float innerCutoff;           // cos(innerConeAngle) — precomputed on CPU to avoid acos in shader
    float outerCutoff;           // cos(outerConeAngle) — same
};

// ── Inputs from vertex shader ──────────────────────────────────────────
in Varyings {
    vec4 color;           // per-vertex tint
    vec2 tex_coord;       // UV coordinate for texture sampling
    vec3 world_position;  // fragment position in world space
    vec3 world_normal;    // fragment normal in world space (already normalized in vert shader)
} fs_in;

out vec4 frag_color;  // final RGBA color written to the framebuffer

// ── Texture maps ───────────────────────────────────────────────────────
// Bound in LitMaterial::setup() to texture units 0–4.
// Fallback 1×1 textures are used when the JSON does not specify a map.
uniform sampler2D albedo_map;            // unit 0: base surface color (diffuse reflectance)
uniform sampler2D specular_map;          // unit 1: per-pixel specular tint/mask
uniform sampler2D ambient_occlusion_map; // unit 2: pre-baked shadowing of crevices (grayscale)
uniform sampler2D roughness_map;         // unit 3: surface roughness (r channel only)
uniform sampler2D emissive_map;          // unit 4: self-emitted light (glows without any light source)

// ── Per-frame uniforms ─────────────────────────────────────────────────
uniform vec3  camera_position;           // world-space eye position (for computing view vector V)
uniform Light lights[MAX_LIGHT_COUNT];   // array of all active lights
uniform int   light_count;              // how many entries in lights[] are valid this frame

// ══════════════════════════════════════════════════════════════════════
void main(){

    // ── Sample all texture maps ──────────────────────────────────────
    // .rgb to discard alpha (we don't use alpha in lighting math)
    vec3  albedo   = texture(albedo_map,            fs_in.tex_coord).rgb * fs_in.color.rgb;
    //     ↑ tint by vertex color so a single material can be recolored from the scene graph

    vec3  specTex  = texture(specular_map,           fs_in.tex_coord).rgb;
    //     ↑ scales the specular highlight per pixel: white = full specular, black = no specular

    float ao       = texture(ambient_occlusion_map,  fs_in.tex_coord).r;
    //     ↑ single channel (r). Value 1.0 = fully open (no occlusion), 0.0 = fully blocked

    float rough    = texture(roughness_map,           fs_in.tex_coord).r;
    //     ↑ single channel (r). 0 = mirror-smooth surface, 1 = completely rough/matte

    vec3  emissive = texture(emissive_map,            fs_in.tex_coord).rgb;
    //     ↑ added to final result unconditionally — represents glowing / self-lit areas

    // ── Roughness → Shininess ────────────────────────────────────────
    // Formula from the lecture (page 69):
    //   shininess = 2 / r^4 - 2
    // clamp prevents r=0 (→ infinite shininess) and r=1 (→ shininess < 0).
    float shininess = 2.0 / pow(clamp(rough, 0.001, 0.999), 4.0) - 2.0;
    //   rough = 0.1  → shininess ≈ 1998  (very sharp mirror-like highlight)
    //   rough = 0.5  → shininess ≈  30   (typical painted surface)
    //   rough = 0.9  → shininess ≈   1.7 (very wide, soft highlight)

    // ── Lighting vectors constant for this fragment ──────────────────
    vec3 N = normalize(fs_in.world_normal);
    //   ↑ re-normalize after interpolation across the triangle (interpolation can de-normalize)

    vec3 V = normalize(camera_position - fs_in.world_position);
    //   ↑ direction from this fragment toward the camera eye — used for specular (angle of view)

    // ── Accumulate contributions from all lights ─────────────────────
    // Start with emissive so the surface glows even when light_count == 0
    vec3 result = emissive;

    for(int i = 0; i < min(light_count, MAX_LIGHT_COUNT); i++){
        Light light = lights[i];  // copy struct locally for this iteration

        vec3  L;               // direction FROM the fragment TOWARD the light
        float attenuation = 1.0; // starts at full intensity; reduced for point/spot

        // ── Compute L and attenuation per light type ─────────────────
        if(light.type == TYPE_DIRECTIONAL){
            // Directional: all rays are parallel and come from the same fixed direction.
            // light.direction points FROM the light TOWARD the scene (like "sun direction").
            // We negate it to get the vector FROM the fragment TOWARD the light source.
            L = normalize(-light.direction);
            // attenuation stays 1.0 — directional lights have no distance falloff

        } else {
            // Point or Spot: the light is at a specific world-space position.
            vec3  toLight = light.position - fs_in.world_position; // vector from fragment to light
            float dist    = length(toLight);                         // distance (scalar)
            L = normalize(toLight);                                  // unit direction

            // Quadratic attenuation: energy diminishes with distance squared (physically correct).
            // Kc prevents division-by-zero when dist ≈ 0.
            attenuation = 1.0 / (light.attenuationConstant
                               + light.attenuationLinear    * dist
                               + light.attenuationQuadratic * dist * dist);

            if(light.type == TYPE_SPOT){
                // Spot: additionally restrict light to a cone.
                // cosTheta = how aligned the fragment-to-light direction is with the cone axis.
                //   cosTheta = 1.0  →  fragment is on the cone axis (center of beam)
                //   cosTheta < cos(outerAngle) → fragment is outside the cone → no light
                float cosTheta   = dot(L, normalize(-light.direction));

                // smoothstep(edge0, edge1, x):
                //   x < edge0  → returns 0.0
                //   x > edge1  → returns 1.0
                //   between    → smooth Hermite curve
                // Here: x < outerCutoff → 0 (outside cone), x > innerCutoff → 1 (full intensity)
                float spotFactor = smoothstep(light.outerCutoff, light.innerCutoff, cosTheta);

                attenuation *= spotFactor;  // multiply so outside-cone fragments get zero
            }
        }

        // ── Diffuse term (Lambertian) ─────────────────────────────────
        // NdotL = cos of angle between surface normal and light direction.
        // max(..., 0) clamps: surfaces facing away from the light get 0, not negative.
        float NdotL = max(dot(N, L), 0.0);

        // ambient: constant contribution, scaled by AO map to darken crevices
        vec3 ambient_contrib  = light.ambient  * albedo * ao;

        // diffuse: proportional to how directly the surface faces the light
        vec3 diffuse_contrib  = light.diffuse  * albedo * NdotL;

        // ── Specular term (Phong) ─────────────────────────────────────
        // R = mirror reflection of the incoming light direction off the surface.
        // reflect(-L, N): reflect the vector pointing AWAY from the light around N.
        vec3  R      = reflect(-L, N);
        float RdotV  = max(dot(R, V), 0.0);

        // Guard: only compute specular when the surface actually faces the light (NdotL > 0).
        // Without this, a surface facing AWAY from the light could still get a specular
        // highlight because reflect() would produce a vector pointing toward the camera.
        float specFactor = (NdotL > 0.0) ? pow(RdotV, shininess) : 0.0;

        vec3 specular_contrib = light.specular * specTex * specFactor;

        // ── Accumulate this light's contribution ──────────────────────
        // ambient is indirect light — NOT attenuated by distance (it fills the scene globally).
        // Only direct contributions (diffuse + specular) decay with distance/cone falloff.
        result += ambient_contrib + attenuation * (diffuse_contrib + specular_contrib);
    }

    // ── Output ────────────────────────────────────────────────────────
    // Alpha is always 1.0 — the lit material does not support transparency.
    frag_color = vec4(result, 1.0);
}
```

---

## Part 3 — LitMaterial

**Class:** `LitMaterial` in `material.hpp` / `material.cpp`

Inherits directly from `Material` (not `TintedMaterial`/`TexturedMaterial`).

### `setup()`

Binds all 5 texture maps to texture units 0–4 and sends their unit indices to the shader.  
For any map that is `nullptr` (not configured in JSON), a pre-allocated 1×1 fallback texture is used — so the shader always has valid samplers and no GL errors occur.

### `deserialize()`

Reads from `AssetLoader<Texture2D>` using these JSON keys:

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

When a texture map is not specified in the JSON, `LitMaterial::setup()` binds a hard-coded 1×1 pixel texture instead. This keeps the shader from reading from an unbound sampler (which is undefined behaviour in OpenGL).

```cpp
static Texture2D* white   = makeDefaultTexture(255, 255, 255, 255);
static Texture2D* black   = makeDefaultTexture(0,   0,   0,   255);
static Texture2D* midgray = makeDefaultTexture(128, 128, 128, 255);
```

| Map | Fallback | Why that value |
|-----|----------|----------------|
| `albedo_map` | white | Surface color = 1 → light color comes through unmodified |
| `specular_map` | white | Full specular on all areas (no masking) |
| `ambient_occlusion_map` | white | No occlusion → `ao = 1.0` → ambient not reduced |
| `roughness_map` | mid-gray (0.5) | Medium shininess — visible but not mirror-sharp |
| `emissive_map` | black | `emissive = 0` → no glow |

These are `static` so they are created once on the first `setup()` call and reused every frame. They are never deleted (they live for the duration of the program) — a safe simplification given that OpenGL context lifetime matches program lifetime here.

---

## Part 4 — Forward Renderer Changes

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
5. For each light: all fields of the `Light` struct (type, position, direction, colors, attenuation, cutoffs)

Position and direction are derived from the light entity's transform at draw time.

### Deep dive: the full `sendLitUniforms` logic annotated

```cpp
auto sendLitUniforms = [&](const RenderCommand& command){

    // dynamic_cast returns nullptr if command.material is NOT a LitMaterial.
    // This is how we distinguish lit objects from tinted/textured objects
    // without changing the base Material class or the core rendering loop.
    LitMaterial* lit = dynamic_cast<LitMaterial*>(command.material);
    if(!lit) return;  // not a lit material — skip entirely, zero overhead for other materials

    // Camera position in world space — needed in the fragment shader to compute V (view vector).
    // cameraPosition was already computed earlier in render() from the camera entity's transform.
    lit->shader->set("camera_position", cameraPosition);

    // Model matrix (object → world). Used in the fragment shader to compute world_position
    // (needed for point/spot light distance calculations).
    lit->shader->set("object_to_world", command.localToWorld);

    // Inverse-transpose of the model matrix. Used in the VERTEX shader to transform normals.
    // glm::inverse() computes M⁻¹, glm::transpose() then transposes it → (M⁻¹)ᵀ.
    // This is re-computed per draw call. For performance, could be cached if the transform is static.
    lit->shader->set("object_to_world_inv_transpose",
                     glm::transpose(glm::inverse(command.localToWorld)));

    // Clamp to 16 — the shader array lights[MAX_LIGHT_COUNT] cannot hold more.
    int count = static_cast<int>(lights.size());
    if(count > 16) count = 16;
    lit->shader->set("light_count", count);

    // Send each light's data as a flat series of uniforms.
    // GLSL struct arrays are set field by field using the "lights[i].fieldName" naming convention.
    for(int i = 0; i < count; i++){
        LightComponent* lc   = lights[i];
        std::string     base = "lights[" + std::to_string(i) + "].";

        // int cast: LightType::DIRECTIONAL=0, POINT=1, SPOT=2 — matches #define in the shader
        lit->shader->set(base + "type", static_cast<int>(lc->lightType));

        // Derive position and direction from the light entity's transform (see Part 1 deep dive)
        glm::mat4 ltw = lc->getOwner()->getLocalToWorldMatrix();
        lit->shader->set(base + "position",
            glm::vec3(ltw * glm::vec4(0, 0, 0, 1)));          // entity origin in world space
        lit->shader->set(base + "direction",
            glm::normalize(glm::vec3(ltw * glm::vec4(0, 0, -1, 0)))); // entity -Z in world space

        // Light color components
        lit->shader->set(base + "diffuse",  lc->diffuse);
        lit->shader->set(base + "specular", lc->specular);
        lit->shader->set(base + "ambient",  lc->ambient);

        // Attenuation coefficients (only used by point/spot, but sent for all — shader branches)
        lit->shader->set(base + "attenuationConstant",  lc->attenuationConstant);
        lit->shader->set(base + "attenuationLinear",    lc->attenuationLinear);
        lit->shader->set(base + "attenuationQuadratic", lc->attenuationQuadratic);

        // Cone cutoffs stored as cosines (not angles) so the shader avoids calling acos().
        // glm::cos() of an angle in radians gives the dot-product threshold directly.
        lit->shader->set(base + "innerCutoff", glm::cos(lc->innerConeAngle));
        lit->shader->set(base + "outerCutoff", glm::cos(lc->outerConeAngle));
    }
};
```

### Why the light loop is inside `render()` and not pre-built once

The light array is rebuilt **every frame** because:
- Lights can be on moving entities (their world position changes every frame)
- Lights can be added or removed from the world at runtime
- The cost is proportional to `light_count × mesh_count`, but with the `dynamic_cast` early-exit,  
  non-lit meshes cost only one failed cast per draw call

---

## How to Verify the Implementation

### Step 1 — Run the lighting test scene

```bash
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-0.jsonc
```

**What you should see:**
- A Suzanne (monkey) head, a sphere, and a ground plane — all lit
- The point light from `[5, 5, 5]` casts brighter light on the surfaces facing it
- The directional light gives a soft blue-tinted fill from above
- Surfaces facing away from both lights appear dark (only ambient remains)
- Specular highlights are visible as small bright spots on the sphere and monkey

### Step 2 — Verify each light type works

Add this to the `"world"` array in the config to test a spot light:

```jsonc
{
    "position": [0, 4, 0],
    "rotation": [90, 0, 0],
    "components": [{
        "type": "Light",
        "lightType": "spot",
        "diffuse":  [1.0, 0.3, 0.3],
        "specular": [1.0, 0.3, 0.3],
        "ambient":  [0.0, 0.0, 0.0],
        "attenuationConstant":  1.0,
        "attenuationLinear":    0.14,
        "attenuationQuadratic": 0.07,
        "innerConeAngle": 12.5,
        "outerConeAngle": 25.0
    }]
}
```

**Expected:** A red circular spotlight cone visible on the ground plane below `[0, 4, 0]`, with a soft edge between inner and outer cone angles.

### Step 3 — Verify multiple lights work simultaneously

The test scene already has **2 lights** (1 point + 1 directional) affecting the same objects.  
To confirm, temporarily change `light_count` checking: if you remove one light entity from the config, the scene should be noticeably darker / differently shaded.

### Step 4 — Verify normal transformation (non-uniform scale)

Add `"scale": [3, 1, 1]` to a sphere entity. The normals should still look correct (specular highlight stays at the correct position on the surface). If normals were transformed with the plain model matrix instead of the inverse-transpose, the highlight would slide incorrectly.

### Step 5 — Verify texture maps

Add a specular map to a material and observe that specular highlights are masked where the specular map is dark. Add an emissive map and observe that those surfaces glow even with no lights nearby.

### Step 6 — Check that non-lit materials are unaffected

The test scene does not break `textured` or `tinted` materials. Run the main `config/app.jsonc` to confirm existing scenes still render correctly — the `dynamic_cast` guard ensures the lighting code is never invoked for non-lit materials.

---

## Checklist Against the Requirements

| Requirement | Implemented | Where |
|-------------|-------------|-------|
| Light component holds color, type, cone angles | ✅ | `light.hpp` |
| Position & direction derived from entity transform | ✅ | `forward-renderer.cpp` `sendLitUniforms` |
| Shader supports lighting (Phong model) | ✅ | `lit.vert` + `lit.frag` |
| Lit material with albedo, specular, roughness, AO, emissive maps | ✅ | `LitMaterial` in `material.hpp/cpp` |
| Forward renderer detects and sends lights | ✅ | `forward-renderer.cpp` |
| Multiple lights with different types/parameters | ✅ | `lights[]` uniform array, loop in `lit.frag` |
| Supports up to 16 lights simultaneously | ✅ | `MAX_LIGHT_COUNT 16` in `lit.frag` |
| Non-lit materials unaffected | ✅ | `dynamic_cast` guard in renderer |
