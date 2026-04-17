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

out vec4 frag_color;

void main(){
    vec3  albedo   = texture(material.albedo_tex,           fs_in.tex_coord).rgb
                     * fs_in.color.rgb;                   
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
