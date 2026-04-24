#version 330 core

in vec4 particleColor;
out vec4 frag_color;

uniform int weatherMode;

void main() {
    if(weatherMode == 0 || particleColor.a <= 0.0) discard;

    vec2 coord = gl_PointCoord - vec2(0.5);
    float dist = length(coord);

    if (dist > 0.5) discard;

    if(weatherMode == 1) {
        // Rain - narrower vertically, but thick enough to be very visible
        if (abs(coord.x) > 0.20) discard; 
        
        // Emphasize brightness and make it blue
        vec3 blueTint = vec3(0.3, 0.6, 1.0);
        frag_color = vec4(blueTint * 1.8, particleColor.a);
    } else if(weatherMode == 2) {
        // Snow - fluffy circle with soft edges
        float alpha = clamp((0.5 - dist) * 2.5, 0.0, 1.0); // Sharper flake core
        frag_color = vec4(particleColor.rgb * 1.2, particleColor.a * alpha);
    }
}