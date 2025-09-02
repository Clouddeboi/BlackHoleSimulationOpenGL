//Window and loop

#include "../headers/app.hpp"
#include "../headers/renderer.hpp"
#include "../headers/camera.hpp"
#include <stdexcept>
#include <iostream>

//GLAD before GLFW
#include <glad/glad.h>
#include <GLFW/glfw3.h>

//----------------- Constructor -----------------
App::App(int width, int height, const std::string& title)
    : m_width(width), m_height(height), m_title(title), m_window(nullptr),
    m_renderer(nullptr), m_camera(nullptr), m_lastFrame(0.0f)
{
    initGLFW();
    initGLAD();

    m_camera = new Camera(60.0f, (float)m_width / m_height, 0.1f, 100.0f);
    m_renderer = new Renderer(m_width, m_height);

    //Hook mouse callback
    glfwSetWindowUserPointer(m_window, m_camera);
    glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double xpos, double ypos) {
        Camera* cam = reinterpret_cast<Camera*>(glfwGetWindowUserPointer(window));
        if (cam) cam->processMouse((float)xpos, (float)ypos);
        });
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

//----------------- Destructor -----------------
App::~App() {
    delete m_renderer;
    delete m_camera;
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

static void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
    Camera* cam = reinterpret_cast<Camera*>(glfwGetWindowUserPointer(window));
    if (cam) cam->processMouse((float)xpos, (float)ypos);
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

    glfwSetWindowUserPointer(m_window, m_camera);
    glfwSetCursorPosCallback(m_window, mouse_callback);

    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

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
    static bool gridKeyPressed = false;
    if (glfwGetKey(m_window, GLFW_KEY_G) == GLFW_PRESS) {
        if (!gridKeyPressed) {
            m_renderer->toggleGrid();
            gridKeyPressed = true;
        }
    }
    else {
        gridKeyPressed = false;
    }
}

//----------------- Run -----------------
void App::run() {
    while (!glfwWindowShouldClose(m_window)) {
        processInput();
        float currentFrame = glfwGetTime();
        float deltaTime = currentFrame - m_lastFrame;
        m_lastFrame = currentFrame;

        m_camera->update(deltaTime);

        //Render: clear screen
        glViewport(0, 0, m_width, m_height);
        glClearColor(0.1f, 0.0f, 0.2f, 1.0f);//Dark purple background
        glClear(GL_COLOR_BUFFER_BIT);

        m_renderer->render(*m_camera);

        //Swap
        glfwSwapBuffers(m_window);
        glfwPollEvents();
    }
}
