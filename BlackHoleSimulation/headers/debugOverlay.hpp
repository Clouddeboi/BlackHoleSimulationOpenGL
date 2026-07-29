#pragma once
#include <string>
#include <vector>
#include <glad/glad.h>

class ShaderManager;

class DebugOverlay {
public:
    DebugOverlay(int width, int height, ShaderManager& shaderMgr);
    ~DebugOverlay();

    //Prevent copying
    DebugOverlay(const DebugOverlay&) = delete;
    DebugOverlay& operator=(const DebugOverlay&) = delete;

    //Initialize debug text rendering
    void init();

    //Render lines of debug text
    void renderText(const std::vector<std::string>& lines);

    //Visibility toggle
    void setVisible(bool visible) { m_visible = visible; }
    bool isVisible() const { return m_visible; }
    void toggle() { m_visible = !m_visible; }

private:
    int m_width, m_height;
    ShaderManager& m_shaderMgr;
    bool m_visible;

    GLuint m_textVAO, m_textVBO;

    void initBuffers();
};