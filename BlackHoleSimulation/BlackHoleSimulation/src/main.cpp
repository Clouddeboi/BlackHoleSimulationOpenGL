#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <glm/glm.hpp>
#include <cmath>
#include <vector>

using namespace glm;

//Variable for tracking the time passed
float timeElapsed = 0.0f;

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

//Globals for ray rendering
unsigned int rayVAO, rayVBO;
int rayMaxPoints = 2000;//Max amount of vertices the ray can have

//Uniform locations cached
int uColorLoc = -1;
int uOffsetLoc = -1;

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

//Setting up the circle mesh
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
    std::string vertexCode = loadShaderSource("shaders/blackHole/shader.vert");
    std::string fragmentCode = loadShaderSource("shaders/blackHole/shader.frag");

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

    uColorLoc = glGetUniformLocation(shaderProgram, "uColor");
    uOffsetLoc = glGetUniformLocation(shaderProgram, "uOffset");

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

    // Cache uniform locations here for efficiency
    glUseProgram(shaderProgram);
    uColorLoc = glGetUniformLocation(shaderProgram, "uColor");
    uOffsetLoc = glGetUniformLocation(shaderProgram, "uOffset");
}

//Renders the circle each frame
void drawCircle(const BlackHole& bh, float aspect) {
    //Use shader program
    glUseProgram(shaderProgram);

    //Black hole offest in the render space
    vec2 bhRenderOffest = vec2(bh.position.x / aspect, bh.position.y);
    if (uOffsetLoc != -1) {
        glUniform2f(uOffsetLoc, bhRenderOffest.x, bhRenderOffest.y);
    }

    //Bind the VAO with our circle data
    glBindVertexArray(VAO);
    //Draw circle as a triangluar fan
    glDrawArrays(GL_TRIANGLE_FAN, 0, NUM_SEGMENTS + 2);
    //unBind the VAO for safety
    glBindVertexArray(0);
}

//Setup VAO/VBO for drawing the ray dynamically
void setupRayBuffers() {
    glGenVertexArrays(1, &rayVAO);
    glBindVertexArray(rayVAO);

    glGenBuffers(1, &rayVBO);
    glBindBuffer(GL_ARRAY_BUFFER, rayVBO);

    //Preallocating enough space for rayMaxPoints vec2 vertices
    glBufferData(GL_ARRAY_BUFFER, rayMaxPoints * sizeof(vec2), nullptr, GL_DYNAMIC_DRAW);

    //Vertex layout matches shader location 0
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(vec2), (void*)0);
    glEnableVertexAttribArray(0);

    //Unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

/// <summary>
/// We are using the CPU to calculate each of the rays movement
/// This is a simplified version, it is not physically perfect (we are not using the GPU yet)
/// We are using a Newtonian approach, using newtons gravity formula's and not Einstein's curved space time equations
/// The blackhole pos (bh.position) is expected in NDC(normalized device coords) where both x and y range from -1 to 1
/// startpos is the rays starting position in the render space
/// dir is the direction the ray starts moving
/// dt is the step size
/// </summary>
std::vector<vec2> computeRayPath(const BlackHole& bh, vec2 startPos, vec2 dir, float aspect, int maxSteps, float dt) {
    std::vector<vec2> path;
    path.reserve(maxSteps);

    //Convert the black holes position to the same render space used by the vertices
    vec2 bhRenderPos = vec2(bh.position.x / aspect, bh.position.y);

    vec2 pos = startPos;
    vec2 velocity = normalize(dir);

    for (int i = 0; i < maxSteps; i++) {
        path.push_back(pos);

        //vector from ray point to the BH center
        vec2 d = pos - bhRenderPos;
        float r = length(d);

        //If we cross the event horizon, we stop (ray is captured)
        if (r < bh.r_s) {
            break;
        }

        //Gravitational acceleration:
        //a = -strength * mass * d/r^3
        float eps = 1e-6f;
        float denom = r * r * r + eps;
        vec2 acc = -bh.strength * bh.mass * d / denom;

        //Eulers integrate velocity and position
        velocity += acc * dt;
        //velocity = normalize(velocity);
        pos += velocity * dt;

        //Stop the loop if we exit a reasonable bounding area
        //this is to avoid wasting steps
        if (pos.x > 1.2f / aspect || pos.x < -1.2f / aspect || pos.y > 1.2f || pos.y < -1.2f) {
            break;
        }
    }
    return path;
}

//Upload the computed path into the dynamic ray VBO
void updateRayVBO(const std::vector<vec2>& path) {
    glBindBuffer(GL_ARRAY_BUFFER, rayVBO);

    //Only upload the used prefix this buffer was preallocated
    GLsizeiptr size = (GLsizeiptr)path.size() * sizeof(vec2);
    if (size > 0) {
        glBufferSubData(GL_ARRAY_BUFFER, 0, size, path.data());
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

//Draw the ray
void drawRay(int vertexCount) {
    glLineWidth(2.0f);
    glBindVertexArray(rayVAO);
    glDrawArrays(GL_LINE_STRIP, 0, vertexCount);
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
    setupRayBuffers();

    //BlackHole instance with default values
    BlackHole blackHole = {vec2(0.0f, 0.0f), 2.0f, RADIUS, 5.0f };

    //Main loop that keeps running until we close the window
    while (!glfwWindowShouldClose(window)) {

        //Per frame increments
        timeElapsed += 0.01f;

        //get the current framebuffer size (for window resizing)
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        float aspect = (float)width / (float)height;

        //Set the background clear colour to white
        glClearColor(1.f, 1.f, 1.f, 1.f);

        //Clears the screen with the current clear color
        glClear(GL_COLOR_BUFFER_BIT);

        //Use the shader program
        glUseProgram(shaderProgram);

        //Draw the circle representing the black hole
        drawCircle(blackHole, aspect);

        //Draw a ray
        if (uOffsetLoc != -1) glUniform2f(uOffsetLoc, 0.0f, 0.0f);

        //initial ray paramaters
        vec2 rayStart = vec2(-0.9f / aspect, 0.5f + 0.5f * sin(timeElapsed));
        vec2 rayDir = vec2(1.0f, 0.0f);

        //Integration param
        float dt = 0.01f;
        int maxSteps = 2000;

        //Compute the path on CPU
        std::vector<vec2> rayPath = computeRayPath(blackHole, rayStart, rayDir, aspect, maxSteps, dt);

        //Upload path vertices to the VBO and draw the line
        if (!rayPath.empty()) {
            updateRayVBO(rayPath);
            //Set the ray color
            if (uColorLoc != -1) glUniform4f(uColorLoc, 0.0f, 0.2f, 1.0f, 1.0f);
            //Draw the ray as a connected strip of lines
            drawRay((int)rayPath.size());
        }

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
