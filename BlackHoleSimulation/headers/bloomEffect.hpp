#pragma once
#include <glad/glad.h>

class ShaderManager;
class TextureManager;

class BloomEffect {
public:
    BloomEffect(int width, int height, ShaderManager& shaderMgr, TextureManager& texMgr);
    ~BloomEffect();

    //Prevent copying
    BloomEffect(const BloomEffect&) = delete;
    BloomEffect& operator=(const BloomEffect&) = delete;

    //Initialize bloom resources
    void init();

    //Apply bloom effect to input texture
    void apply(GLuint inputTex);

    //Get the final blurred texture
    GLuint getBloomTexture() const { return m_bloomBlurTex[0]; }

private:
    int m_width, m_height;
    ShaderManager& m_shaderMgr;
    TextureManager& m_texMgr;

    GLuint m_bloomExtractFBO;
    GLuint m_bloomBlurFBO[2];
    GLuint m_bloomExtractTex;
    GLuint m_bloomBlurTex[2];

    GLuint m_quadVAO, m_quadVBO;

    void initFramebuffers();
    void initQuad();
    void extractBrightPixels(GLuint inputTex);
    void blurTexture();
};