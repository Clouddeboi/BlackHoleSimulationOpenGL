#version 330 core
out vec4 FragColor;

//Color set from C++
uniform vec4 uColor;

void main() {
    FragColor = vec4(0.9490196078431372, 0.6980392156862745, 0.4627450980392157, 1.0);
}
