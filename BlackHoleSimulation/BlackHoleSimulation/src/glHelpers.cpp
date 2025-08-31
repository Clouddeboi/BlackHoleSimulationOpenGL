//OpenGL utils

#include "../headers/glHelpers.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

// Utility: read file contents
static std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open shader file: " << path << std::endl;
        return "";
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

GLuint GLHelpers::loadShaderProgram(const std::string& vertPath, const std::string& fragPath) {
    std::string vsrc = readFile(vertPath);
    std::string fsrc = readFile(fragPath);

    const char* vsrcC = vsrc.c_str();
    const char* fsrcC = fsrc.c_str();

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vsrcC, nullptr);
    glCompileShader(vs);

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fsrcC, nullptr);
    glCompileShader(fs);

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);

    glDeleteShader(vs);
    glDeleteShader(fs);

    return prog;
}

GLuint GLHelpers::loadComputeShader(const std::string& compPath) {
    std::string csrc = readFile(compPath);
    const char* csrcC = csrc.c_str();

    GLuint cs = glCreateShader(GL_COMPUTE_SHADER);
    glShaderSource(cs, 1, &csrcC, nullptr);
    glCompileShader(cs);

    GLint success;
    glGetShaderiv(cs, GL_COMPILE_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetShaderInfoLog(cs, 512, nullptr, infoLog);
        std::cerr << "Compute shader compile error:\n" << infoLog << std::endl;
    }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, cs);
    glLinkProgram(prog);

    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if (!success) {
        char infoLog[512];
        glGetProgramInfoLog(prog, 512, nullptr, infoLog);
        std::cerr << "Compute shader link error:\n" << infoLog << std::endl;
    }

    glDeleteShader(cs);
    return prog;
}
