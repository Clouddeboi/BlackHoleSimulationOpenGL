//Rendering

#define STB_IMAGE_IMPLEMENTATION
#include "../headers/renderer.hpp"
#include "../headers/glHelpers.hpp"
#include <glad/glad.h>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <sstream>
#include <GLFW/glfw3.h>
#include <stb_image.h>
#include "stb_easy_font.h"
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

//Utility function to load shaders
static std::string loadFile(const std::string& path) {
    std::ifstream file(path);
    if (!file.is_open()) throw std::runtime_error("Failed to open file: " + path);
    std::stringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

static unsigned int compileShader(unsigned int type, const std::string& src) {
    unsigned int shader = glCreateShader(type);
    const char* csrc = src.c_str();
    glShaderSource(shader, 1, &csrc, nullptr);
    glCompileShader(shader);

    int success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[512];
        glGetShaderInfoLog(shader, 512, nullptr, info);
        throw std::runtime_error("Shader compile error: " + std::string(info));
    }
    return shader;
}

//Utility to load a texture from file
static GLuint loadTexture(const std::string& path) {
    int width, height, channels;
    unsigned char* data = stbi_load(path.c_str(), &width, &height, &channels, 4);
    if (!data) throw std::runtime_error("Failed to load texture: " + path);

    GLuint tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);
    return tex;
}

//----------------- Constructor -----------------
Renderer::Renderer(int width, int height)
    : m_width(width), m_height(height), m_quadVAO(0), m_quadVBO(0), m_shaderProgram(0)
{
    initFullscreenQuad();
    initShaders();

    //init compute shader
    m_computeShader = GLHelpers::loadComputeShader("shaders/geodesic.comp");

    //init render texture
    initRenderTexture();
    initBloomTextures();

    initUBO();
    initBlackHoleUBO();

    glGenBuffers(1, &m_planetSSBO);

    glGenBuffers(1, &m_diskUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_diskUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(DiskBlock), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 2, m_diskUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    for (size_t i = 0; i < m_planets.size(); ++i) {
        const Planet& planet = m_planets[i];

        //Set up UBO for this planet
        PlanetBlock planetBlock;
        planetBlock.planetPosition = planet.position;
        planetBlock.planetRadius = planet.radius;
        planetBlock.planetColor = planet.color;
        planetBlock._pad = 0.0f;

        glBindBuffer(GL_UNIFORM_BUFFER, m_planetUBO);
        glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PlanetBlock), &planetBlock);
        glBindBuffer(GL_UNIFORM_BUFFER, 0);

        //Bind planet texture to a texture unit (e.g., 7 + i)
        glActiveTexture(GL_TEXTURE7 + static_cast<GLenum>(i));
        glBindTexture(GL_TEXTURE_2D, planet.texture);
        glUniform1i(glGetUniformLocation(m_computeShader, "uPlanetTex"), 7 + static_cast<GLint>(i));

        //Dispatch compute or draw call for this planet
        //(You may need to update your compute shader to handle multiple planets and textures)
    }

    //Time UBO (for animation)
    glGenBuffers(1, &m_timeUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_timeUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(float), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 4, m_timeUBO);//binding = 4
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    int texWidth, texHeight, texChannels;
    unsigned char* data = stbi_load("textures/smoke/smoke_01.png", &texWidth, &texHeight, &texChannels, 4);//force RGBA
    if (!data) {
        throw std::runtime_error("Failed to load smoke texture!");
    }
    glGenTextures(1, &m_smokeTex);
    glBindTexture(GL_TEXTURE_2D, m_smokeTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texWidth, texHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);
    stbi_image_free(data);

    //Cubemap face order: +X, -X, +Y, -Y, +Z, -Z
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

    int nrChannels;
    for (GLuint i = 0; i < faces.size(); i++) {
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 3);
        if (data) {
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
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

    //Schwarzschild radius calculation for a real black hole
    //Physical constants:
    constexpr double G = 6.67430e-11;//Gravitational constant (m^3 kg^-1 s^-2)
    constexpr double c = 2.99792458e8;//Speed of light in vacuum (m/s)
    constexpr double solarMass = 1.98847e30;//Mass of the sun (kg)

	//Black hole mass (in kg) (10 solar masses)
    m_bhMass = 5.0 * solarMass;

    //Schwarzschild radius formula:
    //r_s = 2 * G * M / c^2
    //- r_s: Schwarzschild radius (meters)
    //- G: gravitational constant
    //- M: black hole mass (kg)
    //- c: speed of light (m/s)
    double rs_meters = 2.0 * G * m_bhMass / (c * c);

    //Simulation scale factor to convert meters to simulation units
    scale = 0.0001016;

    //convert to simulation units
    bhRadiusSim = static_cast<float>(rs_meters * scale);

    //Calculate ISCO (innermost stable circular orbit) for this black hole
    double isco_radius_m = 3.0 * rs_meters;//meters
    double isco_radius_sim = isco_radius_m * scale;//simulation units

    double earth_radius_m = 1.496e11; //1 AU in meters
    double v_earth = sqrt(G * m_bhMass / earth_radius_m);
    double omega_earth = v_earth / earth_radius_m;

    Planet earth;
    earth.orbitRadius = earth_radius_m * scale;//simulation units
    earth.orbitSpeed = omega_earth;//radians/sec (real time)
    earth.orbitPhase = 0.0;
    earth.orbitInclination = 0.0;
    earth.radius = 6378.0f * scale;
    earth.color = glm::vec3(1.0f);
    earth.texturePath = "textures/planets/earthTexture.jpg";
    earth.texture = loadTexture(earth.texturePath);
    m_planets.push_back(earth);

    //Debug: print orbital period and orbits per sim minute
    double T = 2.0 * M_PI / omega_earth;
    double timeScale = 31557600.0 / 60.0;
    double orbits_per_sim_minute = (timeScale * 60.0) / T;
    std::cout << "Earth orbital period (s): " << T << std::endl;
    std::cout << "Orbits per sim minute: " << orbits_per_sim_minute << std::endl;

    Planet mars;
    mars.position = glm::vec3(-15.0f, 0.0f, -90.0f);
    mars.radius = 3389.5f * scale;
    mars.color = glm::vec3(1.0f, 0.5f, 0.3f);
    mars.texturePath = "textures/planets/marsTexture.jpg";
    mars.texture = loadTexture(mars.texturePath);
    m_planets.push_back(mars);

    //Setup grid
    m_grid = new Grid3D(-50.0f, 50.0f, 1.0f, bhRadiusSim);
}

const std::vector<Planet>& Renderer::getPlanets() const {
    return m_planets;
}

//----------------- Destructor -----------------
Renderer::~Renderer() {
    glDeleteProgram(m_shaderProgram);
    glDeleteVertexArrays(1, &m_quadVAO);
    glDeleteBuffers(1, &m_quadVBO);
    glDeleteBuffers(1, &m_blackHoleUBO);
    delete m_grid;
}

void Renderer::initUBO() {
    glGenBuffers(1, &m_cameraUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_cameraUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(CameraUBO), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_cameraUBO);//binding=0
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
}

void Renderer::initBlackHoleUBO() {
    glGenBuffers(1, &m_blackHoleUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_blackHoleUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(BlackHoleUBO), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 1, m_blackHoleUBO); //binding=1
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
    std::string vertSrc = loadFile("shaders/blit.vert");
    std::string fragSrc = loadFile("shaders/blit.frag");

    unsigned int vert = compileShader(GL_VERTEX_SHADER, vertSrc);
    unsigned int frag = compileShader(GL_FRAGMENT_SHADER, fragSrc);

    m_shaderProgram = glCreateProgram();
    glAttachShader(m_shaderProgram, vert);
    glAttachShader(m_shaderProgram, frag);
    glLinkProgram(m_shaderProgram);

    int success;
    glGetProgramiv(m_shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        char info[512];
        glGetProgramInfoLog(m_shaderProgram, 512, nullptr, info);
        throw std::runtime_error("Shader linking error: " + std::string(info));
    }

    //Load and compile debug text shaders
    std::string textVertSrc = loadFile("shaders/debugtext/text.vert");
    std::string textFragSrc = loadFile("shaders/debugtext/text.frag");
    GLuint textVert = compileShader(GL_VERTEX_SHADER, textVertSrc);
    GLuint textFrag = compileShader(GL_FRAGMENT_SHADER, textFragSrc);
    m_debugTextShader = glCreateProgram();
    glAttachShader(m_debugTextShader, textVert);
    glAttachShader(m_debugTextShader, textFrag);
    glLinkProgram(m_debugTextShader);
    glDeleteShader(textVert);
    glDeleteShader(textFrag);

    //Create VAO/VBO for text
    glGenVertexArrays(1, &m_debugTextVAO);
    glGenBuffers(1, &m_debugTextVBO);
    glBindVertexArray(m_debugTextVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_debugTextVBO);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 16, (void*)0);
    glEnableVertexAttribArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    //Bloom extract shader
    std::string extractFrag = loadFile("shaders/bloomExtract.frag");
    m_bloomExtractShader = glCreateProgram();
    {
        unsigned int vert2 = compileShader(GL_VERTEX_SHADER, vertSrc);
        unsigned int frag2 = compileShader(GL_FRAGMENT_SHADER, extractFrag);
        glAttachShader(m_bloomExtractShader, vert2);
        glAttachShader(m_bloomExtractShader, frag2);
        glLinkProgram(m_bloomExtractShader);
        glDeleteShader(vert2);
        glDeleteShader(frag2);
    }

    //Bloom blur shader
    std::string blurFrag = loadFile("shaders/bloomBlur.frag");
    m_bloomBlurShader = glCreateProgram();
    {
        unsigned int vert2 = compileShader(GL_VERTEX_SHADER, vertSrc);
        unsigned int frag2 = compileShader(GL_FRAGMENT_SHADER, blurFrag);
        glAttachShader(m_bloomBlurShader, vert2);
        glAttachShader(m_bloomBlurShader, frag2);
        glLinkProgram(m_bloomBlurShader);
        glDeleteShader(vert2);
        glDeleteShader(frag2);
    }

    glDeleteShader(vert);
    glDeleteShader(frag);
}

//----------------- Render -----------------
void Renderer::render(const Camera& camera, float fps) {
    float time = static_cast<float>(glfwGetTime());
    glBindBuffer(GL_UNIFORM_BUFFER, m_timeUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(float), &time);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    //Update Camera UBO
    CameraUBO data = camera.getUBO();
    glBindBuffer(GL_UNIFORM_BUFFER, m_cameraUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(CameraUBO), &data);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    DiskBlock diskBlock;
    diskBlock.diskInnerRadius = bhRadiusSim * 3.0f;
    diskBlock.diskOuterRadius = bhRadiusSim * 10.0f;
    diskBlock.diskColor = glm::vec3(1.0f, 0.7f, 0.2f);
    diskBlock._pad = 0.0f;

    glBindBuffer(GL_UNIFORM_BUFFER, m_diskUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(DiskBlock), &diskBlock);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    const double timeScale = 31557600.0 / 60.0;//1 year in 1 minute
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

    PlanetBlock planetBlock;
    planetBlock.planetPosition = glm::vec3(0.0f, 0.0f, -80.0f);
    planetBlock.planetRadius = 2.0f;
    planetBlock.planetColor = glm::vec3(0.2f, 0.5f, 1.0f);
    planetBlock._pad = 0.0f;

    glBindBuffer(GL_UNIFORM_BUFFER, m_planetUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PlanetBlock), &planetBlock);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    glActiveTexture(GL_TEXTURE5);//Use texture unit 5
    glBindTexture(GL_TEXTURE_2D, m_smokeTex);
    glUniform1i(glGetUniformLocation(m_computeShader, "uSmokeTex"), 5);

    glActiveTexture(GL_TEXTURE6); //Use texture unit 6
    glBindTexture(GL_TEXTURE_CUBE_MAP, m_skyboxTex);
    glUniform1i(glGetUniformLocation(m_computeShader, "uSkybox"), 6);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    std::vector<std::string> debugLines;
    glm::vec3 camPos = camera.getPosition();
    //debugLines.push_back("BLACK HOLE SIMULATION");
    debugLines.push_back("Camera: (" + std::to_string(camPos.x) + ", " + std::to_string(camPos.y) + ", " + std::to_string(camPos.z) + ")");
    debugLines.push_back("Black Hole Radius: " + std::to_string(bhRadiusSim));
    debugLines.push_back("Black Hole Mass: " + std::to_string(m_bhMass) + " kg");
    debugLines.push_back("FPS: " + std::to_string(fps));
    debugLines.push_back("Simulation Scale Factor:" + std::to_string(scale));

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
    glUseProgram(m_computeShader);
    glUniform1i(glGetUniformLocation(m_computeShader, "uNumPlanets"), static_cast<GLint>(m_planets.size()));

    //Bind planet textures to units 10, 11, ...
    for (size_t i = 0; i < m_planets.size(); ++i) {
        glActiveTexture(GL_TEXTURE10 + static_cast<GLenum>(i));
        glBindTexture(GL_TEXTURE_2D, m_planets[i].texture);
    }

    //--- Compute Shader Pass ---
    glUseProgram(m_computeShader);
    GLuint blockIndex = glGetUniformBlockIndex(m_computeShader, "CameraBlock");
    if (blockIndex != GL_INVALID_INDEX) {
        glUniformBlockBinding(m_computeShader, blockIndex, 0);
    }
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, m_cameraUBO);

    //std::cout << "bhRadiusSim: " << bhRadiusSim << std::endl;
    //std::cout << "diskInnerRadius: " << diskBlock.diskInnerRadius << std::endl;
    //std::cout << "diskOuterRadius: " << diskBlock.diskOuterRadius << std::endl;

    glBindBuffer(GL_UNIFORM_BUFFER, m_planetUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PlanetBlock), &planetBlock);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    //Update Black Hole UBO
    BlackHoleUBO bhData;
    bhData.bhPosition = glm::vec3(0.0f, 0.0f, 0.0f);
    bhData.bhRadius = bhRadiusSim;

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

    //--- Bloom Extract Pass ---
    glUseProgram(m_bloomExtractShader);
    glBindFramebuffer(GL_FRAMEBUFFER, m_bloomExtractFBO);
    glViewport(0, 0, m_width, m_height);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_renderTex);
    glUniform1i(glGetUniformLocation(m_bloomExtractShader, "uRenderTex"), 0);
    glUniform1f(glGetUniformLocation(m_bloomExtractShader, "uThreshold"), 0.1f);
    glBindVertexArray(m_quadVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    //--- Bloom Blur Passes (ping-pong) ---
    bool horizontal = true, first_iteration = true;
    int blurPasses = 8;
    for (int i = 0; i < blurPasses; ++i) {
        glUseProgram(m_bloomBlurShader);
        glBindFramebuffer(GL_FRAMEBUFFER, m_bloomBlurFBO[horizontal]);
        glViewport(0, 0, m_width, m_height);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, first_iteration ? m_bloomExtractTex : m_bloomBlurTex[!horizontal]);
        glUniform1i(glGetUniformLocation(m_bloomBlurShader, "uImage"), 0);
        glUniform2f(glGetUniformLocation(m_bloomBlurShader, "uDirection"), horizontal ? 1.0f : 0.0f, horizontal ? 0.0f : 1.0f);
        glBindVertexArray(m_quadVAO);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
        horizontal = !horizontal;
        if (first_iteration) first_iteration = false;
    }

    //--- Final Composite Pass ---
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, m_width, m_height);
    glUseProgram(m_shaderProgram);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_renderTex);
    glUniform1i(glGetUniformLocation(m_shaderProgram, "uRenderTex"), 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, m_bloomBlurTex[!horizontal]);
    glUniform1i(glGetUniformLocation(m_shaderProgram, "uBloomTex"), 1);
    glUniform1f(glGetUniformLocation(m_shaderProgram, "uBloomStrength"), 0.0f);
    glBindVertexArray(m_quadVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    //Draw the 3D grid only if draw grid is true
    if (m_showGrid) {
        m_grid->draw(camera.getViewMatrix(), camera.getProjectionMatrix());
    }

    if (m_showDebugText) {
        renderDebugText(debugLines);
    }
}

void Renderer::initRenderTexture() {
    glGenTextures(1, &m_renderTex);
    glBindTexture(GL_TEXTURE_2D, m_renderTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
}

void Renderer::initBloomTextures() {
    //Extract texture
    glGenTextures(1, &m_bloomExtractTex);
    glBindTexture(GL_TEXTURE_2D, m_bloomExtractTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    //Blur textures (ping-pong)
    glGenTextures(2, m_bloomBlurTex);
    for (int i = 0; i < 2; ++i) {
        glBindTexture(GL_TEXTURE_2D, m_bloomBlurTex[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, m_width, m_height, 0, GL_RGBA, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    }

    //FBOs
    glGenFramebuffers(1, &m_bloomExtractFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_bloomExtractFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_bloomExtractTex, 0);

    glGenFramebuffers(2, m_bloomBlurFBO);
    for (int i = 0; i < 2; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_bloomBlurFBO[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_bloomBlurTex[i], 0);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::renderDebugText(const std::vector<std::string>& lines) {
    float x = 10.0f, y = 30.0f;
    char buffer[99999];
    std::vector<float> vertices;

    for (const auto& line : lines) {
        int quads = stb_easy_font_print(x, y, (char*)line.c_str(), NULL, buffer, sizeof(buffer));
        float* buf = (float*)buffer;
        for (int i = 0; i < quads * 4; ++i) {
            vertices.push_back(buf[i * 4 + 0]);
            vertices.push_back(buf[i * 4 + 1]);
        }
        y += 20.0f;
    }

    if (vertices.empty()) return;

    glm::mat4 ortho = glm::ortho(0.0f, float(m_width), float(m_height), 0.0f);

    glUseProgram(m_debugTextShader);
    glUniformMatrix4fv(glGetUniformLocation(m_debugTextShader, "uOrtho"), 1, GL_FALSE, &ortho[0][0]);
    glUniform3f(glGetUniformLocation(m_debugTextShader, "uColor"), 1.0f, 1.0f, 0.0f);

    glBindVertexArray(m_debugTextVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_debugTextVBO);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    //Draw each quad as a triangle fan
    for (size_t i = 0; i < vertices.size() / 2; i += 4) {
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 8, &vertices[i * 2], GL_DYNAMIC_DRAW);
        glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    }

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
    glUseProgram(0);
}