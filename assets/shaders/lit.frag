#version 330 core
#include "light_common.glsl"

in Varyings {
    vec4 color;
    vec2 tex_coord;
    vec3 world_position;
    vec3 world_normal;
} fs_in;

uniform TexturedMaterial material;
uniform vec3  camera_position;
uniform vec3  material_tint;            // default (1,1,1) = no change
uniform int   material_recolor;         // 1 = tint recolors the albedo (arena colour picker)
uniform vec2  material_uv_scale;        // texture tiling
uniform float material_rim;             // fresnel rim light strength
uniform int   material_has_normal_map;
uniform float material_normal_strength;
uniform vec3  object_emissive;          // per-object glow (hit flash, stun...)

// Set by the renderer (all default to 0 = disabled)
uniform int   hdr_pipeline;             // 1 = linear HDR lighting (tone mapped later)
uniform int   weatherMode;              // 0 none, 1 sun, 2 rain, 3 snow
uniform vec3  ambient_sky;              // hemisphere ambient: color coming from above
uniform vec3  ambient_ground;           // ... and from below
uniform vec3  fog_color;
uniform float fog_density;

out vec4 frag_color;

// ── Arena colour tint ─────────────────────────────────────────────────
// Strategy: replace the albedo with the chosen tint directly, then let
// the lighting system shade it.  Two paths:
//
//   • Saturated tints (blue, red…): luminance-preserving hue recolor.
//   • Grey/white tints: swap albedo = tintRaw directly.
//     Weight = 0 ONLY for pure white (1,1,1) → default unchanged.
//
// Trace for each arena pick:
//   (1,1,1)          greyW=0, hueW=0  → unchanged (no pick)          ✓
//   (0.9,0.9,0.9)    greyW=1, hueW=0  → near-white albedo            ✓
//   (0.18,0.18,0.18) greyW=1, hueW=0  → dark-grey albedo             ✓
//   (0.1,0.2,0.5)    greyW=0, hueW=1  → vivid blue hue-recolor       ✓
vec3 recolorAlbedo(vec3 albedo_raw, vec3 tintRaw){
    float maxC       = max(max(tintRaw.r, tintRaw.g), tintRaw.b);
    float minC       = min(min(tintRaw.r, tintRaw.g), tintRaw.b);
    float targetGrey = (tintRaw.r + tintRaw.g + tintRaw.b) / 3.0;
    float sat        = (maxC > 0.001) ? (maxC - minC) / maxC : 0.0;
    vec3  normHue    = (maxC > 0.001) ? (tintRaw / maxC) : vec3(1.0);

    // Path A — hue recolor (coloured arena picks)
    float lum        = dot(albedo_raw, vec3(0.2126, 0.7152, 0.0722));
    float boostedLum = clamp(lum * 2.5 + 0.25, 0.0, 1.0);
    vec3  recolored  = boostedLum * normHue;

    // Path B — direct replacement for grey/white picks
    // (1-targetGrey)*10: 0 when tint=(1,1,1), ≥1 for any other grey shade
    float greyWeight = clamp((1.0 - sat) * (1.0 - targetGrey) * 10.0, 0.0, 1.0);

    // Combine — hue first, then grey override on top
    float hueWeight = clamp(sat * 1.5, 0.0, 1.0);
    vec3  tinted    = mix(albedo_raw, recolored, hueWeight);
    return mix(tinted, tintRaw, greyWeight);
}

// Normal mapping without per-vertex tangents: the tangent frame is rebuilt from screen-space
// derivatives of the position and the texture coordinates ("cotangent frame", C. Schüler 2013).
vec3 perturbNormal(vec3 N, vec3 p, vec2 uv, vec3 mapNormal){
    vec3 dp1 = dFdx(p);
    vec3 dp2 = dFdy(p);
    vec2 duv1 = dFdx(uv);
    vec2 duv2 = dFdy(uv);
    vec3 dp2perp = cross(dp2, N);
    vec3 dp1perp = cross(N, dp1);
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
    float invmax = inversesqrt(max(max(dot(T, T), dot(B, B)), 1e-12));
    mat3 TBN = mat3(T * invmax, B * invmax, N);
    return normalize(TBN * mapNormal);
}

vec3 weatherMultiplier(){
    if(weatherMode == 1) return vec3(1.06, 1.03, 0.96);   // Sunny: warm
    if(weatherMode == 2) return vec3(0.45, 0.50, 0.65);   // Rainy / night: dark & cold
    if(weatherMode == 3) return vec3(0.85, 0.90, 1.00);   // Snow: overcast
    return vec3(1.0);
}

void main(){
    vec2 uv = fs_in.tex_coord * material_uv_scale;
    vec3 albedo_raw = texture(material.albedo_tex, uv).rgb * fs_in.color.rgb;
    vec3 albedo = (material_recolor == 1) ? recolorAlbedo(albedo_raw, material_tint)
                                          : albedo_raw * material_tint;

    vec3  specTex  = texture(material.specular_tex,          uv).rgb;
    float ao       = texture(material.ambient_occlusion_tex, uv).r;
    float rough    = texture(material.roughness_tex,         uv).r;
    vec3  emissive = texture(material.emissive_tex,          uv).rgb;

    float shininess = 2.0 / pow(clamp(rough, 0.001, 0.999), 4.0) - 2.0;

    vec3 N = normalize(fs_in.world_normal);
    vec3 V = normalize(camera_position - fs_in.world_position);

    if(hdr_pipeline == 0){
        // ── Original (Phase 2 requirement) path ───────────────────────
        frag_color = vec4(
            calculateLighting(albedo, specTex, ao, shininess, emissive,
                              fs_in.world_position, N, V),
            1.0
        );
        return;
    }

    // ── HDR path: everything below is in linear color space ──────────
    albedo   = pow(albedo, vec3(2.2));       // textures and tints are authored in sRGB
    emissive = pow(emissive, vec3(2.2));
    shininess = clamp(shininess, 1.0, 2048.0);

    if(material_has_normal_map == 1){
        vec3 mapN = texture(material.normal_tex, uv).xyz * 2.0 - 1.0;
        mapN.xy *= material_normal_strength;
        N = perturbNormal(N, fs_in.world_position, uv, normalize(mapN));
    }

    vec3 ambient;
    vec3 direct = calculateLightingEnhanced(albedo, specTex, ao, shininess,
                                            fs_in.world_position, N, V, ambient);

    // Hemisphere ambient light: surfaces facing up receive the "sky" color, facing down the "ground" color
    float up = N.y * 0.5 + 0.5;
    ambient += mix(ambient_ground, ambient_sky, up) * albedo * ao;

    // Fresnel rim light: brightens silhouettes so characters stand out from the dark background
    float rim = pow(1.0 - max(dot(N, V), 0.0), 3.0) * material_rim;
    vec3  rimColor = rim * (ambient_sky * 6.0 + 0.04) * mix(vec3(1.0), albedo * 2.0, 0.5);

    vec3 color = (direct + ambient + rimColor) * weatherMultiplier() + emissive + object_emissive;

    // Exponential distance fog
    if(fog_density > 0.0){
        float dist = length(camera_position - fs_in.world_position);
        float fog  = 1.0 - exp(-fog_density * dist);
        color = mix(color, fog_color, fog);
    }

    frag_color = vec4(color, 1.0);
}
