#pragma once
#include <glad/glad.h>
#include <string>

namespace GLHelpers {
    GLuint loadShaderProgram(const std::string& vertPath, const std::string& fragPath);
    GLuint loadComputeShader(const std::string& compPath);
}
