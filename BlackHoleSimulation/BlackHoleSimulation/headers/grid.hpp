#pragma once
#include <vector>
#include <glad/glad.h>
#include <glm/glm.hpp>
#include "camera.hpp"

class Grid3D {
public:
    Grid3D(float min, float max, float spacing, float bhRadius);
    ~Grid3D();

    void draw(const glm::mat4& view, const glm::mat4& proj);

private:
    GLuint m_vao, m_vbo;
    size_t m_vertexCount;
    GLuint m_shaderProgram;

    void initShader();
};