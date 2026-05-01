#version 330

// The texture holding the scene pixels
uniform sampler2D tex;

// Grayscale intensity: 0.0 = full color, 1.0 = full grayscale (player KO effect)
uniform float u_grayscale;

// Read "assets/shaders/fullscreen.vert" to know what "tex_coord" holds;
in vec2 tex_coord;

out vec4 frag_color;

// Vignette: darkens screen corners to focus attention on the center.
// Grayscale: activated when the player is knocked out to signal defeat.

void main(){
    // Sample the scene
    frag_color = texture(tex, tex_coord);

    // Vignette: divide by (1 + r^2) where r is distance from center in NDC
    vec2 ndc = tex_coord * 2.0 - 1.0;
    frag_color.rgb /= 1.0 + dot(ndc, ndc);

    // Grayscale (KO effect): convert to luminance and mix with original colour
    // Luminance weights (ITU-R BT.709) give perceptually correct gray.
    float gray = dot(frag_color.rgb, vec3(0.2126, 0.7152, 0.0722));
    frag_color.rgb = mix(frag_color.rgb, vec3(gray), u_grayscale);

    // DEBUG: Turn the whole screen BRIGHT RED if knocked out, so we know 100% the C++ uniform reached the shader!
    if (u_grayscale > 0.5) {
        frag_color.rgb = vec3(1.0, 0.0, 0.0);
    }
}
