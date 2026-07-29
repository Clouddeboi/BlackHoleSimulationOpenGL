/*
    Core Rendering logic
    Sets up OpenGL, shaders, textures
    Handles simulation data
*/

#define STB_IMAGE_IMPLEMENTATION
#include "../headers/renderer.hpp"
#include "../headers/glHelpers.hpp"
#include "../headers/shaderManager.hpp"
#include "../headers/textureManager.hpp"
#include "../headers/debugOverlay.hpp"
#include "../headers/grid.hpp"
#include "../headers/constants.hpp"

#include <glad/glad.h>
#include <stdexcept>
#include <iostream>
#include <GLFW/glfw3.h>
#include <stb_image.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//----------------- Constructor -----------------
Renderer::Renderer(int width, int height)
    : m_width(width), m_height(height), m_quadVAO(0), m_quadVBO(0), m_computeShader(0),
    m_renderTex(0), m_cameraUBO(0), m_blackHoleUBO(0), m_diskUBO(0), m_planetUBO(0),
    m_timeUBO(0), m_blackHoleRadiusSim(0.0f), m_blackHoleMass(0.0), m_simulationScale(0.0)
{
    //Initialize subsystem managers
    m_shaderMgr = std::make_unique<ShaderManager>();
    m_textureMgr = std::make_unique<TextureManager>();
    m_debugOverlay = std::make_unique<DebugOverlay>(width, height, *m_shaderMgr);

    //Setup fullscreen quad and shaders
    initFullscreenQuad();
    initShaders();

    //Initialize Debug overlay
    m_debugOverlay->init();

    //Init compute shader
    m_computeShader = m_shaderMgr->loadComputeShader("geodesic", "shaders/geodesic.comp");

    //Init render texture
    initRenderTexture();

    //Initialize UBOs
    initUBO();
    initBlackHoleUBO();

    glGenBuffers(1, &m_planetSSBO);

    //Create planet UBO
    glGenBuffers(1, &m_planetUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_planetUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(PlanetBlock), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 3, m_planetUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glGenBuffers(1, &m_diskUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_diskUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(DiskBlock), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 2, m_diskUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    //Time UBO (for animation)
    glGenBuffers(1, &m_timeUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_timeUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 4, m_timeUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    //Load smoke texture (for accretion disk)
    m_textureMgr->loadTexture("smoke", "textures/smoke/smoke_01.png");

    //Load skybox cubemap textures
    std::vector<std::string> faces = {
        "textures/skybox/right.png",
        "textures/skybox/left.png",
        "textures/skybox/top.png",
        "textures/skybox/bottom.png",
        "textures/skybox/front.png",
        "textures/skybox/back.png"
    };

    glGenTextures(1, &m_skyboxTex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_skyboxTex);

    int texWidth, texHeight, nrChannels;
    for (GLuint i = 0; i < faces.size(); i++) {
        unsigned char* data = stbi_load(faces[i].c_str(), &texWidth, &texHeight, &nrChannels, 3);
        if (data) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, texWidth, texHeight, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);
        }
        else {
            throw std::runtime_error("Cubemap texture failed to load at path: " + faces[i]);
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_CUBE_MAP, 0);

    //Schwarzschild radius calculation
    using namespace BlackHoleConstants;

    m_blackHoleMass = kBlackHoleMassSolarMasses * kSolarMass;
    double rs_meters = 2.0 * kGravitationalConstant * m_blackHoleMass / (kSpeedOfLight * kSpeedOfLight);
    m_simulationScale = kSimulationScale;
    m_blackHoleRadiusSim = static_cast<float>(rs_meters * m_simulationScale);

    //Create planets
    Planet earth;
    earth.position = glm::vec3(0.0f, 0.0f, -90.0f);
    earth.radius = 6378.0f * m_simulationScale;
    earth.color = glm::vec3(1.0f);
    earth.texturePath = "textures/planets/earthTexture.jpg";
    earth.texture = m_textureMgr->loadTexture("earth", earth.texturePath);
    m_planets.push_back(earth);

    Planet mars;
    mars.position = glm::vec3(-15.0f, 0.0f, -90.0f);
    mars.radius = 3389.5f * m_simulationScale;
    mars.color = glm::vec3(1.0f, 0.5f, 0.3f);
    mars.texturePath = "textures/planets/marsTexture.jpg";
    mars.texture = m_textureMgr->loadTexture("mars", mars.texturePath);
    m_planets.push_back(mars);

    //Bind compute shader and setup planet uniforms
    m_shaderMgr->useShader("geodesic");

    //Planet textures are bound in render() loop, not here
    m_shaderMgr->unbindShader();

    m_shaderMgr->unbindShader();

    //Setup grid
    m_grid = std::make_unique<Grid3D>(kGridMin, kGridMax, kGridSpacing, m_blackHoleRadiusSim);
}

//----------------- Get Planets -----------------
const std::vector<Planet>& Renderer::getPlanets() const {
    return m_planets;
}

//----------------- Destructor -----------------
Renderer::~Renderer() {
    //Smart pointers clean themselves up automatically

    //VAOs and VBOs
    if (m_quadVAO) glDeleteVertexArrays(1, &m_quadVAO);
    if (m_quadVBO) glDeleteBuffers(1, &m_quadVBO);

    //UBOs and SSBOs
    if (m_cameraUBO) glDeleteBuffers(1, &m_cameraUBO);
    if (m_blackHoleUBO) glDeleteBuffers(1, &m_blackHoleUBO);
    if (m_diskUBO) glDeleteBuffers(1, &m_diskUBO);
    if (m_planetUBO) glDeleteBuffers(1, &m_planetUBO);
    if (m_planetSSBO) glDeleteBuffers(1, &m_planetSSBO);
    if (m_timeUBO) glDeleteBuffers(1, &m_timeUBO);

    //Render texture
    if (m_renderTex) glDeleteTextures(1, &m_renderTex);

    //Skybox
    if (m_skyboxTex) glDeleteTextures(1, &m_skyboxTex);
}

//----------------- UBOs -----------------
void Renderer::initUBO() {
    glGenBuffers(1, &m_cameraUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_cameraUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(CameraUBO), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_cameraUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

//----------------- Black Hole UBO -----------------
void Renderer::initBlackHoleUBO() {
    glGenBuffers(1, &m_blackHoleUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_blackHoleUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(BlackHoleUBO), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_blackHoleUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

//----------------- Fullscreen Quad -----------------
void Renderer::initFullscreenQuad() {
    float quadVertices[] = {
        //positions   //texcoords
        -1.0f, -1.0f, 0.0f, 0.0f,
         1.0f, -1.0f, 1.0f, 0.0f,
         1.0f,  1.0f, 1.0f, 1.0f,
        -1.0f,  1.0f, 0.0f, 1.0f
    };
    unsigned int indices[] = { 0, 1, 2, 2, 3, 0 };

    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    unsigned int EBO;
    glGenBuffers(1, &EBO);

    glBindVertexArray(m_quadVAO);

    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

//----------------- Shaders -----------------
void Renderer::initShaders() {
    //Main blit shader (for final composition)
    m_shaderMgr->loadShaderProgram("blit", "shaders/blit.vert", "shaders/blit.frag");
}

//----------------- Render Texture -----------------
void Renderer::initRenderTexture() {
    glGenTextures(1, &m_renderTex);
    glBindTexture(GL_TEXTURE_2D, m_renderTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
}

//----------------- Render -----------------
void Renderer::render(const Camera& camera, float fps) {
    //Update time UBO
    float time = static_cast<float>(glfwGetTime());
    glBindBuffer(GL_UNIFORM_BUFFER, m_timeUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(float), &time);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    //Update Camera UBO
    CameraUBO data = camera.getUBO();
    glBindBuffer(GL_UNIFORM_BUFFER, m_cameraUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(CameraUBO), &data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    //Set up accretion disk parameters
    DiskBlock diskBlock;
    diskBlock.diskInnerRadius = m_blackHoleRadiusSim * BlackHoleConstants::kDiskInnerRadiusMultiplier;
    diskBlock.diskOuterRadius = m_blackHoleRadiusSim * BlackHoleConstants::kDiskOuterRadiusMultiplier;
    diskBlock.diskColor = glm::vec3(1.0f, 0.7f, 0.2f);
    diskBlock._pad = 0.0f;

    glBindBuffer(GL_UNIFORM_BUFFER, m_diskUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(DiskBlock), &diskBlock);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    //Update planet positions
    const double timeScale = BlackHoleConstants::kTimeScaleYearToMinute;
    double simTime = double(time) * timeScale;
    for (auto& p : m_planets) {
        if (p.orbitRadius > 0.0 && p.orbitSpeed > 0.0) {
            double angle = p.orbitPhase + p.orbitSpeed * simTime;
            double x = p.orbitRadius * cos(angle);
            double z = p.orbitRadius * sin(angle);
            double y = 0.0;
            if (p.orbitInclination != 0.0) {
                y = z * sin(p.orbitInclination);
                z = z * cos(p.orbitInclination);
            }
            p.position = glm::vec3(float(x), float(y), float(z));
        }
    }

    //Update Planet UBO
    PlanetBlock planetBlock;
    planetBlock.planetPosition = glm::vec3(0.0f, 0.0f, -80.0f);
    planetBlock.planetRadius = 2.0f;
    planetBlock.planetColor = glm::vec3(0.2f, 0.5f, 1.0f);
    planetBlock._pad = 0.0f;

    glBindBuffer(GL_UNIFORM_BUFFER, m_planetUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PlanetBlock), &planetBlock);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    //Bind smoke and skybox textures
    m_shaderMgr->useShader("geodesic");

    glActiveTexture(GL_TEXTURE5);
    m_textureMgr->bindTexture("smoke");
    glUniform1i(m_shaderMgr->getUniformLocation("geodesic", "uSmokeTex"), 5);

    glActiveTexture(GL_TEXTURE6);
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_skyboxTex);
    glUniform1i(m_shaderMgr->getUniformLocation("geodesic", "uSkybox"), 6);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //Prepare debug text
    std::vector<std::string> debugLines;
    glm::vec3 camPos = camera.getPosition();
    std::string tab = "    ";

    debugLines.push_back("Camera Info");
    debugLines.push_back(tab + "Camera Position: (" + std::to_string(camPos.x) + ", " + std::to_string(camPos.y) + ", " + std::to_string(camPos.z) + ")");
    debugLines.push_back(tab + "FPS: " + std::to_string(fps));
    debugLines.push_back("\n");

    debugLines.push_back("BlackHole Info");
    debugLines.push_back(tab + "Black Hole Radius: " + std::to_string(m_blackHoleRadiusSim));
    debugLines.push_back(tab + "Black Hole Mass: " + std::to_string(m_blackHoleMass) + " kg");
    debugLines.push_back("\n");

    debugLines.push_back("Simulation Info");
    debugLines.push_back(tab + "Simulation Scale Factor: " + std::to_string(m_simulationScale));
    debugLines.push_back("\n");

    debugLines.push_back("Planet Info");
    if (!m_planets.empty()) {
        const glm::vec3& earthPos = m_planets[0].position;
        debugLines.push_back(tab + "Earth Position: (" + std::to_string(earthPos.x) + ", " + std::to_string(earthPos.y) + ", " + std::to_string(earthPos.z) + ")");

        if (m_planets.size() > 1) {
            const glm::vec3& marsPos = m_planets[1].position;
            debugLines.push_back(tab + "Mars Position: (" + std::to_string(marsPos.x) + ", " + std::to_string(marsPos.y) + ", " + std::to_string(marsPos.z) + ")");
        }
    }

    //Prepare planet data for SSBO
    struct PlanetDataGPU {
        glm::vec3 position;
        float radius;
        glm::vec3 color;
        float _pad;
    };
    std::vector<PlanetDataGPU> planetData;
    for (const auto& p : m_planets) {
        PlanetDataGPU pd;
        pd.position = p.position;
        pd.radius = p.radius;
        pd.color = p.color;
        pd._pad = 0.0f;
        planetData.push_back(pd);
    }
    glBindBuffer(GL_SHADER_STORAGE_BUFFER, m_planetSSBO);
    glBufferData(GL_SHADER_STORAGE_BUFFER, planetData.size() * sizeof(PlanetDataGPU), planetData.data(), GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 7, m_planetSSBO);

    //Set uNumPlanets uniform
    m_shaderMgr->useShader("geodesic");
    glUniform1i(m_shaderMgr->getUniformLocation("geodesic", "uNumPlanets"), static_cast<GLint>(m_planets.size()));

    //Bind planet textures
    for (size_t i = 0; i < m_planets.size(); ++i) {
        glActiveTexture(GL_TEXTURE10 + static_cast<GLenum>(i));
        glBindTexture(GL_TEXTURE_2D, m_planets[i].texture);
    }

    //--- Compute Shader Pass ---
    m_shaderMgr->useShader("geodesic");
    GLuint blockIndex = glGetUniformBlockIndex(m_computeShader, "CameraBlock");
    if (blockIndex != GL_INVALID_INDEX) {
        glUniformBlockBinding(m_computeShader, blockIndex, 0);
    }
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_cameraUBO);

    glBindBuffer(GL_UNIFORM_BUFFER, m_planetUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PlanetBlock), &planetBlock);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    //Update Black Hole UBO
    BlackHoleUBO bhData;
    bhData.bhPosition = glm::vec3(0.0f, 0.0f, 0.0f);
    bhData.bhRadius = m_blackHoleRadiusSim;

    glBindBuffer(GL_UNIFORM_BUFFER, m_blackHoleUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(BlackHoleUBO), &bhData);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    GLuint bhBlockIndex = glGetUniformBlockIndex(m_computeShader, "BlackHoleBlock");
    if (bhBlockIndex != GL_INVALID_INDEX) {
        glUniformBlockBinding(m_computeShader, bhBlockIndex, 1);
    }
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_blackHoleUBO);

    GLuint planetBlockIndex = glGetUniformBlockIndex(m_computeShader, "PlanetBlock");
    if (planetBlockIndex != GL_INVALID_INDEX) {
        glUniformBlockBinding(m_computeShader, planetBlockIndex, 3);
    }
    glBindBufferBase(GL_UNIFORM_BUFFER, 3, m_planetUBO);

    glBindImageTexture(0, m_renderTex, 0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA32F);

    GLuint groupsX = (m_width + 7) / 8;
    GLuint groupsY = (m_height + 7) / 8;
    glDispatchCompute(groupsX, groupsY, 1);

    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    //--- Final Composite Pass ---
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_width, m_height);
    m_shaderMgr->useShader("blit");

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_renderTex);
    glUniform1i(m_shaderMgr->getUniformLocation("blit", "uRenderTex"), 0);

    glBindVertexArray(m_quadVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    //Draw grid
    if (m_showGrid) {
        m_grid->draw(camera.getViewMatrix(), camera.getProjectionMatrix());
    }

    //Render debug text
    m_debugOverlay->renderText(debugLines);
}

//----------------- Debug Text Toggle -----------------
void Renderer::toggleDebugText() {
    m_debugOverlay->toggle();
}

bool Renderer::isDebugTextVisible() const {
    return m_debugOverlay->isVisible();
}