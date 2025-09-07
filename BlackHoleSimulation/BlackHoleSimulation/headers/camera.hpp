#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

struct CameraUBO {
    glm::mat4 view;
    glm::mat4 proj;
    glm::mat4 invView;
    glm::mat4 invProj;
    glm::vec4 position;//vec4 for alignment
};

//Simple free-fly camera
class Camera {
public:
    Camera(float fov, float aspect, float nearPlane, float farPlane);

    //Update camera based on input
    void update(float deltaTime);
    void focusOn(const glm::vec3& pos);

    //Getters
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;
    glm::vec3 getPosition() const { return m_position; }
    glm::mat4 getView() const;
    glm::mat4 getProj() const;

    //Mouse input
    void processMouse(float xpos, float ypos);
    CameraUBO getUBO() const;

private:
    //State
    glm::vec3 m_position;
    glm::vec3 m_front;
    glm::vec3 m_up;
    glm::vec3 m_right;

    float m_yaw;
    float m_pitch;
    float m_speed;
    float m_sensitivity;

    //Projection
    float m_fov, m_aspect, m_near, m_far;

    //For mouse
    bool m_firstMouse;
    float m_lastX, m_lastY;

    void updateVectors();
};
