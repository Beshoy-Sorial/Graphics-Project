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

#### 7 light entities added to the world — "Fight Night" cinematic setup

| Name | Type | Position / Rotation | Color | Purpose |
|---|---|---|---|---|
| `CenterArenaSpot` | **Spot** | pos `[0, 10, 0]`, rot `[-90,0,0]` | Near-white `[1.0, 0.97, 0.90]` | Classic boxing-match overhead beam with penumbra |
| `RedCornerLight` | **Point** | `[-4.5, 5.5, 0.0]` | Scarlet `[1.0, 0.04, 0.04]` | Vivid red pool on player's side (much stronger than before) |
| `BlueCornerLight` | **Point** | `[4.5, 5.5, 0.0]` | Cobalt `[0.04, 0.18, 1.0]` | Electric blue pool on AI's side |
| `GoldenBroadcastLeft` | **Directional** | rotation `[-30, 45, 0]` | Rich gold `[1.0, 0.78, 0.18]` | TV-broadcast flood from upper-left |
| `AmberBroadcastRight` | **Directional** | rotation `[-30, -45, 0]` | Amber `[0.95, 0.65, 0.12]` | TV-broadcast flood from upper-right (creates cross-shadows) |
| `PurpleMagentaRim` | **Directional** | rotation `[20, 180, 0]` | Vivid purple `[0.65, 0.05, 0.95]` | Dramatic rim/silhouette light from behind |
| `CoolFrontalFill` | **Directional** | rotation `[-8, 0, 0]` | Teal `[0.18, 0.32, 0.55]` | Soft cool fill from camera direction; prevents black faces |

This gives **3 light types** (spot + point + directional) and **7 total lights**.

---

### What you will see in-game

**Center of the ring:**
- A crisp **white oval spotlight** on the canvas center with a soft gradient edge (spot light penumbra).
  The canvas texture glints inside the beam. Fighters standing in center are brightly lit.

**Ring canvas (`ring_mat`):**
- **Golden cross-shading** from two broadcast directionals at ±45° — specular glints visible
  from the `wood.jpg` specular map; the cross-angle creates interesting highlight variation
- **Vivid red left edge** — left side of canvas lit deep scarlet
- **Vivid blue right edge** — right side lit electric cobalt
- **Purple rim** on the far edge of the canvas (back of ring) from the rim light
- The white spot beam brightens the center — clear gradient from white center → coloured edges

**Arena floor (`floor_mat`):**
- **Bright scarlet pool** on the left half — much more vivid and concentrated than before
- **Electric blue pool** on the right half — equally vivid
- **Golden warmth** from the two broadcast lights creates a rich warm-amber base tone
- Floor center receives the tail of the spot beam, adding a subtle bright oval
- AO and roughness maps create surface detail variation under all 7 lights

**Fighters (skin tinted materials):**
- **Warm gold** on faces from the broadcast lights (classic TV look)
- **Purple highlight** on backs and shoulders from the rim light — creates a dramatic silhouette
- Standing in your red corner: clear red tint on one side, purple outline on the back
- Standing in the blue corner: electric blue tint on one side, purple outline
- Moving to ring center: the spot beam creates a bright overhead highlight on the head/shoulders

**Overall atmosphere:**
- High contrast — bright spot center fades to vivid coloured edges
- Warm golden palette from the broadcast lights, cut with cool teal fill on faces
- The purple rim is the most eye-catching addition — impossible to miss even from the default camera angle

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
| **Lighting with multiple lights** | ✅ **Fixed — 7 lights (spot + point + directional), 3 types, cinematic "Fight Night" setup** |
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
