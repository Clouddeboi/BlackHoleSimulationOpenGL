#version 430 core
in vec2 TexCoords;
out vec4 FragColor;
uniform sampler2D uRenderTex;
uniform float uThreshold;

void main() {
    vec3 color = texture(uRenderTex, TexCoords).rgb;
    float brightness = max(max(color.r, color.g), color.b);
    FragColor = brightness > uThreshold ? vec4(color, 1.0) : vec4(0.0);
}