#version 430 core

layout(std140, binding = 0) uniform CameraBlock {
    mat4 view;
    mat4 proj;
    vec4 camPos;
};

out vec4 FragColor;

void main() {
    // Simple color based on camera position
    float strength = length(camPos.xyz) * 0.1;
    vec3 col = vec3(0.2, 0.4, 0.8) + strength * vec3(0.1, 0.3, 0.2);

    FragColor = vec4(col, 1.0);
}
