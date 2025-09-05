#version 430 core
in vec2 TexCoords;
out vec4 FragColor;
uniform sampler2D uImage;
uniform vec2 uDirection; // (1,0)=horizontal, (0,1)=vertical

const float weights[9] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216, 0.005, 0.002, 0.001, 0.0005);

void main() {
    vec2 tex_offset = 1.0 / textureSize(uImage, 0); // gets size of single texel
    vec3 result = texture(uImage, TexCoords).rgb * weights[0];
    for(int i = 1; i < 9; ++i) {
        result += texture(uImage, TexCoords + uDirection * tex_offset * float(i)).rgb * weights[i];
        result += texture(uImage, TexCoords - uDirection * tex_offset * float(i)).rgb * weights[i];
    }
    FragColor = vec4(result, 1.0);
}