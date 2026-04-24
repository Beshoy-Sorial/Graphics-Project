#version 330 core

in vec4 particleColor;
flat in int particleType;
out vec4 frag_color;

uniform int weatherMode;

void main() {
    if (particleType < 0 || particleColor.a <= 0.0) discard;

    vec2 coord = gl_PointCoord - vec2(0.5);
    float dist = length(coord);

    if (dist > 0.5) discard;

    if (particleType == 0) {
        // Sun - glowing sphere
        float alpha = smoothstep(0.5, 0.0, dist);
        vec3 color = mix(vec3(1.0, 0.5, 0.0), vec3(1.0, 1.0, 0.8), smoothstep(0.4, 0.0, dist));
        frag_color = vec4(color, particleColor.a * alpha);
    } 
    else if (particleType == 1) {
        // Cloud - fluffy circle
        float alpha = smoothstep(0.5, 0.1, dist);
        frag_color = vec4(particleColor.rgb, particleColor.a * alpha);
    } 
    else if (particleType == 2) {
        // Rain - narrower vertically
        if (abs(coord.x) > 0.20) discard; 
        vec3 blueTint = vec3(0.3, 0.6, 1.0);
        frag_color = vec4(blueTint * 1.8, particleColor.a);
    } 
    else if (particleType == 3) {
        // Snow - fluffy circle with soft edges
        float alpha = clamp((0.5 - dist) * 2.5, 0.0, 1.0);
        frag_color = vec4(particleColor.rgb * 1.2, particleColor.a * alpha);
    }
}