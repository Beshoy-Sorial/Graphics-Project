# Requirement 1: Lighting — Manual Testing Guide

> Run every test from the **project root** (`Graphics-Project/`).  
> The window opens and stays open until you close it — use `WASD` + mouse to move the camera.  
> Add `-f 1` to the command to take a quick screenshot and exit without interaction.

---

## How to run a test

```bash
./bin/GAME_APPLICATION.exe -c <config-file>
```

To auto-close after N frames (useful for batch checks):

```bash
./bin/GAME_APPLICATION.exe -c <config-file> -f 60
```

---

## Test 0 — Full scene: all 3 light types + all 5 texture maps + non-uniform scale

```bash
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-0.jsonc
```

### What is in the scene

| Object | Position | Purpose |
|--------|----------|---------|
| Monkey | center | Tests full 5-map material |
| Sphere | right | Tests specular variation (color-grid as specular map) |
| Cube | left, scale `[3,1,1]` | Tests normal correctness under non-uniform scale |
| Glowing sphere | back | Tests emissive map (smile pattern visible even in shadow) |
| Ground plane | below all | Tests AO and roughness maps |
| Point light | `[5,5,5]` | White, with quadratic attenuation |
| Directional light | rotated `[45,-30,0]` | Cool blue fill, no attenuation |
| Spot light | `[0,6,0]` pointing down | Red cone with soft edge |

### What to look for ✅

**Point light:**
- Objects closer to `[5,5,5]` are brighter
- Surfaces facing away from that corner are darker
- Small specular highlight (bright dot) on the sphere facing the light

**Directional light:**
- Flat blue tint on all surfaces facing upper-left
- Same intensity regardless of distance (no falloff)

**Spot light:**
- Red circular cone on the ground plane below `[0,6,0]`
- Soft edge between lit and unlit area (not a hard cutoff line)
- Objects outside the cone receive zero red contribution

**Texture maps:**
- The specular map (`color-grid`) makes different areas of the sphere shine differently — some squares should have no highlight, others full highlight
- The roughness map (`moon`) creates varied shininess across the surface
- The emissive sphere in the back should show the smile pattern glowing even when it is in shadow

**Non-uniform scale normals:**
- Move camera to view the left cube from the side (`scale [3,1,1]`)
- Specular highlight should sit correctly on the face closest to the light
- If the highlight slides to a wrong face → normal transformation is broken (it is not — this is the passing state)

---

## Test 1 — Emissive only: NO lights in scene

```bash
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-1.jsonc
```

### What is in the scene

| Object | Material | Expected result |
|--------|----------|-----------------|
| Left sphere | `emissive_map = smile` | Smile pattern glows — visible even without any light |
| Right sphere | `albedo_map = wood`, no emissive | Completely black — no light source, no emission |

### What to look for ✅

- **Left sphere** is NOT black — the smile texture pattern should be visible in color/white
- **Right sphere** is completely black (or near-black from ambient = 0)
- This proves: `frag_color` starts with `emissive` before the light loop, so emission is independent of lights

### What failure looks like ✗

- Both spheres are black → emissive is not added to `result`
- Both spheres are lit → ambient from lights is leaking even though there are no lights

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

- **Center sphere**: specular dot appears where the light reflects toward camera (upper-right area)
- **Left sphere** (wide): the specular dot moves smoothly across the stretched surface — it stays perpendicular to the surface, not skewed
- **Right sphere** (tall): same — highlight is in the geometrically correct position

### How to confirm it is correct

Move the camera around each sphere. The specular highlight should always sit at the point where the angle of incidence equals the angle of reflection. If you stretch a sphere and the highlight "floats" to the wrong side or looks wrong → the plain model matrix was used instead of the inverse-transpose.

### What failure looks like ✗

If you remove the `object_to_world_inv_transpose` line in `lit.vert` and replace it with `object_to_world`, the highlight on the stretched spheres will be visibly off-axis — it will appear on a face that isn't actually facing the light.

---

## Spot light edge cases — modify test-0 live

Open `config/lighting-test/test-0.jsonc` in a text editor and try these tweaks while the app is NOT running:

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

### Hard edge cone (no transition zone)

```jsonc
"innerConeAngle": 25.0,
"outerConeAngle": 25.5
```

Expected: hard sharp cutoff edge (inner ≈ outer → `smoothstep` range is tiny).

---

## Attenuation edge cases — modify test-0 live

### No attenuation (constant = 1, linear = 0, quadratic = 0)

```jsonc
"attenuationConstant":  1.0,
"attenuationLinear":    0.0,
"attenuationQuadratic": 0.0
```

Expected: point light has same intensity at all distances — no falloff.

### Strong attenuation (light fades quickly)

```jsonc
"attenuationConstant":  1.0,
"attenuationLinear":    0.7,
"attenuationQuadratic": 1.8
```

Expected: light barely reaches the monkey, only objects very close to `[5,5,5]` are bright.

---

## Multiple lights — add more lights to test-0

Copy and paste extra light entries into the `"world"` array. The system supports up to **16 lights**.

Example: add a green point light on the other side

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

Expected: monkey/sphere left side turns green, right side stays white (from original point light). The Phong accumulation of both lights is visible simultaneously.

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

Run each command and confirm the visual result:

```bash
# All 3 light types + all 5 maps
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-0.jsonc

# Emissive without any light source
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-1.jsonc

# Normal correctness under non-uniform scale
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-2.jsonc

# Existing scenes still work (no regression)
./bin/GAME_APPLICATION.exe -c config/app.jsonc
```

| Test | Pass condition |
|------|----------------|
| test-0 | Spot cone visible on ground, specular varies per color-grid square, emissive sphere glows |
| test-1 | Left sphere shows smile glow, right sphere is black |
| test-2 | Specular highlight sits correctly on non-uniform-scaled spheres |
| app.jsonc | Existing textured/tinted scene renders unchanged |
