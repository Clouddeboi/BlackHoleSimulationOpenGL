/*
    Utility functions for loading and compiling OpenGL shaders.
*/

#include "../headers/glHelpers.hpp"
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <iostream>
#include <stb_image.h>

//===== File I/O =====

std::string GLHelpers::loadTextFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + path);
    }
    std::stringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

//===== Shader Compilation =====

GLuint GLHelpers::compileShader(GLenum type, const std::string& source, const std::string& debugName) {
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    //Check compilation status
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLchar infoLog[1024];
        glGetShaderInfoLog(shader, 1024, nullptr, infoLog);

        //Determine shader type name
        const char* typeStr = (type == GL_VERTEX_SHADER) ? "VERTEX" :
            (type == GL_FRAGMENT_SHADER) ? "FRAGMENT" :
            (type == GL_COMPUTE_SHADER) ? "COMPUTE" : "UNKNOWN";

        std::string msg = "Shader compilation failed [" + std::string(typeStr) + "]";
        if (!debugName.empty()) {
            msg += " (" + debugName + ")";
        }
        msg += ":\n" + std::string(infoLog);

        glDeleteShader(shader);
        throw std::runtime_error(msg);
    }

    return shader;
}

//===== Program Linking =====

GLuint GLHelpers::linkProgram(const std::vector<GLuint>& shaders, const std::string& debugName) {
    GLuint program = glCreateProgram();

    //Attach all shaders
    for (GLuint shader : shaders) {
        glAttachShader(program, shader);
    }

    //Link program
    glLinkProgram(program);

    //Check linking status
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLchar infoLog[1024];
        glGetProgramInfoLog(program, 1024, nullptr, infoLog);

        std::string msg = "Shader program linking failed";
        if (!debugName.empty()) {
            msg += " (" + debugName + ")";
        }
        msg += ":\n" + std::string(infoLog);

        glDeleteProgram(program);
        throw std::runtime_error(msg);
    }

    return program;
}

//===== Convenience Functions =====

GLuint GLHelpers::loadShaderProgram(const std::string& vertPath, const std::string& fragPath) {
    //Load shader source files
    std::string vertSrc = loadTextFile(vertPath);
    std::string fragSrc = loadTextFile(fragPath);

    //Compile shaders
    GLuint vertShader = compileShader(GL_VERTEX_SHADER, vertSrc, vertPath);
    GLuint fragShader = compileShader(GL_FRAGMENT_SHADER, fragSrc, fragPath);

    //Link program
    GLuint program = linkProgram({ vertShader, fragShader }, vertPath + " + " + fragPath);

    //Clean up shaders (no longer needed after linking)
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    return program;
}

GLuint GLHelpers::loadComputeShader(const std::string& compPath) {
    //Load shader source
    std::string computeSrc = loadTextFile(compPath);

    //Compile shader
    GLuint computeShader = compileShader(GL_COMPUTE_SHADER, computeSrc, compPath);

    //Link program
    GLuint program = linkProgram({ computeShader }, compPath);

    //Clean up shader
    glDeleteShader(computeShader);

    return program;
}

//===== Texture Loading =====

GLuint GLHelpers::loadTexture(const std::string& path) {
    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4); //Force RGBA

    if (!data) {
        throw std::runtime_error("Failed to load texture: " + path);
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);

    //Default texture parameters
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);

    return texture;
}


//===== Framebuffer Validation =====

void GLHelpers::checkFramebufferComplete(const std::string& name) {
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    if (status != GL_FRAMEBUFFER_COMPLETE) {
        std::string error = "Framebuffer incomplete: " + name + " - ";

        switch (status) {
        case GL_FRAMEBUFFER_UNDEFINED:
            error += "UNDEFINED (default framebuffer doesn't exist)";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            error += "INCOMPLETE_ATTACHMENT (attachment point incomplete)";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            error += "MISSING_ATTACHMENT (no images attached)";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
            error += "INCOMPLETE_DRAW_BUFFER (draw buffer incomplete)";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
            error += "INCOMPLETE_READ_BUFFER (read buffer incomplete)";
            break;
        case GL_FRAMEBUFFER_UNSUPPORTED:
            error += "UNSUPPORTED (format combination not supported)";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            error += "INCOMPLETE_MULTISAMPLE (sample counts don't match)";
            break;
        case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
            error += "INCOMPLETE_LAYER_TARGETS (layered attachments incomplete)";
            break;
        default:
            error += "UNKNOWN (" + std::to_string(status) + ")";
            break;
        }

        throw std::runtime_error(error);
    }
}