#version 430 core

//Interpolated values from the vertex shaders
in vec2 TexCoords;

//Output color of the pixel
out vec4 FragColor;

//Input texture
//main render and bloom
uniform sampler2D uRenderTex;
uniform sampler2D uBloomTex;
uniform float uBloomStrength;

void main() {
    //Sample the main scene color
    vec3 scene = texture(uRenderTex, TexCoords).rgb;

    //Sample the bloom texture
    vec3 bloom = texture(uBloomTex, TexCoords).rgb;

    //Combine the scene and bloom, apply bloom strength, output final color
    FragColor = vec4(scene + bloom * uBloomStrength, 1.0);
}