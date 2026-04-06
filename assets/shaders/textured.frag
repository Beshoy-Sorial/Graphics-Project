#version 330 core

in Varyings {
    vec4 color;
    vec2 tex_coord;
} fs_in;

out vec4 frag_color;

uniform vec4 tint;
uniform sampler2D tex;
uniform float alphaThreshold;

void main(){
    vec4 textureColor = texture(tex, fs_in.tex_coord);
    if(textureColor.a < alphaThreshold) discard;
    frag_color = tint * fs_in.color * textureColor;
}
