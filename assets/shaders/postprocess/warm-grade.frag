#version 330

// The texture holding the scene pixels
uniform sampler2D tex;

in vec2 tex_coord;
out vec4 frag_color;

// Warm cinematic color grade + vignette — Phase 2 postprocessing effect.
// Pushes the palette toward gold/amber (boxing arena atmosphere) while
// darkening the screen edges to focus attention on the ring center.

void main(){
    frag_color = texture(tex, tex_coord);

    // ── Warm color grade ─────────────────────────────────────────────
    // Lift reds slightly (warm arena lights), pull blues slightly (less cold)
    frag_color.r = min(frag_color.r * 1.10, 1.0);
    frag_color.g = min(frag_color.g * 1.02, 1.0);
    frag_color.b =     frag_color.b * 0.88;

    // Subtle contrast boost: expand the tonal range away from mid-gray
    frag_color.rgb = (frag_color.rgb - 0.5) * 1.12 + 0.5;
    frag_color.rgb = clamp(frag_color.rgb, 0.0, 1.0);

    // ── Vignette ─────────────────────────────────────────────────────
    // Darken corners; the exponent (0.75) controls how quickly it fades
    vec2 ndc = tex_coord * 2.0 - 1.0;
    frag_color.rgb /= 1.0 + dot(ndc, ndc) * 0.75;
}
