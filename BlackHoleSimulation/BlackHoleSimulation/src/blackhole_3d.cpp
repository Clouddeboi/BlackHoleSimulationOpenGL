#include <glad/gl.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <glm/gtc/matrix_transform.hpp>

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <glm/glm.hpp>
#include <cmath>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

using namespace glm;

//Variable for tracking the time passed
float timeElapsed = 0.0f;

struct BlackHole {
    vec3 position;
    float mass;//Mass (for gravity calculations)
    float r_s;//Radius of the event horizon
    float strength;//lensing strength
};

//We set the r_s as 0
//This is because we will use the mass to calculate the r_s
BlackHole blackHole = { vec3(0.0f, 0.0f, 0.0f), 200.5f, 0.0f, 10.0f };

struct Particle {
    vec3 pos;
    vec3 vel;
    std::vector<vec3> trail;//Recent positions for the visible trail
    bool alive = true;

    float r = 0.0f;
    float phi = 0.0f;
    float pr = 0.0f;
    float L = 0.0f;
    bool grInit = false;
};

int numRays = 1000;
int maxTrailLen = 300;//max amount of points
std::vector<Particle> particles;

//Globals for circle rendering
//Vertex Array object and vertex buffer object
unsigned int VAO, VBO, EBO;
int indexCount = 0;

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

//Orbit camera globals
float camRadius = 10.0f;//Distance from target
float camYaw = glm::radians(45.0f);
float camPitch = glm::radians(30.0f);
glm::vec3 camTarget = glm::vec3(0.0f, 0.0f, 0.0f);

//Grid/Axis globals
unsigned int gridVAO = 0, gridVBO = 0;
int gridVertexCount = 0;

unsigned int axesVAO = 0, axesVBO = 0;
//Grid shader
unsigned int gridShaderProgram = 0;
int grid_uMVPLoc = -1;
int grid_uColorLoc = -1;

//Skybox
unsigned int skyboxVAO = 0, skyboxVBO = 0;
unsigned int skyboxProgram = 0;
unsigned int skyboxTexture = 0;
int skybox_uMVPLoc = -1;

//Helper function for camera orbit
glm::mat4 getOrbitView() {
    //Convert spherical coords to Cartesian
    float x = camRadius * cos(camPitch) * cos(camYaw);
    float y = camRadius * sin(camPitch);
    float z = camRadius * cos(camPitch) * sin(camYaw);

    glm::vec3 camPos = camTarget + glm::vec3(x, y, z);
    return glm::lookAt(camPos, camTarget, glm::vec3(0.0f, 1.0f, 0.0f));
}

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

void setupGridShader() {
    std::string vertexCode = loadShaderSource("shaders/grid/shader.vert");
    std::string fragmentCode = loadShaderSource("shaders/grid/shader.frag");

    if (vertexCode.empty() || fragmentCode.empty()) {
        std::cerr << "Grid shader source empty, aborting setup." << std::endl;
        return;
    }

    const char* vertexSource = vertexCode.c_str();
    const char* fragmentSource = fragmentCode.c_str();

    unsigned int vert = compileShader(vertexSource, GL_VERTEX_SHADER);
    unsigned int frag = compileShader(fragmentSource, GL_FRAGMENT_SHADER);

    gridShaderProgram = glCreateProgram();
    glAttachShader(gridShaderProgram, vert);
    glAttachShader(gridShaderProgram, frag);
    glLinkProgram(gridShaderProgram);

    int success;
    char infoLog[512];
    glGetProgramiv(gridShaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(gridShaderProgram, 512, nullptr, infoLog);
        std::cerr << "Grid shader program linking failed:\n" << infoLog << std::endl;
    }

    glDeleteShader(vert);
    glDeleteShader(frag);

    //cache uniform locations
    grid_uMVPLoc = glGetUniformLocation(gridShaderProgram, "uMVP");
    grid_uColorLoc = glGetUniformLocation(gridShaderProgram, "uColor");
}

void setupGrid(int halfExtent = 25, float spacing = 1.0f, float y = -1.0f) {
    std::vector<float> verts;
    //Lines parallel to X (varying z)
    for (int i = -halfExtent; i <= halfExtent; ++i) {
        float z = i * spacing;
        verts.push_back(-halfExtent * spacing); verts.push_back(y); verts.push_back(z);
        verts.push_back(halfExtent * spacing); verts.push_back(y); verts.push_back(z);
    }
    //Lines parallel to Z (varying x)
    for (int i = -halfExtent; i <= halfExtent; ++i) {
        float x = i * spacing;
        verts.push_back(x); verts.push_back(y); verts.push_back(-halfExtent * spacing);
        verts.push_back(x); verts.push_back(y); verts.push_back(halfExtent * spacing);
    }

    //Upload to GPU
    glGenVertexArrays(1, &gridVAO);
    glBindVertexArray(gridVAO);

    glGenBuffers(1, &gridVBO);
    glBindBuffer(GL_ARRAY_BUFFER, gridVBO);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_STATIC_DRAW);

    //layout(location = 0) -> position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
    gridVertexCount = (int)verts.size() / 3;
}

void setupAxes(float halfExtent = 25.0f, float y = -1.0f) {
    glGenVertexArrays(1, &axesVAO);
    glBindVertexArray(axesVAO);

    glGenBuffers(1, &axesVBO);
    glBindBuffer(GL_ARRAY_BUFFER, axesVBO);
    //reserve a small buffer we will update per draw
    glBufferData(GL_ARRAY_BUFFER, 4 * 3 * sizeof(float), nullptr, GL_DYNAMIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glBindVertexArray(0);
}

void drawGrid(const glm::mat4& mvp) {
    if (gridShaderProgram == 0) return;
    glUseProgram(gridShaderProgram);
    glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uMVP"), 1, GL_FALSE, &mvp[0][0]);

    //grid in faint color
    if (grid_uMVPLoc != -1) glUniformMatrix4fv(grid_uMVPLoc, 1, GL_FALSE, &mvp[0][0]);
    if (grid_uColorLoc != -1) glUniform4f(grid_uColorLoc, 0.643f, 0.643f, 0.643f, 0.5f);

    glBindVertexArray(gridVAO);
    glLineWidth(1.0f);
    glDrawArrays(GL_LINES, 0, gridVertexCount);
    glBindVertexArray(0);
}

void drawAxes(float halfExtent = 25.0f, float y = -1.0f) {
    if (gridShaderProgram == 0) return;
    glUseProgram(gridShaderProgram);

    glBindVertexArray(axesVAO);
    glBindBuffer(GL_ARRAY_BUFFER, axesVBO);

    //X axis
    float xVerts[6] = {
        -halfExtent, y, 0.0f,
         halfExtent, y, 0.0f
    };
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(xVerts), xVerts);
    if (grid_uColorLoc != -1) glUniform4f(grid_uColorLoc, 0.643f, 0.643f, 0.643f, 0.5f);
    glLineWidth(2.0f);
    glDrawArrays(GL_LINES, 0, 2);

    //Z axis
    float zVerts[6] = {
        0.0f, y, -halfExtent,
        0.0f, y,  halfExtent
    };
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(zVerts), zVerts);
    if (grid_uColorLoc != -1) glUniform4f(grid_uColorLoc, 0.643f, 0.643f, 0.643f, 0.5f);
    glDrawArrays(GL_LINES, 0, 2);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

//Setting up the circle mesh
void setupCircle(const BlackHole& bh, GLFWwindow* window) {
    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    float aspectRatio = (float)width / (float)height;

    //Simple sphere aprroximation wiht lattitude(theta) and longtitude(phi)
    std::vector<float> sphereVertices;
    std::vector<unsigned int> indices;
    const int THETA_SEGMENTS = 20;//Vertical slices
    const int PHI_SEGMENTS = NUM_SEGMENTS;//Horizontal slices
    int vertexCount = (THETA_SEGMENTS + 1) * (PHI_SEGMENTS + 1);
    //sphereVertices.push_back(0.0f); sphereVertices.push_back(0.0f); sphereVertices.push_back(0.0f);

    //Schwarzschild radius calculation
    //r_s = 2 * G * M/c^2
    //1e8f scales everything up so it is visible on screen
    blackHole.r_s = 2.0f * 6.67430e-11f * blackHole.mass * 1e8f;

    //Calculating the positions of the circle circ.
    for (int theta = 0; theta <= THETA_SEGMENTS; ++theta) {

        float t = float(theta) / THETA_SEGMENTS;
        float thetaAngle = glm::pi<float>() * t;

        for (int phi = 0; phi <= PHI_SEGMENTS; ++phi) {
            float p = float(phi) / PHI_SEGMENTS;
            float phiAngle = glm::two_pi<float>() * p;

            float x = blackHole.r_s * sin(thetaAngle) * cos(phiAngle);
            float y = blackHole.r_s * sin(thetaAngle) * sin(phiAngle);
            float z = blackHole.r_s * cos(thetaAngle);

            sphereVertices.push_back(x);
            sphereVertices.push_back(y);
            sphereVertices.push_back(z);
        }
    }

    //Build strip indices per latitude band
    for (int theta = 0; theta < THETA_SEGMENTS; ++theta) {
        for (int phi = 0; phi <= PHI_SEGMENTS; ++phi) {
            int i0 = theta * (PHI_SEGMENTS + 1) + phi;
            int i1 = (theta + 1) * (PHI_SEGMENTS + 1) + phi;
            indices.push_back(i0);
            indices.push_back(i1);
        }
        //Add two degenerate indices to separate strips (except after last band)
        if (theta < THETA_SEGMENTS - 1) {
            indices.push_back((theta + 1) * (PHI_SEGMENTS + 1) + PHI_SEGMENTS);
            indices.push_back((theta + 1) * (PHI_SEGMENTS + 1));
        }
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

    int success;
    char infoLog[512];
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, nullptr, infoLog);
        std::cerr << "Shader program linking failed:\n" << infoLog << std::endl;
    }

    uColorLoc = glGetUniformLocation(shaderProgram, "uColor");
    uOffsetLoc = glGetUniformLocation(shaderProgram, "uOffset");
    //Gets the location for MVP
    int uMVPLoc = glGetUniformLocation(shaderProgram, "uMVP");

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
    glBufferData(GL_ARRAY_BUFFER, sphereVertices.size() * sizeof(float), sphereVertices.data(), GL_STATIC_DRAW);

    glGenBuffers(1, &EBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);
    indexCount = (int)indices.size();

    //Here it tells OpenGl how to interpret the vertex data
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    //Unbind VBO and VAO to avoid accidental changes
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    //Cache uniform locations here for efficiency
    glUseProgram(shaderProgram);
    glUniform3f(uOffsetLoc, bh.position.x, bh.position.y, bh.position.z);
    //uColorLoc = glGetUniformLocation(shaderProgram, "uColor");
    //uOffsetLoc = glGetUniformLocation(shaderProgram, "uOffset");

    glBindVertexArray(VAO);
    //glDrawElements(GL_TRIANGLE_STRIP, (GLsizei)indices.size(), GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
}

//Renders the circle each frame
void drawCircle(const BlackHole& bh, float aspect) {
    //Use shader program
    glUseProgram(shaderProgram);

    //Black hole offest in the render space
    vec3 bhRenderOffset = vec3(bh.position.x, bh.position.y, bh.position.z);
    if (uOffsetLoc != -1) {
        glUniform3f(uOffsetLoc, bhRenderOffset.x, bhRenderOffset.y, bhRenderOffset.z);
    }

    //Bind the VAO with our circle data
    glBindVertexArray(VAO);
    if (indexCount > 0) {
        glDrawElements(GL_TRIANGLE_STRIP, indexCount, GL_UNSIGNED_INT, 0);
    }

    //unBind the VAO for safety
    glBindVertexArray(0);
}

//Setup VAO/VBO for drawing the ray dynamically
void setupRayBuffers() {
    glGenVertexArrays(1, &rayVAO);
    glBindVertexArray(rayVAO);

    glGenBuffers(1, &rayVBO);
    glBindBuffer(GL_ARRAY_BUFFER, rayVBO);

    //Preallocating enough space for rayMaxPoints vec3 vertices
    glBufferData(GL_ARRAY_BUFFER, rayMaxPoints * sizeof(vec3), nullptr, GL_DYNAMIC_DRAW);

    //Vertex layout matches shader location 0
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(vec3), (void*)0);
    glEnableVertexAttribArray(0);

    //Unbind
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);
}

//Upload the computed path into the dynamic ray VBO
void updateRayVBO(const std::vector<vec3>& path) {
    glBindBuffer(GL_ARRAY_BUFFER, rayVBO);

    //Only upload the used prefix this buffer was preallocated
    GLsizeiptr size = (GLsizeiptr)path.size() * sizeof(vec3);
    if (size > 0) {
        glBufferSubData(GL_ARRAY_BUFFER, 0, size, path.data());
    }
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

unsigned int loadCubemap(const std::vector<std::string>& faces) {
    unsigned int tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_CUBE_MAP, tex);

    int width, height, nrChannels;
    //For each of the 6 faces we call stbi_load and upload with glTextImage2d
    for (unsigned int i = 0; i < faces.size(); ++i) {
        unsigned char* data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);
        if (data) {
            //Choose imput format based on the number of channels in the loaded image
            GLenum format = (nrChannels == 3) ? GL_RGB : GL_RGBA;

            //Mapping the faces vector to the cube faces in order
            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
            stbi_image_free(data);//Free image memory after upload
        }
        else {
            //If the loading fails print the failed file
            std::cerr << "Cubemap tex failed to load: " << faces[i] << std::endl;
            stbi_image_free(data);
        }
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    
    //CLAMP_TO_EDGE avoids seams at the cube edges
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return tex;
}

void setupSkybox(const std::vector<std::string>& faces) {
    //cube vertex data: each triangle of the cube is defined by these 36 positions
    //These are positions only (no normals/uv) — in the skybox vertex shader
    //we use the position as a direction vector into the cubemap sampler.
    float skyboxVertices[] = {
        //positions
        -1.0f,  1.0f, -1.0f,
        -1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f, -1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,

        -1.0f, -1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f, -1.0f,  1.0f,
        -1.0f, -1.0f,  1.0f,

        -1.0f,  1.0f, -1.0f,
         1.0f,  1.0f, -1.0f,
         1.0f,  1.0f,  1.0f,
         1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f,  1.0f,
        -1.0f,  1.0f, -1.0f,

        -1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f, -1.0f,
         1.0f, -1.0f, -1.0f,
        -1.0f, -1.0f,  1.0f,
         1.0f, -1.0f,  1.0f
    };

    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);
    glBindVertexArray(skyboxVAO);
    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), &skyboxVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    //load cubemap
    skyboxTexture = loadCubemap(faces);

    //compile skybox shader
    std::string vsrc = loadShaderSource("shaders/skybox/skybox.vert");
    std::string fsrc = loadShaderSource("shaders/skybox/skybox.frag");
    unsigned int v = compileShader(vsrc.c_str(), GL_VERTEX_SHADER);
    unsigned int f = compileShader(fsrc.c_str(), GL_FRAGMENT_SHADER);
    skyboxProgram = glCreateProgram();
    glAttachShader(skyboxProgram, v);
    glAttachShader(skyboxProgram, f);
    glLinkProgram(skyboxProgram);
    glDeleteShader(v); glDeleteShader(f);

    //uniforms cache
    skybox_uMVPLoc = glGetUniformLocation(skyboxProgram, "uProjection");//reuse names below
    
    glUseProgram(skyboxProgram);
    glUniform1i(glGetUniformLocation(skyboxProgram, "skybox"), 0);//texture unit 0
}

//Draw the ray
void drawRay(int vertexCount) {
    glUseProgram(rayShaderProgram);
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
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
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
    if (!gladLoadGL(glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD" << std::endl;
        return -1;
    }

    glEnable(GL_DEPTH_TEST);

    //Prep the circle shape and shaders before we start rendering
    setupCircle(blackHole, window);
    setupRayBuffers();
    setupRayShader();

    setupGridShader();
    setupGrid(40, 1.0f, -1.0f);
    setupAxes(40.0f, -1.0f);

    std::vector<std::string> faces = {
    "textures/skybox/right.png",
    "textures/skybox/left.png",
    "textures/skybox/top.png",
    "textures/skybox/bottom.png",
    "textures/skybox/front.png",
    "textures/skybox/back.png"
    };
    setupSkybox(faces);

    //Main loop that keeps running until we close the window
    while (!glfwWindowShouldClose(window)) {

        //Per frame increments
        //timeElapsed += 0.01f;

        //get the current framebuffer size (for window resizing)
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        float aspect = (float)width / (float)height;

        //Improved time handling
        static double lastFrameTime = 0.0;
        double currentFrameTime = glfwGetTime();
        float deltaTime = (float)(currentFrameTime - lastFrameTime);
        if (lastFrameTime == 0.0)deltaTime = 0.016f;//First frame fallback
        lastFrameTime = currentFrameTime;

        //Poll events first instead so GLFW gets updates from the keyboard/mouse
        glfwPollEvents();

        //Keyboard controls
        //WASD to move the camera target in the cameras forward/right plane
        //Pan speed
        float panSpeed = 2.5f * (deltaTime);

        //computing camera basis from current yaw/pitch
        glm::vec3 camDir;
        camDir.x = cos(camPitch) * cos(camYaw);
        camDir.y = sin(camPitch);
        camDir.z = cos(camPitch) * sin(camYaw);
        camDir = normalize(camDir);

        //right vector (world-up is y)
        glm::vec3 right = normalize(cross(camDir, vec3(0.0f, 1.0f, 0.0f)));
        glm::vec3 forward = normalize(cross(vec3(0.0f, 1.0f, 0.0f), right));//forward parallel to XZ plane

        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) camTarget -= forward * panSpeed;
        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) camTarget += forward * panSpeed;
        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) camTarget += right * panSpeed;
        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) camTarget -= right * panSpeed;
        if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) camTarget.y -= panSpeed;//down
        if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) camTarget.y += panSpeed;//up

        //Orbit controls (keyboard fallback)
        //Left/Right rotate yaw, Up/Down rotate pitch, Z/X zoom
        float rotSpeed = 1.2f * deltaTime; //radians/sec approx
        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)  camYaw -= rotSpeed;
        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) camYaw += rotSpeed;
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)    camPitch += rotSpeed;
        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)  camPitch -= rotSpeed;
        if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) camRadius -= 5.0f * deltaTime;
        if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS) camRadius += 5.0f * deltaTime;

        //clamp pitch and radius
        camPitch = clamp(camPitch, radians(-89.0f), radians(89.0f));
        if (camRadius < 1.0f) camRadius = 1.0f;

        //Mouse drag to rotate
        static double lastMouseX = 0.0, lastMouseY = 0.0;
        static bool firstMouseFrame = true;
        double mouseX, mouseY;
        glfwGetCursorPos(window, &mouseX, &mouseY);

        if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS) {
            if (firstMouseFrame) {
                lastMouseX = mouseX;
                lastMouseY = mouseY;
                firstMouseFrame = false;
            }
            double dx = mouseX - lastMouseX;
            double dy = mouseY - lastMouseY;
            float sens = 0.005f;//mouse sensitivity (tweak)
            camYaw += (float)(dx * sens);
            camPitch -= (float)(dy * sens);//inverted Y look typical for orbit
            lastMouseX = mouseX;
            lastMouseY = mouseY;
        }
        else {
            //reset firstMouseFrame when not dragging so we don't get a jump next time
            firstMouseFrame = true;
        }

        //Set the background clear colour
        glClearColor(0.43137254901960786f, 0.3176470588235294f, 0.5058823529411764f, 1.f);

        //Clears the screen with the current clear color
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        //Use the shader program
        glUseProgram(shaderProgram);

        //Setting up simple perspective projection
        mat4 projection = perspective(radians(45.0f), aspect, 0.1f, 100.0f);
        mat4 view = getOrbitView();
        mat4 model = mat4(1.0f);
        mat4 mvp = projection * view * model;
        glUniformMatrix4fv(glGetUniformLocation(shaderProgram, "uMVP"), 1, GL_FALSE, &mvp[0][0]);

        //Draw world grid & axes (no offset)
        drawGrid(mvp);
        drawAxes(40.0f, -1.0f);

        //Draw skybox
        //Use projection and view without translation:
        glDepthFunc(GL_LEQUAL);//allow skybox to be drawn at depth 1.0
        glDepthMask(GL_FALSE);

        glUseProgram(skyboxProgram);

        //projection: same projection used for rest of scene
        glUniformMatrix4fv(glGetUniformLocation(skyboxProgram, "uProjection"),
            1, GL_FALSE, &projection[0][0]);

        //remove translation from view matrix so skybox stays centered on camera
        //This makes it appear infinite
        glm::mat4 viewNoTranslation = glm::mat4(glm::mat3(view));
        glUniformMatrix4fv(glGetUniformLocation(skyboxProgram, "uView"),
            1, GL_FALSE, &viewNoTranslation[0][0]);

        glBindVertexArray(skyboxVAO);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexture);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);

        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS); 

        //Draw the black hole at its world offset
        if (uOffsetLoc != -1) glUniform3f(uOffsetLoc, blackHole.position.x, blackHole.position.y, blackHole.position.z);
        drawCircle(blackHole, aspect);
        glfwSwapBuffers(window);
        //glfwPollEvents();

        //Base vertical offest for ray changes with elapsed time
        float baseY = 0.5f + 0.5f * sin(timeElapsed);
    }

    //Clean up GPU resources
    //glDeleteVertexArrays(1, &VAO);
    //glDeleteBuffers(1, &VBO);
    //glDeleteProgram(shaderProgram);

    //Cleanup (window and GLFW)
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}