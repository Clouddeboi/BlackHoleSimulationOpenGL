#include "../headers/textureManager.hpp"
#include "../headers/glHelpers.hpp"
#include <stdexcept>
#include <iostream>

TextureManager::~TextureManager() {
    for (auto& pair : m_textures) {
        glDeleteTextures(1, &pair.second);
    }
    m_textures.clear();
}

GLuint TextureManager::loadTexture(const std::string& name, const std::string& filepath, bool sRGB) {
    if (name.empty()) {
        throw std::runtime_error("Texture name cannot be empty");
    }

    if (m_textures.find(name) != m_textures.end()) {
        std::cerr << "Warning: Texture '" << name << "' already loaded, returning cached version" << std::endl;
        return m_textures[name];
    }

    try {
        GLuint texture = GLHelpers::loadTexture(filepath, sRGB);
        m_textures[name] = texture;
        return texture;
    }
    catch (const std::exception& e) {
        throw std::runtime_error("TextureManager failed to load '" + name + "': " + e.what());
    }
}

GLuint TextureManager::createRenderTexture(const std::string& name, int width, int height,
    GLenum internalFormat, GLenum format, GLenum type) {
    if (name.empty()) {
        throw std::runtime_error("Render texture name cannot be empty");
    }

    if (width <= 0 || height <= 0) {
        throw std::runtime_error("Invalid render texture dimensions: " +
            std::to_string(width) + "x" + std::to_string(height));
    }

    if (m_textures.find(name) != m_textures.end()) {
        std::cerr << "Warning: Texture '" << name << "' already exists, returning cached version" << std::endl;
        return m_textures[name];
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    if (texture == 0) {
        throw std::runtime_error("Failed to generate render texture object for: " + name);
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, nullptr);

    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        glDeleteTextures(1, &texture);
        throw std::runtime_error("OpenGL error during render texture creation (" +
            std::to_string(error) + ") for: " + name);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);
    m_textures[name] = texture;

    std::cout << "Created render texture: " << name
        << " (" << width << "x" << height << ")" << std::endl;

    return texture;
}

GLuint TextureManager::getTexture(const std::string& name) const {
    auto it = m_textures.find(name);
    if (it == m_textures.end()) {
        throw std::runtime_error("Texture '" + name + "' not found in TextureManager");
    }
    return it->second;
}

bool TextureManager::hasTexture(const std::string& name) const {
    return m_textures.find(name) != m_textures.end();
}

void TextureManager::bindTexture(const std::string& name, GLenum target) {
    glBindTexture(target, getTexture(name));
}

void TextureManager::unbindTexture(GLenum target) {
    glBindTexture(target, 0);
}