#version 330 core
#include "light_common.glsl"

in Varyings {
    vec4 color;
    vec2 tex_coord;
    vec3 world_position;
    vec3 world_normal;
} fs_in;

uniform TexturedMaterial material;
uniform vec3 camera_position;
uniform vec3 material_tint; // default (1,1,1) = no change

out vec4 frag_color;

void main(){
    vec3 albedo_raw = texture(material.albedo_tex, fs_in.tex_coord).rgb * fs_in.color.rgb;

    // ── Arena colour tint ─────────────────────────────────────────────────
    // Strategy: replace the albedo with the chosen tint directly, then let
    // the lighting system shade it.  Two paths:
    //
    //   • Saturated tints (blue, red…): luminance-preserving hue recolor.
    //   • Grey/white tints: swap albedo = tintRaw directly.
    //     Weight = 0 ONLY for pure white (1,1,1) → fighters/default unchanged.
    //
    // Trace for each arena pick:
    //   (1,1,1)          greyW=0, hueW=0  → unchanged (fighters / no pick) ✓
    //   (0.9,0.9,0.9)    greyW=1, hueW=0  → near-white albedo              ✓
    //   (0.18,0.18,0.18) greyW=1, hueW=0  → dark-grey albedo               ✓
    //   (0.1,0.2,0.5)    greyW=0, hueW=1  → vivid blue hue-recolor         ✓

    vec3  tintRaw    = material_tint;
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
    vec3  albedo    = mix(tinted,     tintRaw,   greyWeight);

    vec3  specTex  = texture(material.specular_tex,          fs_in.tex_coord).rgb;
    float ao       = texture(material.ambient_occlusion_tex, fs_in.tex_coord).r;
    float rough    = texture(material.roughness_tex,         fs_in.tex_coord).r;
    vec3  emissive = texture(material.emissive_tex,          fs_in.tex_coord).rgb;

    float shininess = 2.0 / pow(clamp(rough, 0.001, 0.999), 4.0) - 2.0;

    vec3 N = normalize(fs_in.world_normal);
    vec3 V = normalize(camera_position - fs_in.world_position);

    frag_color = vec4(
        calculateLighting(albedo, specTex, ao, shininess, emissive,
                          fs_in.world_position, N, V),
        1.0
    );
}
