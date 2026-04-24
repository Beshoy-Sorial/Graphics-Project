#version 330 core

in Varyings {
    vec4 color;
    vec2 tex_coord;
} fs_in;

out vec4 frag_color;

uniform vec4 tint;
uniform sampler2D tex;
uniform float alphaThreshold;
uniform int weatherMode;

void main(){
    vec4 textureColor = texture(tex, fs_in.tex_coord);
    if(textureColor.a < alphaThreshold) discard;
    vec4 final_color = tint * fs_in.color * textureColor;
    
    if (weatherMode == 0) { // Sunny
        final_color *= vec4(1.2, 1.15, 0.9, 1.0); // Bright sunlight
    } else if (weatherMode == 1) { // Rainy / Night
        final_color *= vec4(0.3, 0.35, 0.5, 1.0); // Dark, moonlit
    } else if (weatherMode == 2) { // Snow
        final_color *= vec4(0.85, 0.9, 1.0, 1.0); // Overcast
    }
    
    frag_color = final_color;
}
