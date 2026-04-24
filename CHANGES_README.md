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
| `config/app.jsonc` | 7 lights (3 types), all materials → lit, inline colour textures, floor yellow-cast fix |
| `assets/shaders/postprocess/warm-grade.frag` | **New** — Phase 2 postprocess effect |
| `source/common/asset-loader.cpp` | Extended Texture2D loader with `color:R,G,B,A` inline format |
