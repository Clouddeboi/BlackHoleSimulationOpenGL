#version 430 core
in vec2 TexCoords;
out vec4 FragColor;
uniform sampler2D uRenderTex;
uniform sampler2D uBloomTex;
uniform float uBloomStrength;

void main() {
    vec3 scene = texture(uRenderTex, TexCoords).rgb;
    vec3 bloom = texture(uBloomTex, TexCoords).rgb;
    FragColor = vec4(scene + bloom * uBloomStrength, 1.0);
}