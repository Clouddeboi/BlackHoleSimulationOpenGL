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

//struct GeodesicState {
//    float r;//radial coord
//    float phi;//angular coord
//    float pr;//radial momentum
//    float L;//angular momentum (constant)
//};

struct Particle {
    vec2 pos;
    vec2 vel;
    std::vector<vec2> trail;//Recent positions for the visible trail
    bool alive = true;
};

int numRays = 100;
int maxTrailLen = 50;//max amount of points
std::vector<Particle> particles;

//Globals for circle rendering
//Vertex Array object and vertex buffer object
unsigned int VAO, VBO;

//Shader program handle
unsigned int shaderProgram;
unsigned int rayShaderProgram;

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

void setupRayShader() {
    std::string vertexCode = loadShaderSource("shaders/ray/shader.vert");
    std::string fragmentCode = loadShaderSource("shaders/ray/shader.frag");

    if (vertexCode.empty() || fragmentCode.empty()) {
        std::cerr << "Ray shader source empty, aborting setup." << std::endl;
        return;
    }

    const char* vertexSource = vertexCode.c_str();
    const char* fragmentSource = fragmentCode.c_str();

    unsigned int vertexShader = compileShader(vertexSource, GL_VERTEX_SHADER);
    unsigned int fragmentShader = compileShader(fragmentSource, GL_FRAGMENT_SHADER);

    rayShaderProgram = glCreateProgram();
    glAttachShader(rayShaderProgram, vertexShader);
    glAttachShader(rayShaderProgram, fragmentShader);
    glLinkProgram(rayShaderProgram);

    int success;
    char infoLog[512];
    glGetProgramiv(rayShaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(rayShaderProgram, 512, nullptr, infoLog);
        std::cerr << "Ray shader program linking failed:\n" << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
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

    //Cache uniform locations here for efficiency
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

//Method to compute acceleration given the position
vec2 acceleration(const BlackHole& bh, vec2 pos, float aspect) {
    vec2 bhRenderPos = vec2(bh.position.x / aspect, bh.position.y);
    vec2 d = pos - bhRenderPos;
    float r = length(d);
    if (r < bh.r_s) return vec2(0.0f);//if inside the event horizon there is no more acceleration needed

    float eps = 1e-6f;
    float denom = r * r * r + eps;
    return -bh.strength * bh.mass * d / denom;
}

//Compute ray path by integrating position and velocity (cartesian RK4)
std::vector<vec2> computeRayPathNewtonian(const BlackHole& bh, vec2 startPos, vec2 startVel, int maxSteps, float dt, float aspect) {
    std::vector<vec2> path;
    path.reserve(maxSteps);

    vec2 pos = startPos;
    vec2 vel = startVel;

    for (int i = 0; i < maxSteps; ++i) {
        //stop if inside event horizon (bh.r_s is in render space units)
        vec2 bhRenderPos = vec2(bh.position.x / aspect, bh.position.y);
        float r = length(pos - bhRenderPos);
        if (r <= bh.r_s) break;

        //record the vertex
        path.emplace_back(pos);

        //RK4 integration for pos & vel using acceleration() helper
        auto acc = [&](const vec2& p) {
            return acceleration(bh, p, aspect);
            };

        //k1
        vec2 k1_v = acc(pos) * dt;
        vec2 k1_p = vel * dt;

        //k2
        vec2 k2_v = acc(pos + 0.5f * k1_p) * dt;
        vec2 k2_p = (vel + 0.5f * k1_v) * dt;

        //k3
        vec2 k3_v = acc(pos + 0.5f * k2_p) * dt;
        vec2 k3_p = (vel + 0.5f * k2_v) * dt;

        //k4
        vec2 k4_v = acc(pos + k3_p) * dt;
        vec2 k4_p = (vel + k3_v) * dt;

        //update vel and pos
        vel += (k1_v + 2.0f * k2_v + 2.0f * k3_v + k4_v) / 6.0f;
        pos += (k1_p + 2.0f * k2_p + 2.0f * k3_p + k4_p) / 6.0f;

        //break if pos goes off screen
        if (fabs(pos.x) > 5.0f || fabs(pos.y) > 5.0f) break;
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
    glUseProgram(rayShaderProgram);
    glLineWidth(2.0f);
    glBindVertexArray(rayVAO);
    glDrawArrays(GL_LINE_STRIP, 0, vertexCount);
    glBindVertexArray(0);
}

//single RK4 step in cartesian coords (updates pos & vel)
void integrateParticleRK4(const BlackHole& bh, Particle& p, float dt, float aspect) {
    auto acc = [&](const vec2& x)->vec2 { return acceleration(bh, x, aspect); };

    //k1
    vec2 k1_v = acc(p.pos);
    vec2 k1_p = p.vel;

    //k2
    vec2 p_k2 = p.pos + 0.5f * dt * k1_p;
    vec2 v_k2 = p.vel + 0.5f * dt * k1_v;
    vec2 k2_v = acc(p_k2);
    vec2 k2_p = v_k2;

    //k3
    vec2 p_k3 = p.pos + 0.5f * dt * k2_p;
    vec2 v_k3 = p.vel + 0.5f * dt * k2_v;
    vec2 k3_v = acc(p_k3);
    vec2 k3_p = v_k3;

    //k4
    vec2 p_k4 = p.pos + dt * k3_p;
    vec2 v_k4 = p.vel + dt * k3_v;
    vec2 k4_v = acc(p_k4);
    vec2 k4_p = v_k4;

    //update
    p.pos += (dt / 6.0f) * (k1_p + 2.0f * k2_p + 2.0f * k3_p + k4_p);
    p.vel += (dt / 6.0f) * (k1_v + 2.0f * k2_v + 2.0f * k3_v + k4_v);
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

    setupRayShader();

    //Initialize particles once
    particles.resize(numRays);
    for (int i = 0; i < numRays; ++i) {
        float yStart = 0.5f - 0.1f + i * 0.08f;
        vec2 startPos = vec2(-1.0f / 1.0f, yStart);
        vec2 startDir = normalize(vec2(1.0f, 0.0f));
        float speed = 0.01f;
        particles[i].pos = startPos;
        particles[i].vel = startDir * speed;
        particles[i].trail.clear();
        particles[i].trail.reserve(maxTrailLen);
        particles[i].trail.push_back(particles[i].pos);
        particles[i].alive = true;
    }

    //BlackHole instance with default values
    BlackHole blackHole = { vec2(0.0f, 0.0f), 0.5f, RADIUS, 1050.0f };

    //Main loop that keeps running until we close the window
    while (!glfwWindowShouldClose(window)) {

        //Per frame increments
        timeElapsed += 0.01f;

        //get the current framebuffer size (for window resizing)
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        float aspect = (float)width / (float)height;

        //Set the background clear colour to white
        glClearColor(0.43137254901960786f, 0.3176470588235294f, 0.5058823529411764f, 1.f);

        //Clears the screen with the current clear color
        glClear(GL_COLOR_BUFFER_BIT);

        //Use the shader program
        glUseProgram(shaderProgram);

        //Draw the circle representing the black hole
        drawCircle(blackHole, aspect);

        ////Draw a ray
        if (uOffsetLoc != -1) glUniform2f(uOffsetLoc, 0.0f, 0.0f);;

        //Base vertical offest for ray changes with elapsed time
        float baseY = 0.5f + 0.5f * sin(timeElapsed);

        //Integration param
        float dt = 0.004f;

        glEnable(GL_PROGRAM_POINT_SIZE);

        for (int i = 0; i < numRays; ++i) {
            Particle& p = particles[i];

            //Random float helper
            auto randf = [](float min, float max) {
                return min + (max - min) * (rand() / (float)RAND_MAX);
                };

            vec2 bhRenderPos = vec2(blackHole.position.x / aspect, blackHole.position.y);

            if (!p.alive || length(p.pos - bhRenderPos) <= blackHole.r_s ||
                fabs(p.pos.x) > 5.0f || fabs(p.pos.y) > 5.0f)
            {
                //Choose one of four edges: 0=left,1=right,2=top,3=bottom
                int edge = rand() % 4;

                vec2 spawnPos;
                vec2 baseVel;

                switch (edge) {
                case 0:
                    spawnPos = vec2(-5.0f / aspect, randf(-5.0f, 5.0f));
                    baseVel = vec2(1.0f, 0.0f);//moving right
                    break;
                case 1: 
                    spawnPos = vec2(5.0f / aspect, randf(-5.0f, 5.0f));
                    baseVel = vec2(-1.0f, 0.0f);//moving left
                    break;
                case 2:
                    spawnPos = vec2(randf(-5.0f / aspect, 5.0f / aspect), 5.0f);
                    baseVel = vec2(0.0f, -1.0f);//moving down
                    break;
                case 3:
                    spawnPos = vec2(randf(-5.0f / aspect, 5.0f / aspect), -5.0f);
                    baseVel = vec2(0.0f, 1.0f);//moving up
                    break;
                }

                //Add small random angle offset for velocity
                float angleOffset = randf(-0.3f, 0.3f);
                float ca = cos(angleOffset), sa = sin(angleOffset);
                vec2 vel = vec2(baseVel.x * ca - baseVel.y * sa, baseVel.x * sa + baseVel.y * ca);

                p.pos = spawnPos;
                p.vel = normalize(vel) * randf(1.5f, 3.0f);
                p.trail.clear();
                p.trail.push_back(p.pos);
                p.alive = true;
            }

            //advance particle one integration step
            integrateParticleRK4(blackHole, p, dt, aspect);

            //push into trail
            p.trail.push_back(p.pos);
            if ((int)p.trail.size() > maxTrailLen) p.trail.erase(p.trail.begin()); //pop front

            //upload trail to VBO and draw
            if (!p.trail.empty()) {
                GLsizeiptr size = (GLsizeiptr)p.trail.size() * sizeof(vec2);
                glBindBuffer(GL_ARRAY_BUFFER, rayVBO);
                glBufferSubData(GL_ARRAY_BUFFER, 0, size, p.trail.data());
                glBindBuffer(GL_ARRAY_BUFFER, 0);

                //set ray color
                if (uColorLoc != -1) glUniform4f(uColorLoc, 0.0f, 0.0f, 0.0f, 1.0f);

                //draw trail
                drawRay((int)p.trail.size());

                //draw head as point
                glPointSize(6.0f);
                glBindVertexArray(rayVAO);
                glDrawArrays(GL_POINTS, 0, 1);
                glBindVertexArray(0);
            }
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