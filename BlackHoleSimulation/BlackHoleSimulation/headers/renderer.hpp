#pragma once
#include <string>
#include "../headers/camera.hpp"
#include <glad/glad.h>

//Forward declaration
class App;

class Renderer {
public:
    Renderer(int width, int height);
    ~Renderer();

    void render(const Camera& camera);//called every frame

private:
    void initFullscreenQuad();
    void initShaders();

    GLuint m_quadVAO, m_quadVBO;
    GLuint m_shaderProgram;

    GLuint m_cameraUBO;

    void initUBO();
};
