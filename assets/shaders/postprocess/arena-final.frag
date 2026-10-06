#version 330

// Final pass of the HDR pipeline (Phase-2 postprocessing effect):
//   bloom composite -> exposure -> ACES filmic tone mapping -> warm arena grade
//   -> vignette -> grayscale (KO effect) -> gamma correction -> dithering

uniform sampler2D tex;        // HDR scene (linear color, can be > 1)
uniform sampler2D bloom_tex;  // blurred bright parts of the scene
uniform float bloom_strength;
uniform float exposure;
uniform float time;

// Grayscale intensity: 0.0 = full color, 1.0 = full grayscale (player KO effect)
uniform float u_grayscale;

in vec2 tex_coord;
out vec4 frag_color;

// ACES filmic curve (Krzysztof Narkowicz fit): compresses highlights smoothly instead of clipping
vec3 acesFilm(vec3 x){
    const float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

float hash(vec2 p){
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main(){
    vec3 hdr = texture(tex, tex_coord).rgb;
    hdr += texture(bloom_tex, tex_coord).rgb * bloom_strength;

    vec3 color = acesFilm(hdr * exposure);

    // ── Warm color grade ─────────────────────────────────────────────
    // Lift reds slightly (warm arena lights), pull blues slightly (less cold)
    color *= vec3(1.04, 1.0, 0.95);

    // ── Vignette ─────────────────────────────────────────────────────
    // Darken corners to focus attention on the ring center
    vec2 ndc = tex_coord * 2.0 - 1.0;
    color *= 1.0 - 0.28 * smoothstep(0.35, 1.45, length(ndc));

    // ── Grayscale (KO effect) ────────────────────────────────────────
    float gray = dot(color, vec3(0.2126, 0.7152, 0.0722));
    color = mix(color, vec3(gray), u_grayscale);

    // ── Gamma correction (linear -> sRGB for the monitor) ────────────
    color = pow(clamp(color, 0.0, 1.0), vec3(1.0 / 2.2));

    // Tiny noise removes color banding in dark gradients
    color += (hash(gl_FragCoord.xy + fract(time) * 100.0) - 0.5) / 255.0;

    frag_color = vec4(color, 1.0);
}
