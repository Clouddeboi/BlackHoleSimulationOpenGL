#pragma once
#include <string>
#include <unordered_map>
#include <glad/glad.h>

class TextureManager {
public:
    TextureManager() = default;
    ~TextureManager();

    //Prevent copying
    TextureManager(const TextureManager&) = delete;
    TextureManager& operator=(const TextureManager&) = delete;

    //Load and cache textures
    GLuint loadTexture(const std::string& name, const std::string& path);

    //Create blank textures for render targets
    GLuint createRenderTexture(const std::string& name, int width, int height, GLenum internalFormat, GLenum format, GLenum type);

    //Retrieve cached textures
    GLuint getTexture(const std::string& name) const;
    bool hasTexture(const std::string& name) const;

    // Texture binding helpers
    void bindTexture(const std::string& name, GLenum target = GL_TEXTURE_2D);
    void unbindTexture(GLenum target = GL_TEXTURE_2D);

private:
    std::unordered_map<std::string, GLuint> m_textures;
};