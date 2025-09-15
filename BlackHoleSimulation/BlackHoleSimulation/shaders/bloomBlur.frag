#version 430 core

//Interpolated values from the vertex shaders
in vec2 TexCoords;

//Output color of the pixel
out vec4 FragColor;

//Input the image to be blurred
uniform sampler2D uImage;

//Input direction for the blur
uniform vec2 uDirection; //(1,0)=horizontal, (0,1)=vertical

//Gaussian weights for a 9-tap blur
const float weights[9] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216, 0.005, 0.002, 0.001, 0.0005);

void main() {
    //Calculate the size of a single texel (pixel in texture space))
    vec2 tex_offset = 1.0 / textureSize(uImage, 0);

    //Start with the center pixel
    vec3 result = texture(uImage, TexCoords).rgb * weights[0];

    //Add the neighboring pixels
    for(int i = 1; i < 9; ++i) {
        result += texture(uImage, TexCoords + uDirection * tex_offset * float(i)).rgb * weights[i];
        result += texture(uImage, TexCoords - uDirection * tex_offset * float(i)).rgb * weights[i];
    }
    //Output the final blurred color
    FragColor = vec4(result, 1.0);
}