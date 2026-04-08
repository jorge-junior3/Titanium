#pragma once
#include <glm/glm.hpp>
#include <SDL2/SDL.h>

class Camera {
public:
    Camera(float fov, float aspect, float near, float far);

    void processEvent(const SDL_Event& event);
    void setMouseActive(bool active);
    void update(float deltaTime);

    glm::mat4 getView()       const;
    glm::mat4 getProjection() const;
    glm::vec3 getPosition() const { return m_position; }

private:
    glm::vec3 m_position = {0.0f, 0.0f,  3.0f};
    glm::vec3 m_front    = {0.0f, 0.0f, -1.0f};
    glm::vec3 m_up       = {0.0f, 1.0f,  0.0f};
    

    float m_yaw         = -90.0f;
    float m_pitch       =   0.0f;
    float m_speed       =   3.0f;
    float m_sensitivity =   0.1f;

    float m_fov, m_aspect, m_near, m_far;

    bool m_mouseActive = false;
    bool m_keys[6]     = {false, false, false, false, false, false}; // W A S D E Q
};