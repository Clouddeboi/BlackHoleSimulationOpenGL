#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <glm/glm.hpp>

using namespace glm;

struct BlackHole {
    vec2 position;
    float mass;//Mass (for gravity calculations)
    float r_s;//Radius of the event horizon
    float strength;//lensing strength
};

//Globals for circle rendering

//Vertex Array object and vertex buffer object
unsigned int VAO, VBO;

//Shader program handle
unsigned int shaderProgram;

//Number of triangles needed to approx. the circle and the radius
//The circle is what our black hole will be represented as
const int NUM_SEGMENTS = 50;
const float RADIUS = 0.2f;

//Loading shader source code from a text file
std::string loadShaderSource(const char* filepath) {
    //open the file stream
    std::ifstream file(filepath);
    //Check if the file is open successfully
    if (!file.is_open()) {
        std::cerr << "Failed to open shader file: " << filepath << std::endl;
        return "";//return empty string on failure
    }
    std::stringstream ss;
    //Reading the entire file into the ss
    ss << file.rdbuf();
    //convert to a string and return
    return ss.str();
}

//Compling an individual shader (vert or frag)
unsigned int compileShader(const char* source, GLenum type) {
    //Creating a shader obj
    unsigned int shader = glCreateShader(type);
    //Attach code to the shader
    glShaderSource(shader, 1, &source, nullptr);
    //Compile
    glCompileShader(shader);

    int success;
    char infoLog[512];
    //Check compile status
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);

    //Error msg
    if (!success) {
        glGetShaderInfoLog(shader, 512, nullptr, infoLog);
        std::cerr << (type == GL_VERTEX_SHADER ? "Vertex" : "Fragment")
            << " shader compilation failed:\n" << infoLog << std::endl;
    }
    return shader;//return compiled shader ID
}

void setupCircle(GLFWwindow* window) {
    //Create an array to store circle vertex positions (x,y)
    //using a triangle fan
    //first vertex is the center, the rest is the circumference
    float circleVertices[2 * (NUM_SEGMENTS + 2)];

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    float aspectRatio = (float)width / (float)height;

    //Centers of x & y
    circleVertices[0] = 0.0f;
    circleVertices[1] = 0.0f;

    //Calculating the positions of the circle circ.
    for (int i = 0; i <= NUM_SEGMENTS; i++) {
        //Angle in radians
        float angle = 2.0f * 3.1415926f * float(i) / float(NUM_SEGMENTS);
        
        //x = r * cos(theta)
        circleVertices[2 * (i + 1)] = RADIUS * cos(angle) / aspectRatio;

        //y = r * sin(theta)
        circleVertices[2 * (i + 1) + 1] = RADIUS * sin(angle);
    }

    //Load vert and frag shader source code from the files
    std::string vertexCode = loadShaderSource("shaders/shader.vert");
    std::string fragmentCode = loadShaderSource("shaders/shader.frag");

    if (vertexCode.empty() || fragmentCode.empty()) {
        std::cerr << "Shader source empty, aborting setup." << std::endl;
        return;
    }

    //Convert to C-string
    const char* vertexSource = vertexCode.c_str();
    const char* fragmentSource = fragmentCode.c_str();

    //Compiles the shaders
    unsigned int vertexShader = compileShader(vertexSource, GL_VERTEX_SHADER);
    unsigned int fragmentShader = compileShader(fragmentSource, GL_FRAGMENT_SHADER);

    //Create the shader program and link both shaders
    shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    //Checks if the linking was a success
    int success;
    char infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "Shader program linking failed:\n" << infoLog << std::endl;
    }

    //After linking the shaders no longer are needed individually
    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    //Generate and bind vertex Array obj (VAO)
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    //Generate and bind Vertex Buffer obj (VBO)
    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    //Upload vertex data to the GPU
    glBufferData(GL_ARRAY_BUFFER, sizeof(circleVertices), circleVertices, GL_STATIC_DRAW);

    //Here it tells OpenGl how to interpret the vertex data
    //location = 0 in shader, 2 floats per vertex (x,y), no normalization
    //tightly packed, starting at offset 0
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    //Unbind VBO and VAO to avoid accidental changes
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

//Renders the circle each frame
void drawCircle() {
    //Use shader program
    glUseProgram(shaderProgram);
    //Bind the VAO with our circle data
    glBindVertexArray(VAO);
    //Draw circle as a triangluar fan
    glDrawArrays(GL_TRIANGLE_FAN, 0, NUM_SEGMENTS + 2);
    //unBind the VAO for safety
    glBindVertexArray(0);
}

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

    //Prep the circle shape and shaders before we start rendering
    setupCircle(window);

    //BlackHole instance with default values
    //BlackHole blackHole = {vec2(0.0f, 0.0f), 1.0f, 0.2f, 5.0f };

    //Main loop that keeps running until we close the window
    while (!glfwWindowShouldClose(window)) {
        //Set the background clear colour to white
        glClearColor(1.f, 1.f, 1.f, 1.f);

        //Clears the screen with the current clear color
        glClear(GL_COLOR_BUFFER_BIT);

        //Draw the circle representing the black hole
        drawCircle();

        //Swap the front and back buffers
        //(Double Buffering)
        glfwSwapBuffers(window);

        //Process any input or events
        glfwPollEvents();
    }

    //Clean up GPU resources
    glDeleteVertexArrays(1, &VAO);
    glDeleteBuffers(1, &VBO);
    glDeleteProgram(shaderProgram);

    //Cleanup (window and GLFW)
    glfwDestroyWindow(window);
    glfwTerminate();

    //End of program
    return 0;
}
