#version 330

// Bloom step 2: separable 9-tap gaussian blur. It is run horizontally then vertically.
// Using the bilinear filter, 9 taps are done with only 5 texture reads.
uniform sampler2D tex;
uniform vec2 direction; // (1/width, 0) or (0, 1/height), scaled by the blur spread

in vec2 tex_coord;
out vec4 frag_color;

void main(){
    const float offsets[3] = float[](0.0, 1.3846153846, 3.2307692308);
    const float weights[3] = float[](0.2270270270, 0.3162162162, 0.0702702703);

    vec3 result = texture(tex, tex_coord).rgb * weights[0];
    for(int i = 1; i < 3; i++){
        result += texture(tex, tex_coord + direction * offsets[i]).rgb * weights[i];
        result += texture(tex, tex_coord - direction * offsets[i]).rgb * weights[i];
    }
    frag_color = vec4(result, 1.0);
}
