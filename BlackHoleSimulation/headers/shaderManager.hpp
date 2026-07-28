#pragma once
#include <glad/glad.h>
#include <string>
#include <unordered_map>

//Provides centralized shader loading, compilation, and uniform location caching, 
//shaders are loaded once and can be reused by name. Manages GPU resources and prevents copying.
class ShaderManager {
public:
    ShaderManager() = default;
    ~ShaderManager();

    //Prevent copying (manages GPU resources)
    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;

    GLuint loadShaderProgram(const std::string& name, const std::string& vertPath, const std::string& fragPath);
    GLuint loadComputeShader(const std::string& name, const std::string& compPath);

    GLuint getShader(const std::string& name) const;
    GLint getUniformLocation(const std::string& shaderName, const std::string& uniformName);

    void useShader(const std::string& name);

    //Unbind any shader program
    void unbindShader();

private:
    std::unordered_map<std::string, GLuint> m_shaders;
    std::unordered_map<std::string, std::unordered_map<std::string, GLint>> m_uniformLocations;
};