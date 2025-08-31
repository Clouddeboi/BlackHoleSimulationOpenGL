//Rendering

#include "../headers/renderer.hpp"
#include <glad/glad.h>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <sstream>

//Utility function to load shaders
static std::string loadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) throw std::runtime_error("Failed to open file: " + path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

static unsigned int compileShader(unsigned int type, const std::string& src) {
    unsigned int shader = glCreateShader(type);
    const char* csrc = src.c_str();
    glShaderSource(shader, 1, &csrc, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[512];
        glGetShaderInfoLog(shader, 512, nullptr, info);
        throw std::runtime_error("Shader compile error: " + std::string(info));
    }
    return shader;
}

//----------------- Constructor -----------------
Renderer::Renderer(int width, int height)
    : m_quadVAO(0), m_quadVBO(0), m_shaderProgram(0)
{
    initFullscreenQuad();
    initShaders();
    initUBO();
}

//----------------- Destructor -----------------
Renderer::~Renderer() {
    glDeleteProgram(m_shaderProgram);
    glDeleteVertexArrays(1, &m_quadVAO);
    glDeleteBuffers(1, &m_quadVBO);
}

void Renderer::initUBO() {
    glGenBuffers(1, &m_cameraUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_cameraUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(CameraUBO), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_cameraUBO);//binding=0
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

//----------------- Fullscreen Quad -----------------
void Renderer::initFullscreenQuad() {
    float quadVertices[] = {
        //positions   //texcoords
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 1.0f
    };
    unsigned int indices[] = { 0, 1, 2, 2, 3, 0 };

    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    unsigned int EBO;
    glGenBuffers(1, &EBO);

    glBindVertexArray(m_quadVAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

//----------------- Shaders -----------------
void Renderer::initShaders() {
    std::string vertSrc = loadFile("shaders/blit.vert");
    std::string fragSrc = loadFile("shaders/blit.frag");

    unsigned int vert = compileShader(GL_VERTEX_SHADER, vertSrc);
    unsigned int frag = compileShader(GL_FRAGMENT_SHADER, fragSrc);

    m_shaderProgram = glCreateProgram();
    glAttachShader(m_shaderProgram, vert);
    glAttachShader(m_shaderProgram, frag);
    glLinkProgram(m_shaderProgram);

    int success;
    glGetProgramiv(m_shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char info[512];
        glGetProgramInfoLog(m_shaderProgram, 512, nullptr, info);
        throw std::runtime_error("Shader linking error: " + std::string(info));
    }

    glDeleteShader(vert);
    glDeleteShader(frag);
}

//----------------- Render -----------------
void Renderer::render(const Camera& camera) {
    CameraUBO data = camera.getUBO();
    glBindBuffer(GL_UNIFORM_BUFFER, m_cameraUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(CameraUBO), &data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    // Render quad
    glUseProgram(m_shaderProgram);
    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
}
