/*
    App class implementation
    Manages window, input, main loop
*/

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
	initGLFW();//Create window and context
	initGLAD();//Load OpenGL functions

	//Create camera and renderer
	//Camera (fov, aspect, near, far)
    m_camera = new Camera(60.0f, (float)m_width / m_height, 0.1f, 10000.0f);
    m_renderer = new Renderer(m_width, m_height);

    //Hook mouse callback
	//Setting user pointer to camera for access in callback
    glfwSetWindowUserPointer(m_window, m_camera);

	//set the mouse callback to update camera direction
    glfwSetCursorPosCallback(m_window, [](GLFWwindow* window, double xpos, double ypos) {
        Camera* cam = reinterpret_cast<Camera*>(glfwGetWindowUserPointer(window));
        if (cam) cam->processMouse((float)xpos, (float)ypos);
        });
	//Hide and capture cursor
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
}

//----------------- Destructor -----------------
App::~App() {
	//Clean up resources
    delete m_renderer;
    delete m_camera;
    glfwDestroyWindow(m_window);
    glfwTerminate();
}

//----------------- Mouse Callback -----------------
//Not used directly but available for reference
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

	//Create window and context 
    m_window = glfwCreateWindow(m_width, m_height, m_title.c_str(), nullptr, nullptr);
    if (!m_window) {
        glfwTerminate();
        throw std::runtime_error("Failed to create GLFW window!");
    }

	//Set user pointer to camera for access in mouse callback
    glfwSetWindowUserPointer(m_window, m_camera);
    glfwSetCursorPosCallback(m_window, mouse_callback);

	//Hide and capture cursor
    glfwSetInputMode(m_window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(1);//Enable vsync
}

//----------------- Init GLAD -----------------
void App::initGLAD() {
	//Load OpenGL functions using GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        throw std::runtime_error("Failed to initialize GLAD!");
    }
    std::cout << "OpenGL version: " << glGetString(GL_VERSION) << std::endl;
}

//----------------- Input -----------------
void App::processInput() {
	//Close on Escape
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

	//Toggle debug text with H
    static bool debugKeyPressed = false;
    if (glfwGetKey(m_window, GLFW_KEY_H) == GLFW_PRESS) {
        if (!debugKeyPressed) {
            m_renderer->toggleDebugText();
            debugKeyPressed = true;
        }
    }
    else {
        debugKeyPressed = false;
    }
}

//----------------- Run -----------------
void App::run() {
	//Main loop
    while (!glfwWindowShouldClose(m_window)) {
		//handle keyboard input
        processInput();

		//Calculate delta time and FPS
        float currentFrame = glfwGetTime();
        float deltaTime = currentFrame - m_lastFrame;
        m_lastFrame = currentFrame;
        float fps = 1.0f / deltaTime;

		//Update camera
        m_camera->update(deltaTime);

        //Render: clear screen
        glViewport(0, 0, m_width, m_height);
        glClearColor(0.1f, 0.0f, 0.2f, 1.0f);//Dark purple background
        glClear(GL_COLOR_BUFFER_BIT);

        m_renderer->render(*m_camera, fps);

        //Swap
        glfwSwapBuffers(m_window);
        glfwPollEvents();
    }
}
