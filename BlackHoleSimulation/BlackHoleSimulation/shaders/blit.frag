#version 450 core
out vec4 FragColor;
in vec2 TexCoord;

void main() {
    // Gradient debug
    FragColor = vec4(TexCoord, 0.5, 1.0);
}
