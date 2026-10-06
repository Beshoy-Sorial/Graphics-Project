#version 330

// Bloom step 1: keep only the pixels brighter than "threshold" (with a soft knee so the
// transition is smooth) while downsampling the HDR scene to half resolution.
uniform sampler2D tex;
uniform float threshold;
uniform float knee;

in vec2 tex_coord;
out vec4 frag_color;

void main(){
    vec2 texel = 1.0 / vec2(textureSize(tex, 0));
    // 4 bilinear taps = average of a 4x4 block -> stable, flicker-free downsample
    vec3 c = texture(tex, tex_coord + texel * vec2(-1.0, -1.0)).rgb
           + texture(tex, tex_coord + texel * vec2( 1.0, -1.0)).rgb
           + texture(tex, tex_coord + texel * vec2(-1.0,  1.0)).rgb
           + texture(tex, tex_coord + texel * vec2( 1.0,  1.0)).rgb;
    c *= 0.25;
    c = min(c, vec3(64.0)); // avoid single super-bright pixels exploding into big blobs

    float brightness = max(c.r, max(c.g, c.b));
    float soft = clamp(brightness - threshold + knee, 0.0, 2.0 * knee);
    soft = soft * soft / (4.0 * knee + 1e-5);
    float contribution = max(soft, brightness - threshold) / max(brightness, 1e-5);

    frag_color = vec4(c * contribution, 1.0);
}
