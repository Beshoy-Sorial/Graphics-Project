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
| `PurpleMagentaRim` | **Directional** | Purple `[0.55, 0.04, 0.80]` | Dramatic silhouette rim on fighters' backs |
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

vec3 albedo = texture(material.albedo_tex, fs_in.tex_coord).rgb
              * fs_in.color.rgb
              * material_tint;   // ← arena colour applied here
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

```cpp
for (auto entity : world.getEntities()) {
    if (entity->name == "Ring" || entity->name == "Floor") {
        auto* mr = entity->getComponent<our::MeshRendererComponent>();
        if (mr) {
            auto* litMat = dynamic_cast<our::LitMaterial*>(mr->material);
            if (litMat) {
                litMat->tint = glm::vec3(tm.selectedArenaColor);
            }
        }
    }
}
```

### What you see now

Selecting any colour in the "Select Arena Color" screen visibly tints both the ring canvas
and the arena floor. Surface detail (grass pattern, ring canvas markings) remains visible
but takes on the chosen hue. The tint is applied per-frame so it works correctly across
all matches and after state transitions.

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

## Change 7 — Visual Fix: Audience + Arena Tint + Purple Rim

### Problem A — Audience looked psychedelic (vivid pink/purple arms)

**Root cause:** All `torso_*` and `skin` materials were converted to `LitMaterial` (Phong).
The ~450 audience body parts (150 spectators × 3 torso/arms) all responded to the
`PurpleMagentaRim` directional light at diffuse `[0.55, 0.04, 0.80]`. Raised audience
arms facing the light received full purple rim illumination → chaotic vivid colors.

**Fix — `config/app.jsonc`:**
- Added 9 audience-specific tinted materials: `aud_red`, `aud_blue`, `aud_green`,
  `aud_yellow`, `aud_purple`, `aud_cyan`, `aud_orange`, `aud_black`, `aud_skin`.
  These use the flat `tinted` shader — no Phong calculations, no response to arena lights.
- Reduced `PurpleMagentaRim` diffuse from `[0.55, 0.04, 0.80]` → `[0.25, 0.01, 0.35]`
  and specular from `[0.60, 0.08, 0.90]` → `[0.35, 0.05, 0.55]`. Still dramatic on
  fighters, but not psychedelic.

**Fix — `source/states/play-state.hpp`:**
- Audience torso/arm/leg materials changed from `torso_red` etc. → `aud_red` etc.
- Audience head material changed from `skin` → `aud_skin`.

### Problem B — Arena floor nearly black under dark tint (e.g. "Deep Blue")

**Root cause:** `material_tint` in `lit.frag` was multiplied directly into the albedo.
`Deep Blue` = `glm::vec3(0.1, 0.2, 0.5)` → albedo multiplied by max component `0.5`
→ floor rendered at ≤50% brightness. Very dark colours (anything with max < 0.3) made
the floor nearly invisible.

**Fix — `assets/shaders/lit.frag`:** Added tint normalization before albedo multiply:
```glsl
vec3 tintCol = material_tint;
float maxC = max(max(tintCol.r, tintCol.g), tintCol.b);
if(maxC > 0.001) tintCol /= maxC;          // normalize hue to max = 1
tintCol = mix(vec3(1.0), tintCol, 0.65);   // blend 65% hue + 35% white → never black
```
Now every color choice (Deep Blue, Dark Red, etc.) shows as a vivid visible tint instead
of a dark near-black stain. Surface detail (grass, ring markings) remains readable.

### Problem C — `skin_yellow` missing (stun flash broken)

**Root cause:** `skin_yellow` was never added when fighter materials were converted to
`LitMaterial`. The player-controller stun flash that swaps to `skin_yellow` returned
`nullptr` from `AssetLoader`.

**Fix — `config/app.jsonc`:** Added `skin_yellow` as a `LitMaterial` using `col_yellow`
albedo (already registered) and `spec_hi` / `rough_lo` for bright shiny appearance
matching the stun effect intent.

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
| `config/app.jsonc` | 7 lights (3 types), all materials → lit, audience tinted materials, `skin_yellow`, purple rim reduced |
| `assets/shaders/postprocess/warm-grade.frag` | **New** — Phase 2 postprocess effect |
| `assets/shaders/lit.frag` | Tint normalization (hue max=1, 65% blend); `material_tint` uniform for arena colour |
| `source/common/asset-loader.cpp` | Extended Texture2D loader with `color:R,G,B,A` inline format |
| `source/common/material/material.hpp` | Added `tint` field to `LitMaterial` |
| `source/common/material/material.cpp` | `LitMaterial::setup()` sends `material_tint`; null-shader guard in `Material::setup()` |
| `source/common/systems/forward-renderer.cpp` | Null-shader guard added to render loop skip condition |
| `source/common/systems/player-controller.hpp` | `resetCache()` method; `resultTimer` moved from static local to member variable |
| `source/states/play-state.hpp` | `resetCache()`, arena tint fix, audience now uses `aud_*` tinted materials |
