#version 330 core
layout (location = 0) in vec2 aPos;

//Offset applied to some draws (we set this from C++ per-object)
uniform vec2 uOffset;

void main() {
    //Add offset in the same coordinate space we use for vertices
    vec2 pos = aPos + uOffset;
    gl_Position = vec4(pos, 0.0, 1.0);
}
