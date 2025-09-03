//Rendering

#include "../headers/renderer.hpp"
#include "../headers/glHelpers.hpp"
#include <glad/glad.h>
#include <stdexcept>
#include <iostream>
#include <fstream>
#include <sstream>

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

    glGenBuffers(1, &m_diskUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_diskUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(DiskBlock), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 2, m_diskUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    //Planet UBO
    glGenBuffers(1, &m_planetUBO);
    glBindBuffer(GL_UNIFORM_BUFFER, m_planetUBO);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(PlanetBlock), nullptr, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 3, m_planetUBO);//binding = 3
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

    //Schwarzschild radius calculation for a real black hole
    //Physical constants:
    constexpr double G = 6.67430e-11;//Gravitational constant (m^3 kg^-1 s^-2)
    constexpr double c = 2.99792458e8;//Speed of light in vacuum (m/s)
    constexpr double solarMass = 1.98847e30;//Mass of the sun (kg)

	//Black hole mass (in kg) (10 solar masses)
    double mass = 5.0 * solarMass;

    //Schwarzschild radius formula:
    //r_s = 2 * G * M / c^2
    //- r_s: Schwarzschild radius (meters)
    //- G: gravitational constant
    //- M: black hole mass (kg)
    //- c: speed of light (m/s)
    double rs_meters = 2.0 * G * mass / (c * c);

    //Simulation scale factor to convert meters to simulation units
    double scale = 0.0001016;

    //convert to simulation units
    bhRadiusSim = static_cast<float>(rs_meters * scale);

    //Setup grid
    m_grid = new Grid3D(-50.0f, 50.0f, 1.0f, bhRadiusSim);
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
void Renderer::render(const Camera& camera) {
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

    PlanetBlock planetBlock;
    planetBlock.planetPosition = glm::vec3(0.0f, 0.0f, -80.0f);
    planetBlock.planetRadius = 2.0f;
    planetBlock.planetColor = glm::vec3(0.2f, 0.5f, 1.0f);
    planetBlock._pad = 0.0f;

    glBindBuffer(GL_UNIFORM_BUFFER, m_planetUBO);
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(PlanetBlock), &planetBlock);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);

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
    glUniform1f(glGetUniformLocation(m_bloomExtractShader, "uThreshold"), 0.7f);
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
    glUniform1f(glGetUniformLocation(m_shaderProgram, "uBloomStrength"), 1.0f);
    glBindVertexArray(m_quadVAO);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);

    //Draw the 3D grid only if draw grid is true
    if (m_showGrid) {
        m_grid->draw(camera.getViewMatrix(), camera.getProjectionMatrix());
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