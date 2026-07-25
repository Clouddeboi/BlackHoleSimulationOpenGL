#version 430 core
in vec2 TexCoords;
out vec4 FragColor;

//Input the image to extract bright areas from
uniform sampler2D uRenderTex;

//Threshold for brightness
uniform float uThreshold;

void main() {
    //Sample the color from the input texture
    vec3 color = texture(uRenderTex, TexCoords).rgb;

    //Calculate brightness as the maximum color channel
    float brightness = max(max(color.r, color.g), color.b);

    //Output the color if above the threshold, otherwise output black
    FragColor = brightness > uThreshold ? vec4(color, 1.0) : vec4(0.0);
}