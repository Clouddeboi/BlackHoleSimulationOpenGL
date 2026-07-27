/*
	Generates a 3D grid with a gravitational well effect.
*/

#include "../headers/grid.hpp"
#include "../headers/glHelpers.hpp"
#include <vector>

Grid3D::Grid3D(float min, float max, float spacing, float bhRadius)
    : m_vao(0), m_vbo(0), m_vertexCount(0), m_shaderProgram(0)
{
    //More physical well
    //y = -wellDepth / r (Newtonian/Schwarzschild-like)
    float wellDepth = bhRadius * 5.0f;

	//Store vertices in a vector
    std::vector<glm::vec3> vertices;

    //X lines (varying x, fixed z)
    for (float x = min; x <= max; x += spacing) {
        for (float z = min; z < max; z += spacing) {
			float r1 = sqrt(x * x + z * z);//Distance from origin
            float r2 = sqrt(x * x + (z + spacing) * (z + spacing));
            float y1 = (r1 > 0.01f) ? -wellDepth / r1 : -wellDepth * 100.0f;
            float y2 = (r2 > 0.01f) ? -wellDepth / r2 : -wellDepth * 100.0f;
			vertices.push_back({ x, y1, z });//First point
			vertices.push_back({ x, y2, z + spacing });//Second point
        }
    }
    //Z lines (varying z, fixed x)
    for (float z = min; z <= max; z += spacing) {
        for (float x = min; x < max; x += spacing) {
            float r1 = sqrt(x * x + z * z);
            float r2 = sqrt((x + spacing) * (x + spacing) + z * z);
            float y1 = (r1 > 0.01f) ? -wellDepth / r1 : -wellDepth * 100.0f;
            float y2 = (r2 > 0.01f) ? -wellDepth / r2 : -wellDepth * 100.0f;
            vertices.push_back({ x, y1, z });
            vertices.push_back({ x + spacing, y2, z });
        }
    }

	//Store vertex count
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

//----------------- Destructor -----------------
Grid3D::~Grid3D() {
    glDeleteVertexArrays(1, &m_vao);
    glDeleteBuffers(1, &m_vbo);
    glDeleteProgram(m_shaderProgram);
}

//----------------- Init Shader -----------------
//----------------- Init Shader -----------------
void Grid3D::initShader() {
    m_shaderProgram = GLHelpers::loadShaderProgram("shaders/grid/shader.vert", "shaders/grid/shader.frag");
}

//----------------- Draw -----------------
void Grid3D::draw(const glm::mat4& view, const glm::mat4& proj) {
    glUseProgram(m_shaderProgram);
    glUniform3f(glGetUniformLocation(m_shaderProgram, "uGridColor"), 0.0f, 0.0f, 0.0f);
    glUniformMatrix4fv(glGetUniformLocation(m_shaderProgram, "uView"), 1, GL_FALSE, &view[0][0]);
    glUniformMatrix4fv(glGetUniformLocation(m_shaderProgram, "uProj"), 1, GL_FALSE, &proj[0][0]);
    glBindVertexArray(m_vao);
    glDrawArrays(GL_LINES, 0, static_cast<GLsizei>(m_vertexCount));
    glBindVertexArray(0);
}