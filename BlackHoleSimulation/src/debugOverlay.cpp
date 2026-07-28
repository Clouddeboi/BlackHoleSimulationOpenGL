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
        m_shaderMgr.loadShaderProgram("debug_text", "shaders/debugText/vert.glsl", "shaders/debugText/frag.glsl");
    }

    initBuffers();
}

void DebugOverlay::initBuffers() {
    glGenVertexArrays(1, &m_textVAO);
    glGenBuffers(1, &m_textVBO);
    glBindVertexArray(m_textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_textVBO);
    glBufferData(GL_ARRAY_BUFFER, 99999 * 4, nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glBindVertexArray(0);
}

void DebugOverlay::renderText(const std::vector<std::string>& lines) {
    if (!m_visible || lines.empty()) return;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    m_shaderMgr.useShader("debug_text");

    glm::mat4 projection = glm::ortho(0.0f, static_cast<float>(m_width),
        0.0f, static_cast<float>(m_height));
    glUniformMatrix4fv(m_shaderMgr.getUniformLocation("debug_text", "uProjection"),
        1, GL_FALSE, glm::value_ptr(projection));

    glBindVertexArray(m_textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_textVBO);

    float yPos = static_cast<float>(m_height) - 20.0f;
    const float lineHeight = 15.0f;

    for (const auto& line : lines) {
        static char buffer[99999];
        std::vector<char> lineBuffer(line.begin(), line.end());
        lineBuffer.push_back('\0');
        int numQuads = stb_easy_font_print(10.0f, yPos, lineBuffer.data(), nullptr, buffer, sizeof(buffer));

        glBufferSubData(GL_ARRAY_BUFFER, 0, numQuads * 4 * 4 * sizeof(float), buffer);
        glDrawArrays(GL_QUADS, 0, numQuads * 4);

        yPos -= lineHeight;
    }

    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
}