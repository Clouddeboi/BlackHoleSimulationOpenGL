#include <glad/glad.h>
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

//Draw the ray
void drawRay(int vertexCount) {
    glUseProgram(rayShaderProgram);
    glLineWidth(2.0f);
    glBindVertexArray(rayVAO);
    glDrawArrays(GL_LINE_STRIP, 0, vertexCount);
    glBindVertexArray(0);
}

//Initialize GR state from current p.pos (screen coords) and p.vel (direction only).
//void initParticleGR(Particle& p, const BlackHole& bh, float aspect) {
//
//    //Reduce scaling to avoid overly strong gravitational effects
//    float M = bh.mass / 100;
//
//    //Converts the bh pos from screen coords to render coords accounting for the aspect ratio
//    vec3 bhRenderPos = vec3(bh.position.x / aspect, bh.position.y);
//
//    //Calculates the vector from the bh to the particles current pos in render coords
//    vec3 rvec_render = p.pos - bhRenderPos;
//
//    //Transforms the render vector into physical coords by reapplying the aspect ratio to the x component
//    float phys_x = rvec_render.x * aspect;
//    float phys_y = rvec_render.y;
//    vec3 rvec_phys = vec3(phys_x, phys_y);
//
//    //Computes the initial radial distance from the bh to the particle (euclidean distance)
//    float r0 = length(rvec_phys);
//
//    //Check if the particle is too close to the bh
//    //If it is, kill it to simulate it crossing the event horizon
//    if (r0 <= 1e-6f) { p.alive = false; return; }
//
//    //Calculate the initial angular coordinate of the particle relative to the bh
//    float phi0 = atan2(phys_y, phys_x);
//
//    //Normalizes the particles velocity vector to get a unit direction
//    vec3 dir_render = normalize(p.vel);
//
//    //Adjusts the direction vector to physical coordinates and renormalizes it
//    vec3 dphys = vec3(dir_render.x * aspect, dir_render.y);
//    float dphys_len = length(dphys);
//    vec3 dir_phys = dphys / dphys_len;
//
//    //Computes the impact parameter (b)
//    //This determines the angular momentum (L) for the light ray
//    float b = fabs(rvec_phys.x * dir_phys.y - rvec_phys.y * dir_phys.x);
//    float L = b;
//
//    //Veff determines the radial motion stability
//    //Veff is the default if the ray is too close to avoid division issues
//    float Veff = 0.0f;
//    if (r0 > 1e-8f) Veff = (1.0f - 2.0f * M / r0) * (L * L) / (r0 * r0);
//
//    //Computes the square of the radial momentum magnitude and its positive root
//    //This infulences if the particle goes inwards or outwards
//    float pr2 = 1.0f - Veff;
//    if (pr2 < 0.0f) pr2 = 0.0f;
//    float prMag = sqrt(pr2);
//
//    //Determines the sign of the radial momentum based on the direction of motion
//    //This reflects the particles radial velocity direction consistent with its initial trajectory
//    float radialDot = dot(rvec_phys, dir_phys);
//    float pr = (radialDot < 0.0f) ? -prMag : prMag;
//
//    //Stores the computed GR state in the particle object
//    //These values are used to calculate the particles trajectory
//    //This will enable bending or orbiting based on GR effects
//    p.r = r0;
//    p.phi = phi0;
//    p.pr = pr;
//    p.L = L;
//    p.grInit = true;
//}
//
////One RK4 step for null geodesic in (r, pr, phi)
//void integrateParticleRK4_GR(const BlackHole& bh, Particle& p, float dlambda, float aspect) {
//
//    //Ensures that the particles GR state is initialized
//    if (!p.grInit) {
//        initParticleGR(p, bh, aspect);
//        if (!p.grInit) return;
//    }
//
//    //Defines the gravitational parameter M
//    float M = bh.mass / 100;
//
//    //Lambda function to compute the derivatives of r, pr, phi
//    //These derivatives decide the particles motion in curved spacetime
//    auto deriv = [&](float r, float pr, float L)->vec3 {
//        float invr = (r > 1e-8f) ? 1.0f / r : 0.0f;
//        float invr2 = invr * invr;
//        float invr3 = invr2 * invr;
//        float invr4 = invr3 * invr;
//        float dr = pr;
//        float dpr = L * L * (invr3 - 3.0f * M * invr4);
//        float dphi = L * invr2;
//        return vec3(dr, dpr, dphi);
//        };
//
//    //Stores the initial values of the particles state variables
//    float r0 = p.r, pr0 = p.pr, phi0 = p.phi, L = p.L;
//
//    //Computes Rk4 increments
//    //This estimates the next stage
//    vec3 k1 = deriv(r0, pr0, L);
//    vec3 k2 = deriv(r0 + 0.5f * dlambda * k1.x, pr0 + 0.5f * dlambda * k1.y, L);
//    vec3 k3 = deriv(r0 + 0.5f * dlambda * k2.x, pr0 + 0.5f * dlambda * k2.y, L);
//    vec3 k4 = deriv(r0 + dlambda * k3.x, pr0 + dlambda * k3.y, L);
//
//    //Updates the particles state variables using the rk4 avg
//    p.r = r0 + (dlambda / 6.0f) * (k1.x + 2.0f * k2.x + 2.0f * k3.x + k4.x);
//    p.pr = pr0 + (dlambda / 6.0f) * (k1.y + 2.0f * k2.y + 2.0f * k3.y + k4.y);
//    p.phi = phi0 + (dlambda / 6.0f) * (k1.z + 2.0f * k2.z + 2.0f * k3.z + k4.z);
//
//    //Checks if the particle has crossed the event horizon and kills it if it has
//    if (p.r <= bh.r_s) { p.alive = false; return; }
//
//    //Updates the particles screen pos from its polar coords
//    float x = p.r * cos(p.phi);
//    float y = p.r * sin(p.phi);
//    p.pos.x = x / aspect + bh.position.x / aspect;
//    p.pos.y = y + bh.position.y;
//
//    //Updates the particles velocity vector in screen coords
//    float dr = p.pr;
//    float dphi = (p.L / (p.r * p.r));
//    float vx = dr * cos(p.phi) - p.r * dphi * sin(p.phi);
//    float vy = dr * sin(p.phi) + p.r * dphi * cos(p.phi);
//    float vlen = sqrt(vx * vx + vy * vy);
//    p.vel = (vlen > 1e-9f) ? vec3(vx / vlen, vy / vlen) : vec3(1.0f, 0.0f);
//}

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

    glEnable(GL_DEPTH_TEST);

    //Prep the circle shape and shaders before we start rendering
    setupCircle(blackHole, window);
    setupRayBuffers();
    setupRayShader();

    setupGridShader();
    setupGrid(40, 1.0f, -1.0f);
    setupAxes(40.0f, -1.0f);

    //Initialize particles once
    //particles.resize(numRays);
    //for (int i = 0; i < numRays; ++i) {
    //    float yStart = 0.5f - 0.1f + i * 0.08f;
    //    vec3 startPos = vec3(-1.0f / 1.0f, yStart);
    //    vec3 startDir = normalize(vec3(1.0f, 0.0f));
    //    float speed = 0.01f;
    //    particles[i].pos = startPos;
    //    particles[i].vel = startDir * speed;
    //    particles[i].trail.clear();
    //    particles[i].trail.reserve(maxTrailLen);
    //    particles[i].trail.push_back(particles[i].pos);
    //    particles[i].alive = true;
    //}

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

        //Draw the black hole at its world offset
        if (uOffsetLoc != -1) glUniform3f(uOffsetLoc, blackHole.position.x, blackHole.position.y, blackHole.position.z);
        drawCircle(blackHole, aspect);
        glfwSwapBuffers(window);
        //glfwPollEvents();

        //Draw a ray
        //if (uOffsetLoc != -1) glUniform2f(uOffsetLoc, 0.0f, 0.0f);

        //Base vertical offest for ray changes with elapsed time
        float baseY = 0.5f + 0.5f * sin(timeElapsed);

        //Integration param
        float dt = 0.004f;

        //for (int i = 0; i < numRays; ++i) {
        //    Particle& p = particles[i];

        //    //Random float helper
        //    auto randf = [](float min, float max) {
        //        return min + (max - min) * (rand() / (float)RAND_MAX);
        //        };

        //    vec3 bhRenderPos = vec3(blackHole.position.x / aspect, blackHole.position.y);

        //    if (!p.alive || length(p.pos - bhRenderPos) <= blackHole.r_s ||
        //        fabs(p.pos.x) > 5.0f || fabs(p.pos.y) > 5.0f)
        //    {
        //        int edge = rand() % 4;//Choose one of four edges: 0=left,1=right,2=top,3=bottom
        //        vec3 spawnPos;
        //        vec3 baseVel;

        //        switch (edge) {
        //        case 0: spawnPos = vec3(-5.0f / aspect, randf(-5.0f, 5.0f)); baseVel = vec3(1.0f, 0.0f); break;
        //        case 1: spawnPos = vec3(5.0f / aspect, randf(-5.0f, 5.0f)); baseVel = vec3(-1.0f, 0.0f); break;
        //        case 2: spawnPos = vec3(randf(-5.0f / aspect, 5.0f / aspect), 5.0f); baseVel = vec3(0.0f, -1.0f); break;
        //        case 3: spawnPos = vec3(randf(-5.0f / aspect, 5.0f / aspect), -5.0f); baseVel = vec3(0.0f, 1.0f); break;
        //        }

        //        //Add small random angle offset for velocity
        //        float angleOffset = randf(-0.3f, 0.3f);
        //        float ca = cos(angleOffset), sa = sin(angleOffset);
        //        vec3 vel = vec3(baseVel.x * ca - baseVel.y * sa, baseVel.x * sa + baseVel.y * ca);

        //        p.pos = spawnPos;
        //        p.vel = normalize(vel);
        //        p.trail.clear();
        //        p.trail.push_back(p.pos);
        //        p.alive = true;
        //        p.grInit = false;
        //        initParticleGR(p, blackHole, aspect);
        //    }

        //    //advance particle one integration step
        //    float dlambda = 0.0025f;
        //    integrateParticleRK4_GR(blackHole, p, dlambda, aspect);

        //    //push into trail
        //    p.trail.push_back(p.pos);
        //    if ((int)p.trail.size() > maxTrailLen) p.trail.erase(p.trail.begin());

        //    //upload trail to VBO and draw
        //    if (!p.trail.empty()) {
        //        GLsizeiptr size = (GLsizeiptr)p.trail.size() * sizeof(vec3);
        //        glBindBuffer(GL_ARRAY_BUFFER, rayVBO);
        //        glBufferSubData(GL_ARRAY_BUFFER, 0, size, p.trail.data());
        //        glBindBuffer(GL_ARRAY_BUFFER, 0);

        //        //set ray color
        //        if (uColorLoc != -1) glUniform4f(uColorLoc, 0.0f, 0.0f, 0.0f, 1.0f);

        //        //draw trail
        //        drawRay((int)p.trail.size());

        //        //draw head as point
        //        glPointSize(6.0f);
        //        glBindVertexArray(rayVAO);
        //        glDrawArrays(GL_POINTS, 0, 1);
        //        glBindVertexArray(0);
        //    }
        //}

        //Swap the front and back buffers
        //(Double Buffering)
        //glfwSwapBuffers(window);

        ////Process any input or events
        //glfwPollEvents();
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