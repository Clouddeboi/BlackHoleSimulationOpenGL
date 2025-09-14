/*
	Movement and view matrix calculations for the camera.
*/

#include "../headers/camera.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <GLFW/glfw3.h>
#include <iostream>

//----------------- Constructor -----------------
Camera::Camera(float fov, float aspect, float nearPlane, float farPlane)
	: m_position(0.0f, 0.0f, 30.0f),//Start position
	m_front(0.0f, 0.0f, -1.0f),//Initial front vector (forward direction)
	m_up(0.0f, 1.0f, 0.0f),//Initial up vector
	m_yaw(-90.0f), m_pitch(0.0f),//Yaw and pitch angles
	m_speed(2.5f), m_sensitivity(0.1f),//Movement speed and mouse sensitivity
	m_fov(fov), m_aspect(aspect), m_near(nearPlane), m_far(farPlane),//Near and far plane clipping
	m_firstMouse(true), m_lastX(0.0f), m_lastY(0.0f)//Mouse state
{
	updateVectors();//Calculate initial direction vectors
}

//----------------- Update -----------------
void Camera::update(float deltaTime) {
    GLFWwindow* window = glfwGetCurrentContext();
    if (!window) return;

    //Speed boost with Left Shift
    float speedMultiplier = (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) ? 4.0f : 1.0f;
    float velocity = m_speed * speedMultiplier * deltaTime;

	//WASD for forward/backward/left/right
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
        m_position += m_front * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        m_position -= m_front * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        m_position -= m_right * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
        m_position += m_right * velocity;
    }

	//E/Q for up/down
    if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS) {
        m_position += m_up * velocity;
    }
    if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS) {
        m_position -= m_up * velocity;
    }

    //Focus on black hole (reset position and orientation)
    if (glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS) {
        m_position = glm::vec3(0.0f, 0.0f, 40.0f);
        m_yaw = -90.0f;
        m_pitch = 0.0f;
        updateVectors();
    }
}

//----------------- Mouse -----------------
void Camera::processMouse(float xpos, float ypos) {
	//Initialize lastX and lastY on first mouse movement
    if (m_firstMouse) {
        m_lastX = xpos;
        m_lastY = ypos;
        m_firstMouse = false;
    }

	//Calculate offset
    float xoffset = xpos - m_lastX;
    float yoffset = m_lastY - ypos; //reversed
    m_lastX = xpos;
    m_lastY = ypos;

	//Apply sensitivity
    xoffset *= m_sensitivity;
    yoffset *= m_sensitivity;

	//Update yaw and pitch
    m_yaw += xoffset;
    m_pitch += yoffset;

    //Clamp pitch to prevent flipping
    if (m_pitch > 89.0f) m_pitch = 89.0f;
    if (m_pitch < -89.0f) m_pitch = -89.0f;

	//Update direction vectors
    updateVectors();
}

//----------------- Getters -----------------
glm::mat4 Camera::getViewMatrix() const {
    return glm::lookAt(m_position, m_position + m_front, m_up);
}

glm::mat4 Camera::getProjectionMatrix() const {
    return glm::perspective(glm::radians(m_fov), m_aspect, m_near, m_far);
}
glm::mat4 Camera::getView() const {
    return glm::lookAt(m_position, m_position + m_front, m_up);
}

glm::mat4 Camera::getProj() const {
    return glm::perspective(glm::radians(m_fov), m_aspect, m_near, m_far);
}

//----------------- Update Vectors -----------------
void Camera::updateVectors() {
	//Calculate the new front vector
    glm::vec3 front;
    front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    front.y = sin(glm::radians(m_pitch));
    front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
    m_front = glm::normalize(front);

    m_right = glm::normalize(glm::cross(m_front, glm::vec3(0.0f, 1.0f, 0.0f)));
    m_up = glm::normalize(glm::cross(m_right, m_front));
}

//----------------- Get UBO -----------------
CameraUBO Camera::getUBO() const {
	//Fill UBO structure for GPU
    CameraUBO data{};
    data.view = getView();
    data.proj = getProj();
    data.invView = glm::inverse(getView());
    data.invProj = glm::inverse(getProj());
    data.position = glm::vec4(m_position, 1.0f);
    return data;
}
