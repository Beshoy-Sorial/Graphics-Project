# Phase 2, Requirement 1: Lighting — Complete Study Guide

---

## 🧠 What is Lighting in our Engine?

In Phase 1, we learned how to draw objects using flat colors or simple textures. However, the world isn't evenly lit. To make a 3D scene look realistic, we need to simulate how light interacts with surfaces. 

In this requirement, we implement the **Phong Reflection Model** using an ECS-based Forward Renderer. The lighting system involves three main parts:
1. **`LightComponent`**: Defines the properties of a light source (color, type, falloff).
2. **`LitMaterial`**: Defines the properties of a surface (base color, shininess, emissive glow) using 5 distinct texture maps.
3. **Shaders**: The GPU programs that do the heavy lifting, calculating how much light hits each pixel based on distance, angle, and surface properties.

---

## 📁 Relevant Files

| File | Purpose |
|---|---|
| `source/common/components/light.hpp/cpp` | The `LightComponent` class. |
| `source/common/material/material.hpp/cpp` | The `LitMaterial` class. |
| `assets/shaders/light_common.glsl` | A shared GLSL library containing lighting structs and the main `calculateLighting()` math. |
| `assets/shaders/lit.vert` | Transforms positions/normals to world space for the lighting calculations. |
| `assets/shaders/lit.frag` | Samples the material's textures and calls the lighting math. |
| `source/common/systems/forward-renderer.cpp` | Collects all lights and sends them to the shaders before drawing. |
| `source/common/shader/shader.cpp` | Adds a `#include` preprocessor so shaders can share code. |

---

## 💡 The `LightComponent`

### What it stores

A light component stores all the properties of a light source, **except** its position and direction. 

```cpp
enum class LightType { DIRECTIONAL, POINT, SPOT };

class LightComponent : public Component {
    LightType lightType;        // directional / point / spot
    glm::vec3 diffuse;          // diffuse color contribution
    glm::vec3 specular;         // specular color contribution
    glm::vec3 ambient;          // ambient color contribution
    float attenuationConstant;  // point/spot: base falloff
    float attenuationLinear;    // point/spot: linear falloff
    float attenuationQuadratic; // point/spot: quadratic falloff
    float innerConeAngle;       // spot: inner cutoff (radians)
    float outerConeAngle;       // spot: outer cutoff (radians)
};
```

### Why no Position or Direction?
Because it's an ECS component! Its position and direction are automatically calculated from the entity's own transform matrix:
- **Position** = The entity's origin point (`localToWorld * vec4(0,0,0,1)`).
- **Direction** = The entity's -Z axis (`localToWorld * vec4(0,0,-1,0)`).

This means you can parent a light to a moving spaceship, or attach a spot light to a player's camera, and it will perfectly follow them without any extra code!

### Types of Lights

1. **Directional (`LightType::DIRECTIONAL`)**: Like the Sun. It affects the whole scene evenly and its light rays are perfectly parallel. It has a direction but no specific start position, and does not fade over distance.
2. **Point (`LightType::POINT`)**: Like a light bulb. Radiates outwards in all directions from a specific point. It fades over distance (attenuation).
3. **Spot (`LightType::SPOT`)**: Like a flashlight. Radiates from a point in a specific direction, but restricted within a cone.

---

## 🎨 The 3 Components of Phong Lighting

The final light intensity on a surface is the sum of three distinct types of light:

1. **Ambient**: `light.ambient * albedo * ao`. The "baseline" light. It fakes indirect light bouncing around the room so shadows don't look pitch black.
2. **Diffuse**: `light.diffuse * albedo * max(dot(N, L), 0)`. The main direct light. Depends on the angle (`NdotL`) between the surface normal (`N`) and the direction to the light (`L`). If the surface faces the light, it gets bright. If it tilts away, it gets darker.
3. **Specular**: `light.specular * specTex * pow(max(dot(R, V), 0), shininess)`. The bright reflection highlight. Depends on the viewer's eye (`V`) matching the reflected light direction (`R`).

---

## 📖 The `LitMaterial`

To support realistic rendering, our materials need more than just one texture. The `LitMaterial` class inherits from `Material` and supports up to **5 texture maps**. 

```cpp
class LitMaterial : public Material {
public:
    Texture2D* albedo_map;             // Unit 0: Surface base color
    Texture2D* specular_map;           // Unit 1: Specular tint/intensity
    Texture2D* ambient_occlusion_map;  // Unit 2: Darkens crevices
    Texture2D* roughness_map;          // Unit 3: How rough/smooth the surface is
    Texture2D* emissive_map;           // Unit 4: Self-emitted light (glow)
    Sampler*   sampler;
};
```

### The 1x1 Fallback Textures
What if a JSON mesh doesn't provide an emissive map? We don't want OpenGL to sample random garbage! `LitMaterial::setup()` uses **1x1 fallback textures**:
- Missing `albedo` or `specular`? Use a solid **white** texture (doesn't modify colors).
- Missing `roughness`? Use a solid **mid-gray** texture (average shininess).
- Missing `emissive`? Use a solid **black** texture (no glow).

---

## 💻 The Shaders

### 1. `light_common.glsl` (The Shared Library)
To avoid duplicating structs and formulas across many shaders, we created a shared file. It defines the `Light` array, the `TexturedMaterial` samplers, and the master `calculateLighting( albedo... N, V )` function.

### 2. `lit.vert` (Vertex Shader)
```glsl
// Transform normally for the screen:
gl_Position = transform * vec4(position, 1.0);

// We need the ACTUAL world position of this vertex for lighting math:
vs_out.world_position = (object_to_world * vec4(position, 1.0)).xyz;

// We need the normal, but if the object is scaled weirdly (e.g., scale X by 3),
// normals will stretch. Using the Inverse-Transpose matrix fixes this!
vs_out.world_normal = normalize((object_to_world_inv_transpose * vec4(normal, 0.0)).xyz);
```

### 3. `lit.frag` (Fragment Shader)
```glsl
#version 330 core
#include "light_common.glsl" // Loads our lighting library!

void main(){
    // Sample all 5 maps
    vec3  albedo   = texture(material.albedo_tex, fs_in.tex_coord).rgb * fs_in.color.rgb;
    float rough    = texture(material.roughness_tex, fs_in.tex_coord).r;
    // ... (sample others)

    // Convert roughness into "shininess" power
    float shininess = 2.0 / pow(clamp(rough, 0.001, 0.999), 4.0) - 2.0;

    // Call our massive lighting function!
    frag_color = vec4(calculateLighting(albedo, specTex, ao, shininess, emissive,
                                        fs_in.world_position, N, V), 1.0);
}
```

---

## 🔄 The Forward Renderer Integration

When `ForwardRenderer::render(world)` runs, it needs to grab all the active lights and send them to the `LitMaterial` shaders.

1. **Collect Lights**: It iterates over `world->getEntities()` and stores any `LightComponent*` it finds.
2. **Draw Objects**: For every opaque/transparent command, it calls a `sendLitUniforms` helper. 
3. **Send Data**: 
    - Checks `if(dynamic_cast<LitMaterial*>(command.material))` to make sure we don't accidentally send a dozen lights to a basic `TintedMaterial` shader.
    - Sends the `camera_position`, `object_to_world`, and `object_to_world_inv_transpose`.
    - Loops over every light (up to 16 maximum) and sets uniforms like `lights[i].diffuse`, `lights[i].position`, etc. 

---

## ⚠️ PRO-TIP: Smoothstep in Spotlights

For Spotlight cone cutoffs, we calculate how far a pixel is from the center of the cone. 
Instead of doing expensive `acos()` math in the shader, the renderer sends the `innerCutoff` and `outerCutoff` as cosines (`glm::cos(angle)`).

The shader then does:
```glsl
float cosTheta = dot(L, normalize(-light.direction));
attenuation *= smoothstep(light.outerCutoff, light.innerCutoff, cosTheta);
```
`smoothstep` smoothly blends intensity from 0 (at the outer edge) to 1 (at the inner edge), giving your flashlights a realistic soft edge!

---

## ✅ Key Things to Remember

| Concept | Detail |
|---|---|
| `LightComponent` | Stores light parameters but gets position/direction organically from its Entity Transform. |
| **Ambient Light** | Base background lighting (doesn't attenuate). |
| **Diffuse Light** | Main illumination, uses `N dot L` (faces light = bright, turns away = dark). |
| **Specular Light** | Reflected highlight. Requires viewer direction (`V`). Controlled by Roughness Map. |
| **Inverse-Transpose** | Special matrix required to keep normals perfectly perpendicular when objects are scaled unevenly. |
| `#include "light_common"` | We wrote a custom preprocessor to allow shaders to share complex structs and functions. |
| `LitMaterial` | Inherits from base `Material`, supports up to 5 individual PBR-style textures or defaults to safe 1x1 fallback blocks. |

---

## 🧪 How to Test

Run these commands from the terminal to manually test the lighting implementations. Take note of how lights look.

```powershell
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-0.jsonc  # All 3 lights + 5 maps
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-1.jsonc  # Emissive only
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-3.jsonc  # RGB colored lights
./bin/GAME_APPLICATION.exe -c config/lighting-test/test-5.jsonc  # Distance Attenuation
```
