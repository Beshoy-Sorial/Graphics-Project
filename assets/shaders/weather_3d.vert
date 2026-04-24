#version 330 core
layout(location = 0) in vec3 position;

uniform mat4 VP;
uniform float time;
uniform int weatherMode;

out vec4 particleColor;

float rand(float n) { return fract(sin(n) * 43758.5453123); }

void main() {
    vec3 pos = position;
    
    // A stable pseudo-random value based on original coordinates (0.0 to 1.0)
    float rnd = rand(pos.x + pos.y * 100.0 + pos.z * 1000.0);

    float size = 0.0;

    if (weatherMode == 1) { // Rain
        // Falling fast
        pos.y -= time * (15.0 + rnd * 12.0); // Faster drop speed
        // Slight wind drifting
        pos.x += time * 2.5; // More wind
        
        // Brighter and more opaque blue/white
        particleColor = vec4(0.7, 0.8, 1.0, 0.9);
        size = 35.0; // Much thicker and larger
    } 
    else if (weatherMode == 2) { // Snow
        // Slow falling with drifting
        pos.y -= time * (3.0 + rnd * 3.5); // Slightly faster
        pos.x += sin(time * 0.8 + rnd * 10.0) * 2.5 + time * 1.5;
        pos.z += cos(time * 0.6 + rnd * 10.0) * 2.5;
        
        // Brighter white flakes
        particleColor = vec4(1.0, 1.0, 1.0, rnd * 0.6 + 0.4); 
        size = 50.0; // Twice as big
    }
    else {
        // Sun or default - hidden
        pos.y = -1000.0;
        particleColor = vec4(0.0);
        size = 0.0;
    }

    // Wrap around logic: Keep particles looping smoothly around the center (0,0,0)
    // Box dimensions: X/Z in [-30, 30], Y in [0, 40]
    pos.y = mod(pos.y, 40.0);
    pos.x = mod(pos.x + 30.0, 60.0) - 30.0;
    pos.z = mod(pos.z + 30.0, 60.0) - 30.0;

    gl_Position = VP * vec4(pos, 1.0);
    // Perspective scaling constraint to avoid giant pixels near camera
    if (gl_Position.w > 0.001) {
        gl_PointSize = size / gl_Position.w;
    } else {
        gl_PointSize = 0.0;
    }
}