#version 330 core
layout(location = 0) in vec3 position;

uniform mat4 VP;
uniform float time;
uniform int weatherMode;

out vec4 particleColor;
flat out int particleType;

float rand(float n) { return fract(sin(n) * 43758.5453123); }

void main() {
    vec3 pos = position;
    int id = gl_VertexID;
    float rnd = rand(pos.x + pos.y * 100.0 + pos.z * 1000.0);
    float size = 0.0;
    
    // We'll wrap position only for falling particles
    bool wrap = false;

    if (weatherMode == 0) { // Sunny
        particleType = -1; // hide
        pos.y = -1000.0;
        particleColor = vec4(0.0);
        size = 0.0;
    } 
    else if (weatherMode == 1) { // Rain
        if (id < 2000) {
            // Dark Clouds (many particles)
            particleType = 1;
            pos.x = (rand(float(id) * 1.1) - 0.5) * 80.0;
            pos.y = 10.0 + rand(float(id) * 1.2) * 4.0;
            pos.z = (rand(float(id) * 1.3) - 0.5) * 80.0;
            particleColor = vec4(0.2, 0.2, 0.25, 0.9);
            size = 150.0;
        } else {
            particleType = 2; // Rain
            wrap = true;
            pos.y -= time * (15.0 + rnd * 12.0);
            pos.x += time * 2.5;
            particleColor = vec4(0.7, 0.8, 1.0, 0.9);
            size = 35.0;
        }
    } 
    else if (weatherMode == 2) { // Snow
        if (id < 2000) {
            // White Clouds (many particles)
            particleType = 1;
            pos.x = (rand(float(id) * 1.1) - 0.5) * 80.0;
            pos.y = 10.0 + rand(float(id) * 1.2) * 4.0;
            pos.z = (rand(float(id) * 1.3) - 0.5) * 80.0;
            particleColor = vec4(0.9, 0.9, 0.9, 0.8);
            size = 150.0;
        } else {
            particleType = 3; // Snow
            wrap = true;
            pos.y -= time * (3.0 + rnd * 3.5);
            pos.x += sin(time * 0.8 + rnd * 10.0) * 2.5 + time * 1.5;
            pos.z += cos(time * 0.6 + rnd * 10.0) * 2.5;
            particleColor = vec4(1.0, 1.0, 1.0, rnd * 0.6 + 0.4); 
            size = 50.0;
        }
    }

    if (wrap) {
        pos.y = mod(pos.y, 40.0);
        pos.x = mod(pos.x + 30.0, 60.0) - 30.0;
        pos.z = mod(pos.z + 30.0, 60.0) - 30.0;
    }

    gl_Position = VP * vec4(pos, 1.0);
    
    // Perspective scaling only for Rain and Snow
    if (particleType >= 2) {
        if (gl_Position.w > 0.001) {
            gl_PointSize = size / gl_Position.w;
        } else {
            gl_PointSize = 0.0;
        }
    } else {
        // Sun and Clouds maintain a large fixed pixel size
        gl_PointSize = 60.0; 
    }
}