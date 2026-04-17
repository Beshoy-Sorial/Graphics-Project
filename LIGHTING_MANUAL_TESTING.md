# Requirement 1: Lighting — Manual Testing Guide

> Run every test from the **project root** (`Graphics-Project/`).  
> The window opens and stays open until you close it — use `WASD` + mouse to move the camera.

---

## How to run a test

```bash
./bin/GAME_APPLICATION.exe -c <config-file>
```

To auto-close after N frames:

```bash
./bin/GAME_APPLICATION.exe -c <config-file> -f 60
```

---

## Rotation convention — read this before writing spot lights

The engine builds rotation matrices with `glm::yawPitchRoll(rotation.y, rotation.x, rotation.z)`.  
With this convention, **pitch = +90°** rotates the local -Z forward axis to point **UP**, not down.

| `"rotation"` in JSON | Local -Z direction in world | Use for |
|---|---|---|
| `[-90, 0, 0]` | World **-Y** (downward) ✓ | Ceiling/overhead spot lights |
| `[90, 0, 0]` | World **+Y** (upward) | Upward-pointing lights |
| `[0, 90, 0]` | World **-X** (left) | Side lights |
| `[0, 0, 0]` | World **-Z** (forward) | Lights pointing into the scene |

---

## Test 0 — Full scene: all 3 light types + all 5 texture maps

```bash
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-0.jsonc
```

### What is in the scene

| Object | Position | Purpose |
|--------|----------|---------|
| Monkey | center | Tests full monkey-texture albedo + specular + roughness |
| Sphere | right | Tests specular variation (color-grid as specular map) |
| Cube | left, scale `[3,1,1]` | Tests normal correctness under non-uniform scale |
| Glowing sphere | back | Tests emissive map (smile pattern visible even in shadow) |
| Ground plane | below all | Tests AO and roughness maps |
| Point light | `[5,5,5]` | White, quadratic attenuation |
| Directional light | `rotation [45,-30,0]` | Cool blue fill, no attenuation |
| Spot light | `[0,6,0]`, `rotation [-90,0,0]` | Red cone pointing **down** onto ground |

### What to look for ✅

**Point light:**
- Objects closer to `[5,5,5]` are brighter
- Surfaces facing away from that corner are darker
- Small specular highlight visible on the sphere and monkey

**Directional light:**
- Flat blue tint on all surfaces facing upper-left
- Same intensity regardless of distance from the light entity (no falloff)

**Spot light (red, pointing down):**
- Red/pinkish circle visible on the ground below `[0,6,0]`
- Soft transition between the bright center and the dark edge (smoothstep, not a hard cutoff)
- Objects outside the cone receive zero red contribution

**Texture maps:**
- Specular map (`color-grid`) makes different checkerboard squares shine differently
- Roughness map (`moon`) creates varied shininess across the sphere surface
- Emissive sphere in the back shows the smile pattern glowing even in shadow

**Non-uniform scale normals:**
- Move camera to view the left cube (`scale [3,1,1]`) from the side
- Specular highlight sits correctly on the face closest to the light
- If highlight slides to a wrong face → normal transformation is broken (passing = highlight stays correct)

---

## Test 1 — Emissive only: NO lights in scene

```bash
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-1.jsonc
```

### What is in the scene

| Object | Material | Expected result |
|--------|----------|-----------------|
| Left sphere | `emissive_map = smile` | Smile pattern glows with no light source |
| Right sphere | `albedo_map = wood`, no emissive | Completely black |

### What to look for ✅

- **Left sphere** shows the smile texture pattern in color/white — visible with zero lights
- **Right sphere** is completely black (no lights, no emission, no ambient)
- Proves: `result = emissive` is the starting value before the light loop — emission is independent of lights

### What failure looks like ✗

- Both spheres black → emissive is not added to `result`
- Both spheres lit → ambient from lights is leaking (there are no light entities)

---

## Test 2 — Normal correctness under non-uniform scale

```bash
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-2.jsonc
```

### What is in the scene

| Object | Scale | Position |
|--------|-------|----------|
| Center sphere | `[1,1,1]` uniform | reference |
| Left sphere | `[3,1,1]` stretched wide | non-uniform X |
| Right sphere | `[1,3,1]` stretched tall | non-uniform Y |

Single strong point light at `[3, 4, 4]`.

### What to look for ✅

- **All three spheres** have specular highlights at the geometrically correct position (upper-right, facing the light)
- Left wide sphere: the highlight follows the actual ellipsoid curvature, not skewed by the X scale
- Right tall sphere: same — highlight is perpendicular to the stretched surface
- Move the camera around each sphere — the highlight tracks the correct reflection angle

### What failure looks like ✗

Replace `object_to_world_inv_transpose` with plain `object_to_world` in `lit.vert` — the highlight on the stretched spheres will visibly float to the wrong face.

---

## Test 3 — RGB color mixing (3 colored point lights)

```bash
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-3.jsonc
```

### What is in the scene

- Large white sphere in center
- Ground plane
- **Red** point light at left (`[-4, 1, 2]`)
- **Green** point light at right (`[4, 1, 2]`)
- **Blue** point light at top (`[0, 5, 0]`)

### What to look for ✅

- **Left side** of sphere: red
- **Right side** of sphere: green
- **Top** of sphere: blue
- **Left-right boundary** (bottom of sphere): yellow = red + green mixed
- **Top-left boundary**: magenta = red + blue mixed
- **Top-right boundary**: cyan = green + blue mixed
- Floor also picks up each color in the corresponding zone
- Confirms: each light's contribution is **additively accumulated** in the loop

---

## Test 4 — Spot light cone edges (tight vs wide)

```bash
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-4.jsonc
```

### What is in the scene

- Ground plane
- Two spheres (one under each spot)
- **Tight spot** at `[-4, 7, -2]`, `rotation [-90,0,0]` — yellow-white, `innerCone=10°`, `outerCone=20°`
- **Wide spot** at `[4, 7, -2]`, `rotation [-90,0,0]` — blue, `innerCone=30°`, `outerCone=50°`

Both spots point straight down.

### What to look for ✅

**Left sphere (tight spot):**
- Small, bright, concentrated yellow-white highlight on the top
- The bright area is narrow — most of the sphere is dark

**Right sphere (wide spot):**
- Broad, softer blue illumination covering a larger portion of the sphere
- The lit area transitions gradually to dark (smoothstep gradient)

**Floor:**
- Tight spot creates a small circle of yellow-white light on the floor
- Wide spot creates a much larger circle of blue light with a soft outer edge

**Cone edge quality:**
- No hard pixel-sharp cutoff line — the boundary fades smoothly (smoothstep)
- `smoothstep(outerCutoff, innerCutoff, cosTheta)` produces the gradient

---

## Test 5 — Point light attenuation over distance

```bash
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-5.jsonc
```

### What is in the scene

- Single point light at `[-2, 1, 0]` with strong attenuation (`Kc=1, Kl=0.22, Kq=0.20`)
- Four **identical** spheres at x = -1, 2, 6, 11 (distances ≈ 1.4, 4.1, 8.1, 13.0 units from light)
- Zero ambient so falloff is visible without noise

### Expected attenuation at each sphere

| Sphere | Distance | Attenuation | Expected brightness |
|--------|----------|-------------|---------------------|
| 1 | 1.4 | ~0.58 | Clearly visible |
| 2 | 4.1 | ~0.19 | Noticeably dimmer |
| 3 | 8.1 | ~0.06 | Very dim |
| 4 | 13.0 | ~0.03 | Near black |

### What to look for ✅

- Brightness decreases left to right across the four spheres
- The dropoff is rapid (quadratic) — sphere 2 is much dimmer than sphere 1, not just slightly
- All four spheres have **identical material** — any brightness difference is purely from the `1/(Kc + Kl·d + Kq·d²)` formula
- Confirms: distance-based attenuation is computed per-fragment, not per-object

---

## Test 6 — Directional light in isolation

```bash
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-6.jsonc
```

### What is in the scene

- Three spheres at very different Z depths: `z=0`, `z=-5`, `z=-12`
- Ground plane
- Single directional light with `rotation [-30, 45, 0]` (upper-left-front direction)
- **No point or spot lights**

### What to look for ✅

- **All three spheres look identical** — same highlight position, same brightness, same shading gradient
- The sphere at `z=-12` is NOT darker than the one at `z=0` — directional lights have **no attenuation**
- The specular highlight appears in the same relative position (upper-left) on all three spheres
- Proves: directional light uses a constant `L = normalize(-light.direction)` with `attenuation = 1.0`

### What failure looks like ✗

If directional light accidentally went through the point-light attenuation code, the far sphere would appear much darker — that would be wrong.

---

## Spot light cone tweaks — try live in test-0

Open `config/lighting-test/test-0.jsonc` in a text editor, change the spot values, and re-run:

### Narrow cone (near-laser beam)

```jsonc
"innerConeAngle": 2.0,
"outerConeAngle": 5.0
```

Expected: tiny sharp red dot on the ground.

### Wide cone (floodlight)

```jsonc
"innerConeAngle": 40.0,
"outerConeAngle": 60.0
```

Expected: large soft red circle covering most of the ground.

### Hard edge (no transition zone)

```jsonc
"innerConeAngle": 25.0,
"outerConeAngle": 25.5
```

Expected: hard sharp cutoff edge (inner ≈ outer → `smoothstep` range ≈ 0).

---

## Attenuation tweaks — try live in test-0

### No attenuation (constant, infinite range)

```jsonc
"attenuationConstant":  1.0,
"attenuationLinear":    0.0,
"attenuationQuadratic": 0.0
```

Expected: point light same intensity at all distances.

### Strong attenuation (very short range)

```jsonc
"attenuationConstant":  1.0,
"attenuationLinear":    0.7,
"attenuationQuadratic": 1.8
```

Expected: light barely reaches the monkey — only surfaces very close to `[5,5,5]` are bright.

---

## Multiple lights — add up to 16

Copy and paste extra light entries into any scene's `"world"` array. The system supports up to **16 lights** (`MAX_LIGHT_COUNT` in `light_common.glsl`).

Example: add a green point light on the opposite side to test-0:

```jsonc
{
    "position": [-5, 3, 3],
    "components": [{
        "type": "Light",
        "lightType": "point",
        "diffuse":  [0.0, 1.0, 0.0],
        "specular": [0.0, 1.0, 0.0],
        "ambient":  [0.0, 0.05, 0.0],
        "attenuationConstant":  1.0,
        "attenuationLinear":    0.09,
        "attenuationQuadratic": 0.032
    }]
}
```

Expected: monkey/sphere left side turns green; right side stays white from the original point light. Both lights are visible simultaneously — Phong accumulation works.

---

## Camera controls (in-window)

| Key / Mouse | Action |
|-------------|--------|
| `W` `A` `S` `D` | Move forward/left/back/right |
| `Q` `E` | Move down / up |
| Hold right mouse button + drag | Rotate camera |
| `Shift` | Move faster |

---

## Quick verification checklist

```bash
# All 3 light types + all 5 maps
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-0.jsonc

# Emissive only (no lights)
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-1.jsonc

# Normal correctness (non-uniform scale)
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-2.jsonc

# RGB color mixing (multi-light accumulation)
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-3.jsonc

# Spot cone edges (tight vs wide)
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-4.jsonc

# Attenuation distance falloff
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-5.jsonc

# Directional light (no attenuation, distance-independent)
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-6.jsonc
```

| Test | Pass condition |
|------|----------------|
| test-0 | Red spot cone on ground, blue directional tint, specular varies per color-grid square |
| test-1 | Left sphere shows smile glow; right sphere is completely black |
| test-2 | Specular highlight sits correctly on all 3 non-uniform-scaled spheres |
| test-3 | Red/green/blue sides on sphere; yellow/cyan/magenta where two lights overlap |
| test-4 | Tight spot → small concentrated highlight; wide spot → broad soft halo |
| test-5 | Brightness drops clearly from sphere 1 → 2 → 3 → 4 (quadratic falloff) |
| test-6 | All 3 depth-separated spheres look identical (no distance falloff for directional) |
