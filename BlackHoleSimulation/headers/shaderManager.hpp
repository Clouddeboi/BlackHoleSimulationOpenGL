#pragma once
#include <string>
#include <unordered_map>
#include <glad/glad.h>

class ShaderManager {
public:
    ShaderManager() = default;
    ~ShaderManager();

    //Prevent copying
    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;

    //Load and cache shader programs
    GLuint loadShaderProgram(const std::string& name, const std::string& vertPath, const std::string& fragPath);
    GLuint loadComputeShader(const std::string& name, const std::string& compPath);

    //Retrieve cached shaders
    GLuint getShader(const std::string& name) const;
    bool hasShader(const std::string& name) const;

    //Uniform location caching
    GLint getUniformLocation(const std::string& shaderName, const std::string& uniformName);

    //Shader binding helpers
    void useShader(const std::string& name);
    void unbindShader();

private:
    std::unordered_map<std::string, GLuint> m_shaders;
    std::unordered_map<std::string, std::unordered_map<std::string, GLint>> m_uniformLocations;
};