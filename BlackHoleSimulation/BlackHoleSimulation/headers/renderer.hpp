#pragma once
#include <string>
#include "../headers/camera.hpp"
#include "../headers/grid.hpp"
#include <glad/glad.h>

//Forward declaration
class App;

class Renderer {
public:
    Renderer(int width, int height);
    ~Renderer();

    void render(const Camera& camera);//called every frame

private:
    int m_width, m_height;

    Grid3D* m_grid;

    void initFullscreenQuad();
    void initShaders();

    GLuint m_quadVAO, m_quadVBO;
    GLuint m_shaderProgram;
    GLuint m_computeShader;

    GLuint m_renderTex;
    GLuint m_cameraUBO;

    GLuint m_blackHoleUBO;

	float bhRadiusSim;

    void initUBO();
    void initBlackHoleUBO();
    void initRenderTexture();
};

struct BlackHoleUBO {
    glm::vec3 bhPosition;
    float bhRadius;
};