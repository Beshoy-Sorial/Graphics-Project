#version 330 core

// Shadow map pass: we only need the depth of every object as seen from the light
layout(location = 0) in vec3 position;

uniform mat4 transform; // light view-projection * model

void main(){
    gl_Position = transform * vec4(position, 1.0);
}
