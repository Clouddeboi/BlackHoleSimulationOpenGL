#pragma once
#include <glad/glad.h>
#include <string>
#include <vector>

namespace GLHelpers {
    //File I/O
    std::string loadTextFile(const std::string& path);

    //Shader compilation
    GLuint compileShader(GLenum type, const std::string& source, const std::string& debugName = "");

    //Program linking
    GLuint linkProgram(const std::vector<GLuint>& shaders, const std::string& debugName = "");

    //Convenience functions
    GLuint loadShaderProgram(const std::string& vertPath, const std::string& fragPath);
    GLuint loadComputeShader(const std::string& compPath);

    //Texture loading
    GLuint loadTexture(const std::string& path, bool sRGB = false);

    //Framebuffer validation
    void checkFramebufferComplete(const std::string& name);
}