#version 430 core

//vertex attribute: position of the quad(x,y) and texture(u,v) coordinates
layout(location = 0) in vec2 aPos;
layout(location = 1) in vec2 aTexCoord;

//Passing texture coordinates to the fragment shader
out vec2 TexCoords;

void main() {
    TexCoords = aTexCoord;//pass texture coordinates to fragment shader
    gl_Position = vec4(aPos, 0.0, 1.0);//Set the vertex position in clip space
}
