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

GLuint TextureManager::loadTexture(const std::string& name, const std::string& path) {
    if (m_textures.find(name) != m_textures.end()) {
        std::cerr << "Warning: Texture '" << name << "' already loaded. Returning cached version.\n";
        return m_textures[name];
    }

    GLuint texture = GLHelpers::loadTexture(path);
    m_textures[name] = texture;
    return texture;
}

GLuint TextureManager::createRenderTexture(const std::string& name, int width, int height,
    GLenum internalFormat, GLenum format, GLenum type) {
    if (m_textures.find(name) != m_textures.end()) {
        std::cerr << "Warning: Render texture '" << name << "' already exists. Returning cached version.\n";
        return m_textures[name];
    }

    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, internalFormat, width, height, 0, format, type, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_textures[name] = texture;
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