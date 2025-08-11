#version 330 core
out vec4 FragColor;

//Color set from C++
uniform vec4 uColor;

void main() {
    FragColor = uColor;
}
