//Grid mesh and render

#include "../headers/grid.hpp"
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <stdexcept>

//Simple shader loader
static std::string loadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) throw std::runtime_error("Failed to open file: " + path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

static GLuint compileShader(GLenum type, const std::string& src) {
    GLuint shader = glCreateShader(type);
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

Grid3D::Grid3D(float min, float max, float spacing, float bhRadius)
    : m_vao(0), m_vbo(0), m_vertexCount(0), m_shaderProgram(0)
{
    // Make the well depth and width proportional to the black hole radius
    float wellDepth = bhRadius * 1.2f; // or 1.0f for exact event horizon
    float sigma = bhRadius * 2.5f;     // controls the width of the well

    std::vector<glm::vec3> vertices;

    for (float x = min; x <= max; x += spacing) {
        for (float z = min; z < max; z += spacing) {
            float y1 = -wellDepth * std::exp(-(x * x + z * z) / (2.0f * sigma * sigma));
            float y2 = -wellDepth * std::exp(-(x * x + (z + spacing) * (z + spacing)) / (2.0f * sigma * sigma));
            vertices.push_back({ x, y1, z });
            vertices.push_back({ x, y2, z + spacing });
        }
    }
    for (float z = min; z <= max; z += spacing) {
        for (float x = min; x < max; x += spacing) {
            float y1 = -wellDepth * std::exp(-(x * x + z * z) / (2.0f * sigma * sigma));
            float y2 = -wellDepth * std::exp(-((x + spacing) * (x + spacing) + z * z) / (2.0f * sigma * sigma));
            vertices.push_back({ x, y1, z });
            vertices.push_back({ x + spacing, y2, z });
        }
    }

    m_vertexCount = vertices.size();

    //OpenGL VAO/VBO
    glGenVertexArrays(1, &m_vao);
    glGenBuffers(1, &m_vbo);
    glBindVertexArray(m_vao);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    initShader();
}

Grid3D::~Grid3D() {
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(1, &m_vbo);
    glDeleteProgram(m_shaderProgram);
}

void Grid3D::initShader() {
    std::string vertSrc = loadFile("shaders/grid/shader.vert");
    std::string fragSrc = loadFile("shaders/grid/shader.frag");
    GLuint vert = compileShader(GL_VERTEX_SHADER, vertSrc);
    GLuint frag = compileShader(GL_FRAGMENT_SHADER, fragSrc);
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

void Grid3D::draw(const glm::mat4& view, const glm::mat4& proj) {
    glUseProgram(m_shaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(m_shaderProgram, "uView"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(m_shaderProgram, "uProj"), 1, GL_FALSE, &proj[0][0]);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_vertexCount));
    glBindVertexArray(0);
}