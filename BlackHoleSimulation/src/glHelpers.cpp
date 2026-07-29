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
std::string GLHelpers::loadTextFile(const std::string& filepath) {
    if (filepath.empty()) {
        throw std::runtime_error("Empty filepath provided to loadTextFile");
    }

    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + filepath);
    }

    std::stringstream buffer;
    buffer << file.rdbuf();

    std::string content = buffer.str();
    if (content.empty()) {
        std::cerr << "Warning: File is empty: " << filepath << std::endl;
    }

    file.close();
    return content;
}

//===== Shader Compilation =====
GLuint GLHelpers::compileShader(GLenum type, const std::string& source, const std::string& debugName) {
    GLuint shader = glCreateShader(type);
    if (shader == 0) {
        throw std::runtime_error("Failed to create shader object");
    }

    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        GLint logLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

        std::vector<char> errorLog(logLength > 0 ? logLength : 1);
        glGetShaderInfoLog(shader, logLength, nullptr, errorLog.data());

        std::string shaderType = (type == GL_VERTEX_SHADER) ? "VERTEX" :
            (type == GL_FRAGMENT_SHADER) ? "FRAGMENT" :
            (type == GL_COMPUTE_SHADER) ? "COMPUTE" : "UNKNOWN";

        std::string context = debugName.empty() ? "" : " [" + debugName + "]";

        glDeleteShader(shader);

        throw std::runtime_error("Shader compilation failed (" + shaderType + context + "):\n" +
            std::string(errorLog.data()));
    }

    return shader;
}

//===== Program Linking =====
GLuint GLHelpers::linkProgram(const std::vector<GLuint>& shaders, const std::string& debugName) {
    if (shaders.empty()) {
        throw std::runtime_error("No shaders provided to linkProgram");
    }

    for (GLuint shader : shaders) {
        if (shader == 0) {
            throw std::runtime_error("Invalid shader handle (0) provided to linkProgram");
        }
    }

    GLuint program = glCreateProgram();
    if (program == 0) {
        throw std::runtime_error("Failed to create shader program object");
    }

    //Attach all shaders
    for (GLuint shader : shaders) {
        glAttachShader(program, shader);
    }

    glLinkProgram(program);

    GLint success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        GLint logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);

        std::vector<char> errorLog(logLength > 0 ? logLength : 1);
        glGetProgramInfoLog(program, logLength, nullptr, errorLog.data());

        std::string context = debugName.empty() ? "" : " [" + debugName + "]";

        glDeleteProgram(program);

        throw std::runtime_error("Shader program linking failed" + context + ":\n" +
            std::string(errorLog.data()));
    }

    //Validate program
    glValidateProgram(program);
    GLint validated = 0;
    glGetProgramiv(program, GL_VALIDATE_STATUS, &validated);
    if (!validated) {
        GLint logLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);

        std::vector<char> errorLog(logLength > 0 ? logLength : 1);
        glGetProgramInfoLog(program, logLength, nullptr, errorLog.data());

        std::string context = debugName.empty() ? "" : " [" + debugName + "]";

        std::cerr << "Warning: Shader program validation failed" << context << ":\n"
            << errorLog.data() << std::endl;
        //Don't throw here, validation can fail in some valid contexts
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

GLuint GLHelpers::loadComputeShader(const std::string& computePath) {
    if (computePath.empty()) {
        throw std::runtime_error("Empty compute shader path provided");
    }

    std::string computeSource = loadTextFile(computePath);
    if (computeSource.empty()) {
        throw std::runtime_error("Failed to load compute shader from: " + computePath);
    }

    GLuint computeShader = 0;
    GLuint program = 0;

    try {
        computeShader = compileShader(GL_COMPUTE_SHADER, computeSource);

        program = glCreateProgram();
        if (program == 0) {
            throw std::runtime_error("Failed to create compute program object");
        }

        glAttachShader(program, computeShader);
        glLinkProgram(program);

        GLint success = 0;
        glGetProgramiv(program, GL_LINK_STATUS, &success);
        if (!success) {
            GLint logLength = 0;
            glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLength);

            std::vector<char> errorLog(logLength > 0 ? logLength : 1);
            glGetProgramInfoLog(program, logLength, nullptr, errorLog.data());

            throw std::runtime_error("Compute shader linking failed:\n" +
                std::string(errorLog.data()));
        }

        glDeleteShader(computeShader);
        return program;

    }
    catch (...) {
        if (computeShader != 0) glDeleteShader(computeShader);
        if (program != 0) glDeleteProgram(program);
        throw;
    }
}

//===== Texture Loading =====
GLuint GLHelpers::loadTexture(const std::string& filepath, bool sRGB) {
    if (filepath.empty()) {
        throw std::runtime_error("Empty texture filepath provided");
    }

    int width = 0, height = 0, channels = 0;
    unsigned char* data = stbi_load(filepath.c_str(), &width, &height, &channels, 0);

    if (!data) {
        const char* reason = stbi_failure_reason();
        throw std::runtime_error("Failed to load texture '" + filepath + "': " +
            (reason ? reason : "Unknown error"));
    }

    if (width <= 0 || height <= 0) {
        stbi_image_free(data);
        throw std::runtime_error("Invalid texture dimensions for: " + filepath);
    }

    if (channels < 1 || channels > 4) {
        stbi_image_free(data);
        throw std::runtime_error("Unsupported channel count (" + std::to_string(channels) +
            ") for texture: " + filepath);
    }

    GLenum format = GL_RGB;
    GLenum internalFormat = sRGB ? GL_SRGB8 : GL_RGB8;

    switch (channels) {
    case 1:
        format = GL_RED;
        internalFormat = GL_R8;
        break;
    case 2:
        format = GL_RG;
        internalFormat = GL_RG8;
        break;
    case 3:
        format = GL_RGB;
        internalFormat = sRGB ? GL_SRGB8 : GL_RGB8;
        break;
    case 4:
        format = GL_RGBA;
        internalFormat = sRGB ? GL_SRGB8_ALPHA8 : GL_RGBA8;
        break;
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    if (texture == 0) {
        stbi_image_free(data);
        throw std::runtime_error("Failed to generate texture object for: " + filepath);
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, GL_UNSIGNED_BYTE, data);

    //Check for OpenGL errors during texture upload
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        glDeleteTextures(1, &texture);
        stbi_image_free(data);
        throw std::runtime_error("OpenGL error during texture upload (" +
            std::to_string(error) + ") for: " + filepath);
    }

    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    glBindTexture(GL_TEXTURE_2D, 0);

    std::cout << "Successfully loaded texture: " << filepath
        << " (" << width << "x" << height << ", " << channels << " channels)" << std::endl;

    return texture;
}

//===== Framebuffer Validation =====

void GLHelpers::checkFramebufferComplete(const std::string& context) {
    GLenum status = glCheckFramebufferStatus(GL_FRAMEBUFFER);

    if (status == GL_FRAMEBUFFER_COMPLETE) {
        return;
    }

    std::string errorMsg = "Framebuffer incomplete";
    if (!context.empty()) {
        errorMsg += " (" + context + ")";
    }
    errorMsg += ": ";

    switch (status) {
    case GL_FRAMEBUFFER_UNDEFINED:
        errorMsg += "GL_FRAMEBUFFER_UNDEFINED - default framebuffer does not exist";
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
        errorMsg += "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT - attachment point uninitialized or invalid";
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
        errorMsg += "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT - no images attached";
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
        errorMsg += "GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER - draw buffer mismatch";
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
        errorMsg += "GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER - read buffer mismatch";
        break;
    case GL_FRAMEBUFFER_UNSUPPORTED:
        errorMsg += "GL_FRAMEBUFFER_UNSUPPORTED - format combination not supported";
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
        errorMsg += "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE - multisample mismatch";
        break;
    case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
        errorMsg += "GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS - layered attachment mismatch";
        break;
    default:
        errorMsg += "Unknown error (0x" + std::to_string(status) + ")";
        break;
    }

    throw std::runtime_error(errorMsg);
}