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
    void toggleGrid() { m_showGrid = !m_showGrid; }

private:
    int m_width, m_height;

    Grid3D* m_grid;
    bool m_showGrid = true;

    void initFullscreenQuad();
    void initShaders();

    GLuint m_quadVAO, m_quadVBO;
    GLuint m_shaderProgram;
    GLuint m_computeShader;

    GLuint m_renderTex;
    GLuint m_cameraUBO;

    GLuint m_blackHoleUBO;
    GLuint m_diskUBO;

    GLuint m_planetUBO;

    GLuint m_timeUBO;

    GLuint m_bloomExtractTex = 0, m_bloomBlurTex[2] = { 0, 0 };
    GLuint m_bloomExtractFBO = 0, m_bloomBlurFBO[2] = { 0, 0 };
    GLuint m_bloomExtractShader = 0, m_bloomBlurShader = 0;

	float bhRadiusSim;

    void initUBO();
    void initBlackHoleUBO();
    void initRenderTexture();
    void initBloomTextures();
};

struct BlackHoleUBO {
    glm::vec3 bhPosition;
    float bhRadius;
};

struct PlanetBlock {
    glm::vec3 planetPosition;
    float planetRadius;
    glm::vec3 planetColor;
    float _pad;
};

struct DiskBlock {
    float diskInnerRadius;
    float diskOuterRadius;
    glm::vec3 diskColor;
    float _pad;
};