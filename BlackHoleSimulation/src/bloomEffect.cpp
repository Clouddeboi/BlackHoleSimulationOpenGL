#include "../headers/bloomEffect.hpp"
#include "../headers/shaderManager.hpp"
#include "../headers/textureManager.hpp"
#include "../headers/glHelpers.hpp"
#include <stdexcept>

BloomEffect::BloomEffect(int width, int height, ShaderManager& shaderMgr, TextureManager& texMgr)
    : m_width(width), m_height(height), m_shaderMgr(shaderMgr), m_texMgr(texMgr),
    m_bloomExtractFBO(0), m_bloomExtractTex(0), m_quadVAO(0), m_quadVBO(0) {
    m_bloomBlurFBO[0] = m_bloomBlurFBO[1] = 0;
    m_bloomBlurTex[0] = m_bloomBlurTex[1] = 0;
}

BloomEffect::~BloomEffect() {
    if (m_bloomExtractFBO) glDeleteFramebuffers(1, &m_bloomExtractFBO);
    if (m_bloomBlurFBO[0]) glDeleteFramebuffers(2, m_bloomBlurFBO);
    if (m_quadVAO) glDeleteVertexArrays(1, &m_quadVAO);
    if (m_quadVBO) glDeleteBuffers(1, &m_quadVBO);
}

void BloomEffect::init() {
    //Load bloom shaders
    if (!m_shaderMgr.hasShader("bloom_extract")) {
        m_shaderMgr.loadShaderProgram("bloom_extract", "shaders/bloom/extract.vert", "shaders/bloom/extract.frag");
    }
    if (!m_shaderMgr.hasShader("bloom_blur")) {
        m_shaderMgr.loadShaderProgram("bloom_blur", "shaders/bloom/blur.vert", "shaders/bloom/blur.frag");
    }

    //Create textures
    m_bloomExtractTex = m_texMgr.createRenderTexture("bloom_extract", m_width, m_height, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    m_bloomBlurTex[0] = m_texMgr.createRenderTexture("bloom_blur_0", m_width, m_height, GL_RGBA16F, GL_RGBA, GL_FLOAT);
    m_bloomBlurTex[1] = m_texMgr.createRenderTexture("bloom_blur_1", m_width, m_height, GL_RGBA16F, GL_RGBA, GL_FLOAT);

    initFramebuffers();
    initQuad();
}

void BloomEffect::initFramebuffers() {
    //Extract FBO
    glGenFramebuffers(1, &m_bloomExtractFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, m_bloomExtractFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_bloomExtractTex, 0);
    GLHelpers::checkFramebufferComplete("Bloom Extract FBO");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    //Blur FBOs
    glGenFramebuffers(2, m_bloomBlurFBO);
    for (int i = 0; i < 2; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_bloomBlurFBO[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_bloomBlurTex[i], 0);
        GLHelpers::checkFramebufferComplete("Bloom Blur FBO " + std::to_string(i));
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void BloomEffect::initQuad() {
    float quadVertices[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f
    };

    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);
    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glBindVertexArray(0);
}

void BloomEffect::apply(GLuint inputTex) {
    extractBrightPixels(inputTex);
    blurTexture();
}

void BloomEffect::extractBrightPixels(GLuint inputTex) {
    glBindFramebuffer(GL_FRAMEBUFFER, m_bloomExtractFBO);
    glClear(GL_COLOR_BUFFER_BIT);

    m_shaderMgr.useShader("bloom_extract");
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, inputTex);
    glUniform1i(m_shaderMgr.getUniformLocation("bloom_extract", "uInputTex"), 0);

    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void BloomEffect::blurTexture() {
    m_shaderMgr.useShader("bloom_blur");
    glBindVertexArray(m_quadVAO);

    const int blurPasses = 5;
    bool horizontal = true;

    for (int i = 0; i < blurPasses * 2; ++i) {
        glBindFramebuffer(GL_FRAMEBUFFER, m_bloomBlurFBO[horizontal ? 0 : 1]);
        glClear(GL_COLOR_BUFFER_BIT);

        glUniform1i(m_shaderMgr.getUniformLocation("bloom_blur", "uHorizontal"), horizontal);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, i == 0 ? m_bloomExtractTex : m_bloomBlurTex[horizontal ? 1 : 0]);
        glUniform1i(m_shaderMgr.getUniformLocation("bloom_blur", "uInputTex"), 0);

        glDrawArrays(GL_TRIANGLES, 0, 6);
        horizontal = !horizontal;
    }

    glBindVertexArray(0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}