//Movement and matrices

#include "../headers/camera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include <iostream>

//----------------- Constructor -----------------
Camera::Camera(float fov, float aspect, float nearPlane, float farPlane)
    : m_position(0.0f, 0.0f, 3.0f),
    m_front(0.0f, 0.0f, -1.0f),
    m_up(0.0f, 1.0f, 0.0f),
    m_yaw(-90.0f), m_pitch(0.0f),
    m_speed(2.5f), m_sensitivity(0.1f),
    m_fov(fov), m_aspect(aspect), m_near(nearPlane), m_far(farPlane),
    m_firstMouse(true), m_lastX(0.0f), m_lastY(0.0f)
{
    updateVectors();
}

//----------------- Update -----------------
void Camera::update(float deltaTime) {
    GLFWwindow* window = glfwGetCurrentContext();
    if (!window) return;

    float velocity = m_speed * deltaTime;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        m_position += m_front * velocity;
        std::cout << "W pressed - moving forward, position: "
            << m_position.x << ", "
            << m_position.y << ", "
            << m_position.z << std::endl;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        m_position -= m_front * velocity;
        std::cout << "S pressed - moving backward, position: "
            << m_position.x << ", "
            << m_position.y << ", "
            << m_position.z << std::endl;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        m_position -= m_right * velocity;
        std::cout << "A pressed - moving left, position: "
            << m_position.x << ", "
            << m_position.y << ", "
            << m_position.z << std::endl;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        m_position += m_right * velocity;
        std::cout << "D pressed - moving right, position: "
            << m_position.x << ", "
            << m_position.y << ", "
            << m_position.z << std::endl;
    }
}

//----------------- Mouse -----------------
void Camera::processMouse(float xpos, float ypos) {
    if (m_firstMouse) {
        m_lastX = xpos;
        m_lastY = ypos;
        m_firstMouse = false;
    }

    float xoffset = xpos - m_lastX;
    float yoffset = m_lastY - ypos; //reversed
    m_lastX = xpos;
    m_lastY = ypos;

    xoffset *= m_sensitivity;
    yoffset *= m_sensitivity;

    m_yaw += xoffset;
    m_pitch += yoffset;

    if (m_pitch > 89.0f) m_pitch = 89.0f;
    if (m_pitch < -89.0f) m_pitch = -89.0f;

    updateVectors();
}

//----------------- Getters -----------------
glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(m_position, m_position + m_front, m_up);
}

glm::mat4 Camera::getProjectionMatrix() const {
    return glm::perspective(glm::radians(m_fov), m_aspect, m_near, m_far);
}

//----------------- Update Vectors -----------------
void Camera::updateVectors() {
    glm::vec3 front;
    front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    front.y = sin(glm::radians(m_pitch));
    front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    m_front = glm::normalize(front);

    m_right = glm::normalize(glm::cross(m_front, glm::vec3(0.0f, 1.0f, 0.0f)));
    m_up = glm::normalize(glm::cross(m_right, m_front));
}
