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
uniform vec3 material_tint; // Per-material colour tint; default white (1,1,1)

out vec4 frag_color;

void main(){
    vec3 albedo_raw = texture(material.albedo_tex, fs_in.tex_coord).rgb * fs_in.color.rgb;

    // Arena colour tint.
    // Fighters use the default white tint (1,1,1) — no change to albedo.
    // Arena Ring/Floor use a player-chosen colour.  Direct multiplication makes
    // dark textures (grass) go nearly black for dark picks like Deep Blue [0.1,0.2,0.5].
    // Fix: normalize the tint to max-component = 1 (extracts the hue) then do a
    // luminance-preserving recolor — texture brightness pattern is kept but hue
    // shifts to the chosen colour.  A mix against the plain albedo based on
    // tint saturation ensures white/grey tints (fighters) remain unchanged.
    vec3  tintRaw = material_tint;
    float maxC    = max(max(tintRaw.r, tintRaw.g), tintRaw.b);
    float minC    = min(min(tintRaw.r, tintRaw.g), tintRaw.b);
    // Saturation of the tint (0 = grey/white, 1 = pure hue)
    float sat     = (maxC > 0.001) ? (maxC - minC) / maxC : 0.0;
    // Normalized hue (max component = 1 so no darkening from the raw value)
    vec3  normHue = (maxC > 0.001) ? (tintRaw / maxC) : vec3(1.0);
    // Boost albedo luminance so dark textures still show the colour
    float lum        = dot(albedo_raw, vec3(0.2126, 0.7152, 0.0722));
    float boostedLum = clamp(lum * 2.5 + 0.25, 0.0, 1.0);
    vec3  recolored  = boostedLum * normHue;
    // sat≈0 (default white) → plain albedo; sat≈1 (chosen colour) → recolored
    float blendT = clamp(sat * 1.5, 0.0, 1.0);
    vec3  albedo = mix(albedo_raw, recolored, blendT);
    vec3  specTex  = texture(material.specular_tex,         fs_in.tex_coord).rgb;
    float ao       = texture(material.ambient_occlusion_tex, fs_in.tex_coord).r;
    float rough    = texture(material.roughness_tex,        fs_in.tex_coord).r;
    vec3  emissive = texture(material.emissive_tex,         fs_in.tex_coord).rgb;

    float shininess = 2.0 / pow(clamp(rough, 0.001, 0.999), 4.0) - 2.0;

    vec3 N = normalize(fs_in.world_normal);
    vec3 V = normalize(camera_position - fs_in.world_position);

    frag_color = vec4(
        calculateLighting(albedo, specTex, ao, shininess, emissive,
                          fs_in.world_position, N, V),
        1.0
    );
}
