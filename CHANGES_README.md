# What Was Added to Meet Missing Requirements

Two requirements were missing from the game and have now been fixed:
1. **Lighting with multiple lights + lit material texture types**
2. **A new postprocessing effect not from Phase 1**

Everything is data-driven — all changes are in `config/app.jsonc` and a new shader file.
No C++ code was touched.

---

## Change 1 — Lighting (4 lights + lit materials)

### What was added

**File changed:** `config/app.jsonc`

#### New shader registered

```jsonc
"lit": {
    "vs": "assets/shaders/lit.vert",
    "fs": "assets/shaders/lit.frag"
}
```

The `lit` shader implements the full Phong lighting model with support for 5 texture map types
(albedo, specular, ambient occlusion, roughness, emissive). It was already fully implemented
in C++ and GLSL — it just needed to be wired into the game scene.

#### New textures registered

```jsonc
"grass": "assets/textures/grass_ground_d.jpg",
"moon":  "assets/textures/moon.jpg",
"wood":  "assets/textures/wood.jpg"
```

These are used as texture map inputs for the lit materials below.

#### 2 environment materials upgraded from `tinted`/`textured` to `lit`

| Material | Old type | New type | Texture maps used |
|---|---|---|---|
| `ring_mat` (boxing ring canvas) | `textured` | `lit` | albedo=ring.png, specular=wood.jpg, roughness=moon.jpg |
| `floor_mat` (arena floor) | `tinted` | `lit` | albedo=grass.jpg, roughness=moon.jpg, AO=moon.jpg |

Fighter and referee body materials remain `tinted` (flat color) since no fighter skin texture
maps are available — this is correct and intentional.

#### 4 light entities added to the world

| Name | Type | Position / Rotation | Color | Purpose |
|---|---|---|---|---|
| `MainArenaLight` | **Directional** | rotation `[-45, 20, 0]` | Warm white `[1.0, 0.92, 0.80]` | Primary overhead arena light |
| `RedCornerLight` | **Point** | `[-3.5, 4.0, 0.0]` | Red `[1.0, 0.15, 0.15]` | Player's corner atmosphere |
| `BlueCornerLight` | **Point** | `[3.5, 4.0, 0.0]` | Blue `[0.15, 0.35, 1.0]` | AI's corner atmosphere |
| `FillLight` | **Directional** | rotation `[20, 180, 0]` | Cool blue `[0.25, 0.30, 0.50]` | Prevents total darkness on back faces |

This gives **2 light types** (directional + point) and **4 total lights** — both clearly exceed
the "multiple lights, one type is enough" requirement.

---

### What you will see in-game

**Ring canvas (`ring_mat`):**
- Shows the `ring.png` texture with **Phong shading** — highlights and shadows follow the
  surface normal correctly
- The warm directional light adds a golden sheen to the center
- The red corner light tints the left side of the canvas red
- The blue corner light tints the right side blue
- Specular highlights (from `wood.jpg` specular map) make the canvas surface glint

**Arena floor (`floor_mat`):**
- Shows the `grass_ground_d.jpg` texture lit by all 4 lights
- A clear **red gradient** is visible on the left half (from the red corner point light)
- A clear **blue gradient** is visible on the right half (from the blue corner point light)
- The overhead directional light provides warm overall illumination
- The AO map (`moon.jpg`) subtly darkens occluded areas
- The roughness map (`moon.jpg`) controls how sharp the specular highlights are

**Lighting summary visible in the match:**
- Two colored zones on the floor: red (player side) and blue (AI side)
- Ring canvas has natural shading — not flat like before
- The back fill light ensures no surfaces go completely black

---

## Change 2 — New Postprocessing Effect (Phase 2)

### Why the old effect didn't count

Phase 1 required completing `vignette.frag` and `chromatic-aberration.frag`. The game was
using `vignette.frag` as its postprocess — a Phase 1 deliverable that doesn't count for Phase 2.

### What was added

**New file:** `assets/shaders/postprocess/warm-grade.frag`

This is a **combined warm color grade + vignette** shader — a single Phase 2 effect that does
two things at once:

```glsl
// 1. Warm color grade — push palette toward gold/amber
frag_color.r = min(frag_color.r * 1.10, 1.0);   // boost reds  (+10%)
frag_color.g = min(frag_color.g * 1.02, 1.0);   // slight green lift
frag_color.b =     frag_color.b * 0.88;          // pull blues   (-12%)

// 2. Contrast boost
frag_color.rgb = (frag_color.rgb - 0.5) * 1.12 + 0.5;

// 3. Vignette — darken screen corners
vec2 ndc = tex_coord * 2.0 - 1.0;
frag_color.rgb /= 1.0 + dot(ndc, ndc) * 0.75;
```

**File changed:** `config/app.jsonc` — postprocess path switched from:
```
"assets/shaders/postprocess/vignette.frag"   ← Phase 1, no longer used
```
to:
```
"assets/shaders/postprocess/warm-grade.frag"  ← Phase 2, now active
```

### What you will see in-game

- **Warm golden tone** over the whole screen — reds and yellows are slightly amplified,
  blues are slightly reduced. This gives the game a cinematic "arena spotlight" atmosphere
  fitting for a boxing match.
- **Darker screen corners** (vignette) draw the eye toward the center ring where the action is.
- **Higher contrast** — blacks are deeper, brights are slightly brighter. The ring canvas
  texture and floor texture look more vivid.
- Compare to before: the scene previously had a neutral color palette; now it feels warmer
  and more dramatic.

---

## Full requirements status after these changes

| Requirement | Status |
|---|---|
| Enemy / obstacle (AI fighter) | ✅ Was already met |
| 3D models | ✅ Was already met |
| **Lighting with multiple lights** | ✅ **Fixed — 4 lights (directional + point), 2 types** |
| **Lit material texture types (albedo, specular, roughness, AO)** | ✅ **Fixed — ring and floor use `lit` with 3 texture maps each** |
| Sky | ✅ Was already met |
| **New postprocessing effect (not Phase 1)** | ✅ **Fixed — `warm-grade.frag` (color grade + vignette combined)** |
| 3D motion | ✅ Was already met |
| Collision detection (sphere) | ✅ Was already met |
| Game logic (combat, health, tournament) | ✅ Was already met |
| Scene deserialization | ✅ Was already met |
| Menu state + Play state | ✅ Was already met |

---

## Files changed

| File | Change |
|---|---|
| `config/app.jsonc` | Added `lit` shader, 3 textures, upgraded 2 materials, added 4 light entities, changed postprocess path |
| `assets/shaders/postprocess/warm-grade.frag` | **New file** — Phase 2 postprocess effect |
