/*
    Shader management and uniform location caching
*/
#include "../headers/shaderManager.hpp"
#include "../headers/glHelpers.hpp"
#include <stdexcept>
#include <iostream>

ShaderManager::~ShaderManager() {
    //Delete all shader programs
    for (auto& pair : m_shaders) {
        glDeleteProgram(pair.second);
    }
}

GLuint ShaderManager::loadShaderProgram(const std::string& name, const std::string& vertPath, const std::string& fragPath) {
    //Check if already loaded
    if (m_shaders.find(name) != m_shaders.end()) {
        std::cerr << "Warning: Shader '" << name << "' already loaded. Returning existing shader." << std::endl;
        return m_shaders[name];
    }

    //Load using GLHelpers
    GLuint program = GLHelpers::loadShaderProgram(vertPath, fragPath);
    m_shaders[name] = program;

    std::cout << "Loaded shader program: " << name << " (ID: " << program << ")" << std::endl;
    return program;
}

GLuint ShaderManager::loadComputeShader(const std::string& name, const std::string& compPath) {
    //Check if already loaded
    if (m_shaders.find(name) != m_shaders.end()) {
        std::cerr << "Warning: Shader '" << name << "' already loaded. Returning existing shader." << std::endl;
        return m_shaders[name];
    }

    //Load using GLHelpers
    GLuint program = GLHelpers::loadComputeShader(compPath);
    m_shaders[name] = program;

    std::cout << "Loaded compute shader: " << name << " (ID: " << program << ")" << std::endl;
    return program;
}

GLuint ShaderManager::getShader(const std::string& name) const {
    auto it = m_shaders.find(name);
    if (it != m_shaders.end()) {
        return it->second;
    }

    std::cerr << "Warning: Shader '" << name << "' not found." << std::endl;
    return 0;
}

GLint ShaderManager::getUniformLocation(const std::string& shaderName, const std::string& uniformName) {
    //Check if location is cached
    auto shaderIt = m_uniformLocations.find(shaderName);
    if (shaderIt != m_uniformLocations.end()) {
        auto uniformIt = shaderIt->second.find(uniformName);
        if (uniformIt != shaderIt->second.end()) {
            return uniformIt->second;
        }
    }

    //Get shader program
    GLuint program = getShader(shaderName);
    if (program == 0) {
        return -1;
    }

    //Query location and cache it
    GLint location = glGetUniformLocation(program, uniformName.c_str());
    m_uniformLocations[shaderName][uniformName] = location;

    if (location == -1) {
        std::cerr << "Warning: Uniform '" << uniformName << "' not found in shader '" << shaderName << "'." << std::endl;
    }

    return location;
}

void ShaderManager::useShader(const std::string& name) {
    GLuint program = getShader(name);
    if (program != 0) {
        glUseProgram(program);
    }
}

void ShaderManager::unbindShader() {
    glUseProgram(0);
}