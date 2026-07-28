#include "../headers/shaderManager.hpp"
#include "../headers/glHelpers.hpp"
#include <stdexcept>
#include <iostream>

ShaderManager::~ShaderManager() {
    for (auto& pair : m_shaders) {
        glDeleteProgram(pair.second);
    }
    m_shaders.clear();
    m_uniformLocations.clear();
}

GLuint ShaderManager::loadShaderProgram(const std::string& name, const std::string& vertPath, const std::string& fragPath) {
    if (m_shaders.find(name) != m_shaders.end()) {
        std::cerr << "Warning: Shader program '" << name << "' already loaded. Returning cached version.\n";
        return m_shaders[name];
    }

    GLuint program = GLHelpers::loadShaderProgram(vertPath, fragPath);
    m_shaders[name] = program;
    return program;
}

GLuint ShaderManager::loadComputeShader(const std::string& name, const std::string& compPath) {
    if (m_shaders.find(name) != m_shaders.end()) {
        std::cerr << "Warning: Compute shader '" << name << "' already loaded. Returning cached version.\n";
        return m_shaders[name];
    }

    GLuint program = GLHelpers::loadComputeShader(compPath);
    m_shaders[name] = program;
    return program;
}

GLuint ShaderManager::getShader(const std::string& name) const {
    auto it = m_shaders.find(name);
    if (it == m_shaders.end()) {
        throw std::runtime_error("Shader '" + name + "' not found in ShaderManager");
    }
    return it->second;
}

bool ShaderManager::hasShader(const std::string& name) const {
    return m_shaders.find(name) != m_shaders.end();
}

GLint ShaderManager::getUniformLocation(const std::string& shaderName, const std::string& uniformName) {
    //Check if uniform location is already cached
    auto shaderIt = m_uniformLocations.find(shaderName);
    if (shaderIt != m_uniformLocations.end()) {
        auto uniformIt = shaderIt->second.find(uniformName);
        if (uniformIt != shaderIt->second.end()) {
            return uniformIt->second;
        }
    }

    //Query uniform location and cache it
    GLuint shader = getShader(shaderName);
    GLint location = glGetUniformLocation(shader, uniformName.c_str());
    if (location == -1) {
        std::cerr << "Warning: Uniform '" << uniformName << "' not found in shader '" << shaderName << "'\n";
    }
    m_uniformLocations[shaderName][uniformName] = location;
    return location;
}

void ShaderManager::useShader(const std::string& name) {
    glUseProgram(getShader(name));
}

void ShaderManager::unbindShader() {
    glUseProgram(0);
}