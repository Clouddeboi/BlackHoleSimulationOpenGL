#pragma once
#include <string>
#include <memory>
#include "../headers/camera.hpp"
#include <glad/glad.h>
#include <vector>

// Forward declarations
class App;
class Grid3D;
class ShaderManager;
class TextureManager;
class DebugOverlay;

struct PlanetBlock {
    glm::vec3 planetPosition;
    float planetRadius;
    glm::vec3 planetColor;
    float _pad;
};

struct Planet {
    glm::vec3 position;
    float radius;
    glm::vec3 color;
    GLuint texture;
    std::string texturePath;

    //Orbital parameters (currently not in use)
    double orbitRadius = 0.0;
    double realOrbitRadius = 0.0;
    double orbitSpeed = 0.0;
    double orbitPhase = 0.0;
    double orbitInclination = 0.0;
};

class Renderer {
public:
    Renderer(int width, int height);
    ~Renderer();

    void render(const Camera& camera, float fps);
    void toggleGrid() { m_showGrid = !m_showGrid; }
    void toggleDebugText();

    const std::vector<Planet>& getPlanets() const;

    bool isGridVisible() const { return m_showGrid; }
    bool isDebugTextVisible() const;

private:
    int m_width, m_height;

    //Subsystem managers
    std::unique_ptr<ShaderManager> m_shaderMgr;
    std::unique_ptr<TextureManager> m_textureMgr;
    std::unique_ptr<DebugOverlay> m_debugOverlay;
    std::unique_ptr<Grid3D> m_grid;

    bool m_showGrid = false;

    void initFullscreenQuad();
    void initShaders();
    void initUBO();
    void initBlackHoleUBO();
    void initRenderTexture();

    GLuint m_quadVAO, m_quadVBO;
    GLuint m_computeShader;
    GLuint m_renderTex;
    GLuint m_skyboxTex = 0;

    GLuint m_cameraUBO;
    GLuint m_blackHoleUBO;
    GLuint m_diskUBO;
    GLuint m_planetUBO;
    GLuint m_planetSSBO = 0;
    std::vector<Planet> m_planets;
    GLuint m_timeUBO;

    float m_blackHoleRadiusSim;
    double m_blackHoleMass;
    double m_simulationScale;
};

struct BlackHoleUBO {
    glm::vec3 bhPosition;
    float bhRadius;
};

struct DiskBlock {
    float diskInnerRadius;
    float diskOuterRadius;
    glm::vec3 diskColor;
    float _pad;
};