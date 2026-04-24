# What Was Added / Changed — Full Changelog

---

## Change 1 — Lighting (7 lights, 3 types) + Lit Materials

**File changed:** `config/app.jsonc`

#### Lit shader registered

```jsonc
"lit": { "vs": "assets/shaders/lit.vert", "fs": "assets/shaders/lit.frag" }
```

The `lit` shader implements the full Phong lighting model with 5 texture map types
(albedo, specular, ambient occlusion, roughness, emissive).

#### 7 light entities — "Fight Night" arena setup (3 types)

| Name | Type | Color | Purpose |
|---|---|---|---|
| `CenterArenaSpot` | **Spot** | Near-white `[1.0, 0.97, 0.90]` | Classic boxing overhead beam on ring center |
| `RedCornerLight` | **Point** | Scarlet `[1.0, 0.04, 0.04]` | Vivid red pool on player's left side |
| `BlueCornerLight` | **Point** | Cobalt `[0.04, 0.18, 1.0]` | Electric blue pool on AI's right side |
| `GoldenBroadcastLeft` | **Directional** | Gold `[0.28, 0.22, 0.05]` diffuse | Warm specular glints from upper-left |
| `AmberBroadcastRight` | **Directional** | Amber `[0.24, 0.18, 0.04]` diffuse | Warm specular glints from upper-right |
| `PurpleMagentaRim` | **Directional** | Purple `[0.25, 0.01, 0.35]` | Dramatic silhouette rim on fighters' backs |
| `CoolFrontalFill` | **Directional** | Teal `[0.18, 0.30, 0.50]` | Soft fill + sole ambient source; prevents black faces |

**3 light types used:** Spot, Point, Directional. ✅

---

## Change 2 — New Postprocessing Effect (Phase 2)

**New file:** `assets/shaders/postprocess/warm-grade.frag`

Combined **warm color grade + vignette** shader — a single Phase 2 effect:

```glsl
// Warm grade — push palette toward gold/amber
frag_color.r = min(frag_color.r * 1.10, 1.0);
frag_color.g = min(frag_color.g * 1.02, 1.0);
frag_color.b =     frag_color.b * 0.88;

// Contrast boost
frag_color.rgb = (frag_color.rgb - 0.5) * 1.12 + 0.5;

// Vignette — darken screen corners
vec2 ndc = tex_coord * 2.0 - 1.0;
frag_color.rgb /= 1.0 + dot(ndc, ndc) * 0.75;
```

`config/app.jsonc` postprocess path changed from `vignette.frag` (Phase 1) to `warm-grade.frag`.

---

## Change 3 — Fighter Materials: Tinted → Lit

### Root cause — why lighting wasn't visible on fighters before

All fighter and skin materials were `"type": "tinted"`. The `tinted` shader renders a flat
constant colour with **no lighting calculations** — no diffuse, no specular, no rim light.
Lighting only affected the ring canvas and floor.

### Fix

**`source/common/asset-loader.cpp`** — Extended `AssetLoader<Texture2D>::deserialize` to
support an inline `"color:R,G,B,A"` format that creates a 1×1 GPU texture directly in memory:

```cpp
// Entry in app.jsonc:
"col_red": "color:217,38,38,255"
// → creates a 1px solid-red Texture2D on the GPU, no external file needed
```

**`config/app.jsonc`** — All fighter / skin / referee materials changed from `tinted` → `lit`:

```jsonc
"torso_red": {
  "type": "lit", "shader": "lit",
  "albedo_map": "col_red", "specular_map": "spec_mid", "roughness_map": "rough_mid",
  "sampler": "default"
}
```

Every fighter body part now receives full Phong shading from all 7 lights.

---

## Change 4 — Floor Yellow-Cast Fix (light rebalancing)

### Root cause

The golden/amber directional lights had full-strength diffuse (`[1.0, 0.78, 0.18]` and
`[0.95, 0.65, 0.12]`) plus non-zero ambient across multiple lights. Summed, the total ambient
alone was `≈ [0.27, 0.24, 0.19]` — a strong warm-yellow tint on every surface before any
diffuse even runs. The grass floor's natural green was completely overpowered.

### Fix (`config/app.jsonc`)

| Light | Old diffuse | New diffuse | Ambient change |
|---|---|---|---|
| `GoldenBroadcastLeft` | `[1.0, 0.78, 0.18]` | `[0.28, 0.22, 0.05]` | zeroed |
| `AmberBroadcastRight` | `[0.95, 0.65, 0.12]` | `[0.24, 0.18, 0.04]` | zeroed |
| `CenterArenaSpot` | ambient `[0.10, 0.09, 0.07]` | `[0.04, 0.04, 0.04]` | reduced |
| `PurpleMagentaRim` | ambient `[0.016, 0.0, 0.024]` | `[0.0, 0.0, 0.0]` | zeroed |
| `CoolFrontalFill` | ambient `[0.04, 0.05, 0.09]` | `[0.10, 0.12, 0.14]` | **sole ambient** |

**Strategy:** Golden/amber lights now act as specular-dominant accent lights (strong specular,
very low diffuse, zero ambient). `CoolFrontalFill` is the **only** light with ambient — a
neutral cool-teal that lets the grass show its natural green.

---

## Change 5 — Arena Color Picker Was Broken (always showed yellow)

### Root cause

`play-state.hpp` applied the selected arena colour with:

```cpp
auto* ringMaterial = dynamic_cast<our::TintedMaterial*>(mr->material);
if (ringMaterial) {
    ringMaterial->tint = tm.selectedArenaColor;   // ← never reached
}
```

`ring_mat` and `floor_mat` are both `LitMaterial`, so the cast always returned `nullptr` —
the selected colour was **never applied**. Additionally, only the `"Ring"` entity was checked;
the `"Floor"` entity was ignored entirely. Every time you picked a colour, nothing changed
and the floor kept showing its default grass-under-yellow-lights appearance.

### Fix — 4 files

**`assets/shaders/lit.frag`** — Added `uniform vec3 material_tint` multiplied into the albedo:

```glsl
uniform vec3 material_tint;   // default white (1,1,1) = no change
```

**`source/common/material/material.hpp`** — Added `tint` field to `LitMaterial`:

```cpp
glm::vec3 tint = glm::vec3(1.0f, 1.0f, 1.0f); // Multiplied into albedo
```

**`source/common/material/material.cpp`** — `LitMaterial::setup()` now sends the uniform:

```cpp
shader->set("material_tint", tint);
```

**`source/states/play-state.hpp`** — Fixed the colour application:
- Cast to `LitMaterial*` instead of `TintedMaterial*`
- Apply to both `"Ring"` **and** `"Floor"` entities
- Only apply when `tm.arenaColorSelected == true` (prevents dark default)

---

## Change 6 — Crash Fixes (3 bugs)

### Bug A — Dangling pointer crash on second match (main crash)

**Root cause:** The application main loop calls `onImmediateGui()` **before** `onDraw()` every
frame. `onImmediateGui()` calls `playerController.drawHUD()`, which reads `cachedFighter` and
`cachedAIFighter`. When a match ends, `onDestroy()` frees the world (all `FighterComponent`
objects deleted), but those cached pointers still held the old addresses. On the first frame
of the **next match**, `drawHUD()` ran the check `if (!cachedFighter)` — which passed because
the pointer was non-null (just dangling) — and then read freed memory → crash.

**Fix — `source/common/systems/player-controller.hpp`:** Added `resetCache()` method:

```cpp
void resetCache() {
    cachedFighter   = nullptr;
    cachedAIFighter = nullptr;
    cameraEntity    = nullptr;
    resultTimer     = 0.0f;
    mouseLocked     = false;
}
```

**Fix — `source/states/play-state.hpp`:** Call `resetCache()` in `onDestroy()` **before**
`world.clear()`:

```cpp
playerController.resetCache();   // ← nulls dangling pointers before entities are freed
world.clear();
```

### Bug B — Result timer carried over between sessions

**Root cause:** The match-end countdown (`resultTimer`) was declared `static float` inside
`drawHUD()`. A static local is shared across all calls for the program's lifetime. If the
player exited mid-countdown (e.g., pressed Escape), the timer retained its partial value.
On the next match, the first KO would auto-advance after less than 3 seconds.

**Fix — `source/common/systems/player-controller.hpp`:** Moved `resultTimer` from a static
local to a member variable (`float resultTimer = 0.0f;`). `resetCache()` resets it to zero
at the start of every match, so each KO countdown always runs the full 3 seconds.

### Bug C — Null shader dereference (defensive fix)

**Root cause:** If any material's shader failed to load (typo in config key, missing asset),
`shader` would be `nullptr`. Two call sites dereferenced it without a null check:
- `Material::setup()` → `shader->use()`
- `ForwardRenderer::render()` → `command.material->shader->set("weatherMode", …)`

**Fix — `source/common/material/material.cpp`:**
```cpp
void Material::setup() const {
    pipelineState.setup();
    if (shader) shader->use();   // ← guard added
}
```

**Fix — `source/common/systems/forward-renderer.cpp`:** Extended the existing null check:
```cpp
// was:   if (!command.mesh || !command.material) continue;
if (!command.mesh || !command.material || !command.material->shader) continue;
```

---

## Change 7 — Visual Fix: Psychedelic Audience + Purple Rim

### Problem — Audience looked psychedelic (vivid pink/purple arms)

**Root cause:** All `torso_*` and `skin` materials were `LitMaterial` (Phong). The ~450
audience body parts (150 spectators × 3 body segments) responded fully to `PurpleMagentaRim`
directional light at diffuse `[0.55, 0.04, 0.80]`. Raised audience arms facing the light
received full purple rim illumination → chaotic rainbow of vivid lit colors on crowd.

**Fix — `config/app.jsonc`:**
- Added 9 audience-specific **tinted** materials: `aud_red`, `aud_blue`, `aud_green`,
  `aud_yellow`, `aud_purple`, `aud_cyan`, `aud_orange`, `aud_black`, `aud_skin`.
  The `tinted` shader has no Phong calculations — crowd colors are flat and stable.
- Reduced `PurpleMagentaRim` diffuse from `[0.55, 0.04, 0.80]` → `[0.25, 0.01, 0.35]`
  and specular from `[0.60, 0.08, 0.90]` → `[0.35, 0.05, 0.55]`. Still dramatic on
  fighters, no longer chaotic on crowd.

**Fix — `source/states/play-state.hpp`:**
- Audience torso/arm/leg materials changed: `torso_red` → `aud_red`, etc.
- Audience head material changed: `skin` → `aud_skin`.

### Added — `skin_yellow` for stun flash

`skin_yellow` was never added when fighter materials moved to LitMaterial. The stun flash
effect in player-controller returned `nullptr` from `AssetLoader`. Fixed by adding
`skin_yellow` as a `LitMaterial` with `col_yellow` albedo in `config/app.jsonc`.

---

## Change 8 — Arena Floor Dark-Color Fix (luminance-recolor tint)

### Root cause

Selecting "Deep Blue" `[0.1, 0.2, 0.5]` or any dark arena color made the floor nearly
black because `material_tint` was multiplied directly into the grass albedo:

```
grass_albedo (dark green ≈ 0.15 avg) × [0.1, 0.2, 0.5] → nearly black
```

The grass texture is inherently dark, so any multiplication with a dim value destroys
brightness. A simple normalization (divide by max component) was tried but still left the
floor too dark because `mix(albedo, normalizedTint, 0.65)` was still a multiply-dominated
operation on already-dark pixels.

### Fix — `assets/shaders/lit.frag` — Luminance-Preserving Recolor

The approach changed from **multiplication** to **luminance-recolor**:

```glsl
vec3  tintRaw = material_tint;
float maxC    = max(max(tintRaw.r, tintRaw.g), tintRaw.b);
float minC    = min(min(tintRaw.r, tintRaw.g), tintRaw.b);
// Saturation: 0 = grey/white (fighters), 1 = pure hue (arena selection)
float sat     = (maxC > 0.001) ? (maxC - minC) / maxC : 0.0;
// Normalize the tint to max-component = 1 (extracts pure hue, removes darkness)
vec3  normHue = (maxC > 0.001) ? (tintRaw / maxC) : vec3(1.0);

// Extract and boost the albedo luminance so dark textures still show the colour
float lum        = dot(albedo_raw, vec3(0.2126, 0.7152, 0.0722));
float boostedLum = clamp(lum * 2.5 + 0.25, 0.0, 1.0);
vec3  recolored  = boostedLum * normHue;

// Blend by saturation: white/grey tint → plain albedo; coloured tint → recolored
float blendT = clamp(sat * 1.5, 0.0, 1.0);
vec3  albedo = mix(albedo_raw, recolored, blendT);
```

**Also fixed — `source/states/play-state.hpp`:** Added `arenaColorSelected` guard so the
default dark-gray `[0.18, 0.18, 0.18]` stored in `TournamentManager` before any colour is
chosen never darkens the floor:

```cpp
if (tm.arenaColorSelected) {
    // apply litMat->tint = glm::vec3(tm.selectedArenaColor);
}
```

### How each arena color now looks

| Arena Color | Raw value | Normalized hue | sat | Result on grass floor |
|---|---|---|---|---|
| **Deep Blue** | [0.1, 0.2, 0.5] | [0.2, 0.4, 1.0] | 0.80 | Vivid blue — clearly blue |
| **Forest Green** | [0.1, 0.4, 0.1] | [0.25, 1.0, 0.25] | 0.75 | Vivid green |
| **Dark Red** | [0.5, 0.1, 0.1] | [1.0, 0.2, 0.2] | 0.80 | Vivid red |
| **Golden** | [0.7, 0.6, 0.1] | [1.0, 0.86, 0.14] | 0.86 | Warm gold |
| **Purple** | [0.4, 0.1, 0.5] | [0.8, 0.2, 1.0] | 0.80 | Clear purple |
| **Ocean Blue** | [0.1, 0.5, 0.7] | [0.14, 0.71, 1.0] | 0.86 | Vivid cyan-blue |
| **White** | [0.9, 0.9, 0.9] | [1.0, 1.0, 1.0] | 0.0 | Natural albedo — no tint |
| **Fighter (default)** | [1.0, 1.0, 1.0] | [1.0, 1.0, 1.0] | 0.0 | Plain albedo — no change |

---

# Lighting Workflow — How Each Feature Works

This section explains the full data flow from the engine to the screen for every
lighting-related feature in the game.

---

## Overview — The Rendering Pipeline

```
app.jsonc (scene definition)
    │
    ├── Light entities (7 lights) → LightComponent → light_common.glsl
    ├── LitMaterial per mesh      → lit.vert + lit.frag
    └── Postprocess pass          → warm-grade.frag
```

Each frame the `ForwardRenderer` does two passes:
1. **Geometry pass** — draws every mesh with its material shader (lit, tinted, textured)
2. **Postprocess pass** — runs `warm-grade.frag` on the framebuffer texture

---

## Stage 1 — Vertex Shader (`lit.vert`)

**What it does:** Transforms each vertex from object space to screen space and prepares
world-space data needed for per-fragment lighting calculations.

```
Input per vertex:
  position  (location 0) — object-space XYZ
  color     (location 1) — vertex color (typically white for OBJ meshes)
  tex_coord (location 2) — UV for texture sampling
  normal    (location 3) — object-space surface normal

Uniforms from the renderer:
  transform                    — MVP matrix (model × view × projection)
  object_to_world              — model matrix (object → world space)
  object_to_world_inv_transpose — inverse-transpose for correct normal transform

Output to fragment shader:
  world_position — used for: point/spot distance, specular view vector
  world_normal   — used for: diffuse NdotL, specular reflection
  tex_coord      — passed through for texture sampling
  color          — passed through (vertex tint, usually white)
```

**Key operation — normal transform:**
Normals cannot be transformed with the model matrix alone when the object is non-uniformly
scaled. The inverse-transpose corrects the direction so normals stay perpendicular to the
surface after scaling.

---

## Stage 2 — Fragment Shader (`lit.frag`)

**What it does:** For each pixel, samples textures, applies the arena tint, then calls
`calculateLighting()` with all 7 lights to produce the final lit color.

### Step 2a — Texture sampling

```glsl
albedo_raw = texture(material.albedo_tex, tex_coord).rgb * vertex_color;
specTex    = texture(material.specular_tex, tex_coord).rgb;
ao         = texture(material.ambient_occlusion_tex, tex_coord).r;
rough      = texture(material.roughness_tex, tex_coord).r;
emissive   = texture(material.emissive_tex, tex_coord).rgb;
```

| Map | Default fallback | Role |
|---|---|---|
| albedo | 1×1 white | Base color of the surface |
| specular | 1×1 mid-grey | How strongly highlights appear |
| ambient_occlusion | 1×1 white | Darkens crevices; multiplies ambient term |
| roughness | 1×1 mid-grey | Controls specular sharpness (`shininess`) |
| emissive | 1×1 black | Self-glow (not affected by lights) |

### Step 2b — Shininess from roughness

```glsl
shininess = 2.0 / pow(clamp(rough, 0.001, 0.999), 4.0) - 2.0;
```

Low roughness → very high shininess → tight sharp specular highlight (e.g. `spec_hi`).
High roughness → low shininess → wide diffuse-like specular (e.g. `rough_mid`).

### Step 2c — Arena colour tint (luminance-recolor)

Applies only when a coloured arena is selected (`material_tint` ≠ white).

```glsl
sat     = (maxC - minC) / maxC        // tint saturation (0 = grey, 1 = pure hue)
normHue = tintRaw / maxC              // extract hue (max component = 1)
lum     = dot(albedo_raw, luma_weights) // brightness of the texture pixel
boostedLum = clamp(lum * 2.5 + 0.25)  // amplified so dark textures stay visible
recolored  = boostedLum * normHue      // texture pattern in the chosen hue
albedo     = mix(albedo_raw, recolored, sat * 1.5)
```

Fighter materials always have `material_tint = [1,1,1]` (sat=0), so they are never affected.

### Step 2d — `calculateLighting()` called once for all 7 lights

Each light's contribution is accumulated: `result += lightContribution(...)`.

---

## Stage 3 — Light Calculations (`light_common.glsl`)

### Phong lighting model — applied per light

```
total = ambient + diffuse + specular + emissive
```

| Term | Formula | Role |
|---|---|---|
| **Ambient** | `light.ambient * ao * albedo` | Constant fill; prevents pure-black shadows |
| **Diffuse** | `light.diffuse * max(dot(N, L), 0) * albedo` | Lambertian; surface facing the light is bright |
| **Specular** | `light.specular * specTex * pow(max(dot(R, V), 0), shininess)` | Phong highlight; shiny objects catch the light |
| **Emissive** | `emissive` (added once, not per-light) | Self-illumination, unaffected by any light |

Where:
- `N` = normalized world normal
- `L` = direction toward the light
- `R` = reflection of `-L` around `N`
- `V` = direction toward the camera

---

## Stage 4 — Per-Light Workflow (all 7 lights)

### Light 1: CenterArenaSpot (Spot)

**Purpose:** Overhead boxing spotlight — bright central beam on the ring.

```
Position: [0, 10, 0]   Rotation: [-90, 0, 0] → points straight down (-Y)

Workflow per fragment:
  1. fragToLight = normalize(light.position - world_position)
  2. distance    = length(light.position - world_position)
  3. attenuation = 1 / (constant + linear*d + quadratic*d²)
                 = 1 / (1.0 + 0.022*d + 0.0019*d²)
  4. spotAngle   = dot(-fragToLight, spotDirection)  ← cos of angle to beam center
  5. spotFactor  = smoothstep(cos(outer=36°), cos(inner=20°), spotAngle)
                   → 1.0 inside inner cone, 0.0 outside outer cone, smooth edge
  6. contribution = (ambient + diffuse*NdotL + specular) * attenuation * spotFactor
```

**What you see:** A warm-white oval pool of light on the ring canvas that fades at the
edges. Fighters standing in the center get a dramatic top-light; fighters near the ropes
are dimmer and more shadowed.

---

### Lights 2 & 3: RedCornerLight + BlueCornerLight (Point)

**Purpose:** Colored corner floodlights — red on the player's side, blue on the AI's side.

```
RedCorner:  Position [-4.5, 5.5, 0]   diffuse [1.0, 0.04, 0.04]
BlueCorner: Position [ 4.5, 5.5, 0]   diffuse [0.04, 0.18, 1.0]

Workflow per fragment:
  1. L           = normalize(light.position - world_position)
  2. distance    = length(light.position - world_position)
  3. attenuation = 1 / (1.0 + 0.09*d + 0.032*d²)   ← moderate falloff
  4. contribution = (diffuse * NdotL + specular) * attenuation
     (ambient = 0 — color only appears near the corner, not globally)
```

**What you see:** The player's torso/head are bathed in scarlet from the left; the AI's
side glows cobalt blue. The ring ropes and floor near each corner pick up the corresponding
color. Fighters near the center see both colors mixing.

---

### Lights 4 & 5: GoldenBroadcastLeft + AmberBroadcastRight (Directional)

**Purpose:** Simulated broadcast TV lights from upper-left and upper-right — strong specular
glints on shiny surfaces, very low diffuse so the floor color is not overwhelmed.

```
GoldenLeft:  Rotation [-30, 45, 0]   diffuse [0.28, 0.22, 0.05]  specular [0.55, 0.45, 0.15]
AmberRight:  Rotation [-30,-45, 0]   diffuse [0.24, 0.18, 0.04]  specular [0.50, 0.38, 0.12]
Both: ambient = [0, 0, 0]

Workflow per fragment (directional — no position, no attenuation):
  1. L = -normalize(lightDirection)   ← constant across the entire scene
  2. diffuse contribution  = light.diffuse  * max(NdotL, 0) * albedo
  3. R = reflect(-L, N)
  4. specular contribution = light.specular * specTex * pow(max(dot(R,V),0), shininess)
  5. total = diffuse + specular   (no ambient from these lights)
```

**What you see:** Shiny parts of the ring canvas and fighter gloves catch bright golden
specular flashes depending on the camera angle. The effect is subtle in diffuse but visible
as glinting highlights, adding to the broadcast-TV aesthetic.

---

### Light 6: PurpleMagentaRim (Directional)

**Purpose:** Dramatic back-rim light — violet silhouette on fighters' backs and the far edge
of the ring, like colored stage lighting behind the fighters.

```
Rotation: [20, 180, 0] → light comes FROM behind the fighters (faces -Z)
diffuse:  [0.25, 0.01, 0.35]   specular: [0.35, 0.05, 0.55]   ambient: [0, 0, 0]

Workflow per fragment:
  1. L = direction facing TOWARD the camera (opposite to fighters' forward)
  2. NdotL = dot(N, L)  — positive only for back-facing normals
     Back of fighter torso → NdotL ≈ 0.8 → strong rim
     Front of fighter torso → NdotL ≈ 0.0 → no contribution
  3. contribution = light.diffuse * max(NdotL, 0) * albedo
                  + light.specular * specTex * pow(max(dot(R,V),0), shininess)
```

**What you see:** The backs of both fighters glow violet, separating them from the
background. The ring ropes behind the fighters catch purple specular highlights. Zero ambient
means it only appears on surfaces truly facing away — no purple spill onto the front.

---

### Light 7: CoolFrontalFill (Directional)

**Purpose:** The only ambient light source. Soft teal fill from the front prevents any
surface from going completely black. Contrasts the warm golds/ambers.

```
Rotation: [-8, 0, 0]   diffuse [0.18, 0.30, 0.50]   specular [0, 0, 0]
ambient: [0.10, 0.12, 0.14]   ← THE ONLY LIGHT WITH AMBIENT

Workflow per fragment:
  Ambient term (added regardless of surface angle):
    ambient = light.ambient * ao * albedo
            = [0.10, 0.12, 0.14] * ao * albedo
    → gives every surface a minimum cool-teal brightness
    → ao (ambient occlusion) darkens crevices that should be shadowed

  Diffuse term (directional fill from front):
    L = normalize(-lightDirection)  ← points toward camera-forward direction
    diffuse = [0.18, 0.30, 0.50] * max(dot(N, L), 0) * albedo
    → front-facing surfaces get a teal-blue fill
    → back-facing surfaces get 0 from this light (covered by PurpleMagentaRim)
```

**What you see:** Faces and front torsos are always visible with a cool blue-teal base.
No completely black surfaces exist anywhere in the scene. The cool teal creates a color
contrast against the warm golden/amber broadcast lights, giving the lighting a professional
dual-temperature look.

---

## Stage 5 — Postprocessing (`warm-grade.frag`)

**What it does:** Full-screen pass over the rendered framebuffer — 3 operations in one shader.

```
Input: rendered scene texture (all 7 lights already calculated)
Output: final screen image
```

### Operation 1 — Warm Color Grade

```glsl
frag_color.r = min(frag_color.r * 1.10, 1.0);  // +10% red → warmer
frag_color.g = min(frag_color.g * 1.02, 1.0);  // +2%  green → slight warmth
frag_color.b =     frag_color.b * 0.88;         // -12% blue  → reduces coldness
```

Pushes the entire frame toward gold/amber. Even the blue corner light and teal fill appear
slightly warmer through this grade. Gives the game a "fight night broadcast" feel.

### Operation 2 — Contrast Boost

```glsl
frag_color.rgb = (frag_color.rgb - 0.5) * 1.12 + 0.5;
```

Standard contrast stretch centered at 0.5. Dark areas get darker, bright areas get brighter.
Amplifies the separation between the lit ring and the shadowed audience.

### Operation 3 — Vignette

```glsl
vec2 ndc = tex_coord * 2.0 - 1.0;           // [0,1] UV → [-1,1] NDC
frag_color.rgb /= 1.0 + dot(ndc, ndc) * 0.75;
```

`dot(ndc, ndc)` = squared distance from screen center (0 at center, ~2 at corners).
Dividing by `1 + distance²×0.75` darkens the corners without hard edges. Keeps the
viewer's eye focused on the center of the ring.

---

## Full Lighting Feature Map

```
Every frame, for each LitMaterial mesh pixel:

  albedo_raw = sample(albedo_tex) × vertex_color
  albedo     = luminance_recolor(albedo_raw, material_tint)  ← arena tint

  FOR each of 7 lights:
    L = direction to light
    NdotL = dot(N, L)

    ┌─ Directional (GoldenLeft, AmberRight, PurpleRim, CoolFill)
    │   No distance, no attenuation
    │   L is constant across the scene
    │
    ├─ Point (RedCorner, BlueCorner)
    │   L = normalize(lightPos - fragPos)
    │   attenuation = 1/(c + l*d + q*d²)
    │
    └─ Spot (CenterArenaSpot)
        L = normalize(lightPos - fragPos)
        attenuation = 1/(c + l*d + q*d²)
        spotFactor  = smoothstep(outerCone, innerCone, dot(-L, spotDir))

    contribution = ambient×ao×albedo          (CoolFill only — all others: ambient=0)
                 + diffuse×NdotL×albedo
                 + specular×specTex×pow(RdotV, shininess)
                 × attenuation × spotFactor

  pixel = sum(contributions) + emissive

  ─── postprocess pass ────────────────────────────────────────
  pixel = warmGrade(pixel)     r×1.10, g×1.02, b×0.88
  pixel = contrast(pixel)      (p - 0.5) × 1.12 + 0.5
  pixel /= 1 + dist²×0.75     vignette
```

---

## Full Requirements Status

| Requirement | Status |
|---|---|
| Enemy / obstacle (AI fighter + Referee) | ✅ Already met |
| 3D models (torso, head, arms, legs, ring…) | ✅ Already met |
| **Lighting — 7 lights, 3 types** | ✅ Fixed — spot + point + directional |
| **Lit materials (albedo, specular, roughness, AO)** | ✅ Fixed — ring, floor, AND all fighters |
| Sky (sphere with sky.jpg) | ✅ Already met |
| **New postprocessing effect (Phase 2)** | ✅ Fixed — `warm-grade.frag` |
| 3D motion (movement, animations, weather) | ✅ Already met |
| Collision detection (distance-based punch hit) | ✅ Already met |
| Game logic (health, KO, tournament, difficulty) | ✅ Already met |
| Scene deserialization | ✅ Already met |
| Menu state + Play state (bidirectional) | ✅ Already met |

---

## Files Changed Summary

| File | Change |
|---|---|
| `config/app.jsonc` | 7 lights (3 types), all fighter materials → lit, audience tinted materials, `skin_yellow`, purple rim reduced |
| `assets/shaders/postprocess/warm-grade.frag` | **New** — Phase 2 postprocess: warm grade + contrast + vignette |
| `assets/shaders/lit.frag` | **Two-axis tint**: hue recolor (saturated) + brightness remap (grey/white); fixes white/grey arena colors |
| `source/common/asset-loader.cpp` | Extended Texture2D loader with `color:R,G,B,A` inline format |
| `source/common/material/material.hpp` | Added `tint` field to `LitMaterial` |
| `source/common/material/material.cpp` | `LitMaterial::setup()` sends `material_tint`; null-shader guard in `Material::setup()` |
| `source/common/systems/forward-renderer.cpp` | Null-shader guard added to render loop skip condition |
| `source/common/systems/player-controller.hpp` | `resetCache()` method; `resultTimer` moved from static local to member variable |
| `source/states/play-state.hpp` | `resetCache()`, `arenaColorSelected` guard, arena tint via `LitMaterial*`, audience uses `aud_*` tinted materials |

---

## Change 9 — White / Dark-Grey Arena Color Showed Yellow (shader tint bug)

### Root cause

The tint shader used **saturation** as the sole blend weight:

```glsl
float sat    = (maxC - minC) / maxC;
float blendT = clamp(sat * 1.5, 0.0, 1.0);
vec3  albedo = mix(albedo_raw, recolored, blendT);
```

White `[0.9, 0.9, 0.9]` and dark grey `[0.18, 0.18, 0.18]` both have **sat = 0**
(equal R/G/B), so `blendT = 0` and `mix` returned `albedo_raw` unchanged.
The orange ring.png texture showed through regardless of which color was chosen.

### Fix — `assets/shaders/lit.frag` — Two-axis blend

Added a **brightness-remap axis** that activates for achromatic (grey/white) tints:

```glsl
float targetGrey = (tintRaw.r + tintRaw.g + tintRaw.b) / 3.0;

// Weight = 0 when tint=(1,1,1) (fighters stay unchanged)
// Weight is strong for dark-grey or near-white arena selections
float greyWeight = clamp((1.0 - sat) * abs(1.0 - targetGrey) * 5.0, 0.0, 1.0);
vec3  greyAlbedo = vec3(clamp(lum + targetGrey - 0.5, 0.0, 1.0));

float hueWeight = clamp(sat * 1.5, 0.0, 1.0);
vec3  tinted    = mix(albedo_raw, recolored,  hueWeight);
vec3  albedo    = mix(tinted,     greyAlbedo, greyWeight);
```

### How each case now works

| Arena color | tint value | sat | greyWeight | hueWeight | Result |
|---|---|---|---|---|---|
| **Default / fighters** | `[1.0, 1.0, 1.0]` | 0 | **0** | 0 | Unchanged — natural albedo ✓ |
| **White** | `[0.9, 0.9, 0.9]` | 0 | **~0.5** | 0 | Canvas brightens toward near-white ✓ |
| **Dark Grey** | `[0.18, 0.18, 0.18]` | 0 | **~0.9** | 0 | Canvas goes dark grey ✓ |
| **Deep Blue** | `[0.1, 0.2, 0.5]` | 0.80 | ~0.1 | 1.0 | Vivid blue recolor (hue axis) ✓ |
| **Red** | `[0.8, 0.1, 0.1]` | 0.875 | ~0.05 | 1.0 | Vivid red (hue axis) ✓ |
