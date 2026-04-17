#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec4 color;
layout(location = 2) in vec2 tex_coord;
layout(location = 3) in vec3 normal;

out Varyings {
    vec4 color;
    vec2 tex_coord;
    vec3 world_position;
    vec3 world_normal;
} vs_out;

uniform mat4 transform;
uniform mat4 object_to_world;
uniform mat4 object_to_world_inv_transpose;

void main(){
    gl_Position = transform * vec4(position, 1.0);
    vs_out.color = color;
    vs_out.tex_coord = tex_coord;
    vs_out.world_position = (object_to_world * vec4(position, 1.0)).xyz;
    vs_out.world_normal = normalize((object_to_world_inv_transpose * vec4(normal, 0.0)).xyz);
}
