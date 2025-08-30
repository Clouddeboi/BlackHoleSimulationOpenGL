//Window and loop

#include "../headers/app.hpp"
#include "../headers/renderer.hpp"
#include <stdexcept>
#include <iostream>

//GLAD before GLFW
#include <glad/glad.h>
#include <GLFW/glfw3.h>

//----------------- Constructor -----------------
App::App(int width, int height, const std::string& title)
    : m_width(width), m_height(height), m_title(title), m_window(nullptr), m_renderer(nullptr)
{
    initGLFW();
    initGLAD();

    // create renderer once we have GL context
    m_renderer = new Renderer(m_width, m_height);
}

//----------------- Destructor -----------------
App::~App() {
    delete m_renderer;
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

//----------------- Init GLFW -----------------
void App::initGLFW() {
    if (!glfwInit()) {
        throw std::runtime_error("Failed to initialize GLFW!");
    }

    //Core OpenGL 4.5
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    m_window = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
    if (!m_window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window!");
    }

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);//Enable vsync
}

//----------------- Init GLAD -----------------
void App::initGLAD() {
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        throw std::runtime_error("Failed to initialize GLAD!");
    }
    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;
}

//----------------- Input -----------------
void App::processInput() {
    if (glfwGetKey(m_window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
        glfwSetWindowShouldClose(m_window, true);
    }
}

//----------------- Run -----------------
void App::run() {
    while (!glfwWindowShouldClose(m_window)) {
        processInput();

        //Render: clear screen
        glViewport(0, 0, m_width, m_height);
        glClearColor(0.1f, 0.0f, 0.2f, 1.0f);//Dark purple background
        glClear(GL_COLOR_BUFFER_BIT);

        m_renderer->render();

        //Swap
        glfwSwapBuffers(m_window);
        glfwPollEvents();
    }
}
