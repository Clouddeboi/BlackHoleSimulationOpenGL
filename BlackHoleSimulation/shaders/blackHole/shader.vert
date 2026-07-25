#version 330 core
layout (location = 0) in vec3 aPos;
uniform vec3 uOffset;
uniform mat4 uMVP;

void main() {
    vec4 worldPos = vec4(aPos + uOffset, 1.0);
    gl_Position = uMVP * worldPos;
}
