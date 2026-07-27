#pragma once
#include <glad/glad.h>
#include <iostream>
#include <string>

//===== OpenGL Error Checking Macro =====

#ifdef _DEBUG
#define GL_CHECK(call) \
        do { \
            call; \
            GLenum err; \
            while ((err = glGetError()) != GL_NO_ERROR) { \
                std::cerr << "OpenGL Error: 0x" << std::hex << err \
                          << " at " << __FILE__ << ":" << std::dec << __LINE__ \
                          << std::endl; \
            } \
        } while (0)
#else
#define GL_CHECK(call) call
#endif

//===== OpenGL Debug Callback =====

namespace GLDebug {
    inline void APIENTRY debugCallback(GLenum source, GLenum type, GLuint id,
        GLenum severity, GLsizei length,
        const GLchar* message, const void* userParam) {
        //Filter out notifications
        if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;

        std::cerr << "OpenGL Debug Message [";

        //Source
        switch (source) {
        case GL_DEBUG_SOURCE_API: std::cerr << "API"; break;
        case GL_DEBUG_SOURCE_WINDOW_SYSTEM: std::cerr << "Window"; break;
        case GL_DEBUG_SOURCE_SHADER_COMPILER: std::cerr << "Shader"; break;
        case GL_DEBUG_SOURCE_THIRD_PARTY: std::cerr << "3rdParty"; break;
        case GL_DEBUG_SOURCE_APPLICATION: std::cerr << "App"; break;
        case GL_DEBUG_SOURCE_OTHER: std::cerr << "Other"; break;
        }

        std::cerr << "][";

        //Type
        switch (type) {
        case GL_DEBUG_TYPE_ERROR: std::cerr << "ERROR"; break;
        case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: std::cerr << "Deprecated"; break;
        case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR: std::cerr << "UB"; break;
        case GL_DEBUG_TYPE_PORTABILITY: std::cerr << "Portability"; break;
        case GL_DEBUG_TYPE_PERFORMANCE: std::cerr << "Performance"; break;
        case GL_DEBUG_TYPE_MARKER: std::cerr << "Marker"; break;
        case GL_DEBUG_TYPE_OTHER: std::cerr << "Other"; break;
        }

        std::cerr << "][";

        //Severity
        switch (severity) {
        case GL_DEBUG_SEVERITY_HIGH: std::cerr << "HIGH"; break;
        case GL_DEBUG_SEVERITY_MEDIUM: std::cerr << "MEDIUM"; break;
        case GL_DEBUG_SEVERITY_LOW: std::cerr << "LOW"; break;
        }

        std::cerr << "]: " << message << std::endl;

        //Break on high severity errors in debug builds
#ifdef _DEBUG
        if (severity == GL_DEBUG_SEVERITY_HIGH) {
#ifdef _MSC_VER
            __debugbreak();
#else
            __builtin_trap();
#endif
        }
#endif
    }

    inline void enable() {
#ifdef _DEBUG
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(debugCallback, nullptr);
        glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
        std::cout << "OpenGL debug output enabled" << std::endl;
#endif
    }
}