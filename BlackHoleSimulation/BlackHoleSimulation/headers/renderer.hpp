#pragma once
#include <string>

//Forward declaration
class App;

class Renderer {
public:
    Renderer(int width, int height);
    ~Renderer();

    void render();  // called every frame

private:
    void initFullscreenQuad();
    void initShaders();

    unsigned int m_quadVAO, m_quadVBO;
    unsigned int m_shaderProgram;
};
