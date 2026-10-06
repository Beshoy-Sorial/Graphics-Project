#version 330 core

in Varyings {
    vec4 color;
} fs_in;

out vec4 frag_color;

uniform vec4 tint;
// Set by the forward renderer. 0 (the default when nobody sets it) means "no weather" so the
// requirement tests are not affected; 1 = sun, 2 = rain / night, 3 = snow.
uniform int weatherMode;
// 1 when the renderer uses the linear HDR pipeline (the output is converted back to sRGB at the end)
uniform int hdr_pipeline;

void main(){
    vec4 final_color = tint * fs_in.color;
    if (weatherMode == 1) { // Sunny
        final_color *= vec4(1.2, 1.15, 0.9, 1.0); // Bright sunlight
    } else if (weatherMode == 2) { // Rainy / Night
        final_color *= vec4(0.3, 0.35, 0.5, 1.0); // Dark, moonlit
    } else if (weatherMode == 3) { // Snow
        final_color *= vec4(0.85, 0.9, 1.0, 1.0); // Overcast
    }
    if (hdr_pipeline == 1) final_color.rgb = pow(max(final_color.rgb, vec3(0.0)), vec3(2.2));
    frag_color = final_color;
}
