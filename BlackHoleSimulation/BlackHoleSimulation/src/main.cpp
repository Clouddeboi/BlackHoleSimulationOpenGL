#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <iostream>
#include <glm/glm.hpp>

using namespace glm;

struct BlackHole {
    vec2 position;
    float mass;//Mass (for gravity calculations)
    float r_s;//Radius of the event horizon
    float strength;//lensing strength
};

int main() {
    //Initialize GLFW
    if (!glfwInit()) {
        std::cerr << "Failed to initialize GLFW" << std::endl;
        return -1;
    }

    //Telling GLFW which version of OpenGl we will use
    //We will use 3.3
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);

    //Creating a window titled "Black Hole Simulation"
    //And 800x600 pixels by size
    GLFWwindow* window = glfwCreateWindow(800, 600, "Black Hole Simulation", nullptr, nullptr);
    if (!window) {
        std::cerr << "Failed to create GLFW window" << std::endl;
        glfwTerminate();
        return -1;
    }

    //Make this the current window in OpenGL
    glfwMakeContextCurrent(window);

    //Loading all OpenGl funtion pointers (using GLAD)
    //Must happen after making the current context
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    //BlackHole instance with default values
    BlackHole blackHole = {vec2(0.0f, 0.0f), 1.0f, 0.2f, 5.0f };

    //Main loop that keeps running until we close the window
    while (!glfwWindowShouldClose(window)) {
        //Clears the screen with the current clear color
        glClear(GL_COLOR_BUFFER_BIT);

        //Swap the front and back buffers
        //(Double Buffering)
        glfwSwapBuffers(window);

        //Process any input or events
        glfwPollEvents();
    }

    //Cleanup
    glfwDestroyWindow(window);
    glfwTerminate();

    //End of program
    return 0;
}
