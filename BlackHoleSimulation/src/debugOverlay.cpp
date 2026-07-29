#include "../headers/debugOverlay.hpp"
#include "../headers/shaderManager.hpp"
#include <stb_easy_font.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

DebugOverlay::DebugOverlay(int width, int height, ShaderManager& shaderMgr)
    : m_width(width), m_height(height), m_shaderMgr(shaderMgr), m_visible(true),
    m_textVAO(0), m_textVBO(0) {
}

DebugOverlay::~DebugOverlay() {
    if (m_textVAO) glDeleteVertexArrays(1, &m_textVAO);
    if (m_textVBO) glDeleteBuffers(1, &m_textVBO);
}

void DebugOverlay::init() {
    //Load debug text shader
    if (!m_shaderMgr.hasShader("debug_text")) {
        m_shaderMgr.loadShaderProgram("debug_text", "shaders/debugtext/text.vert", "shaders/debugtext/text.frag");
    }
    initBuffers();
}

void DebugOverlay::initBuffers() {
    glGenVertexArrays(1, &m_textVAO);
    glGenBuffers(1, &m_textVBO);
    glBindVertexArray(m_textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_textVBO);
    glBufferData(GL_ARRAY_BUFFER, 99999 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glBindVertexArray(0);
}

void DebugOverlay::renderText(const std::vector<std::string>& lines) {
    if (!m_visible || lines.empty()) return;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_shaderMgr.useShader("debug_text");

    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(m_width),
        static_cast<float>(m_height), 0.0f);
    glUniformMatrix4fv(m_shaderMgr.getUniformLocation("debug_text", "uOrtho"),
        1, GL_FALSE, glm::value_ptr(projection));
    glUniform3f(m_shaderMgr.getUniformLocation("debug_text", "uColor"),
        1.0f, 1.0f, 0.0f);

    glBindVertexArray(m_textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_textVBO);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    float x = 10.0f;
    float y = 30.0f;
    char buffer[99999];
    std::vector<float> vertices;

    //Accumulate all text vertices
    for (const auto& line : lines) {
        int quads = stb_easy_font_print(x, y, const_cast<char*>(line.c_str()),
            nullptr, buffer, sizeof(buffer));

        float* buf = reinterpret_cast<float*>(buffer);

        //Extract X,Y from each vertex (stride of 4 floats per vertex)
        for (int i = 0; i < quads * 4; ++i) {
            vertices.push_back(buf[i * 4 + 0]);//X
            vertices.push_back(buf[i * 4 + 1]);//Y
        }

        y += 20.0f;
    }

    if (vertices.empty()) return;

    //Draw each quad as a triangle fan (4 vertices per quad)
    for (size_t i = 0; i < vertices.size() / 2; i += 4) {
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 8, &vertices[i * 2], GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }

    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}