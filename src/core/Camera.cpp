#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cmath>

Camera::Camera(float fov, float aspect, float near, float far)
    : m_fov(fov), m_aspect(aspect), m_near(near), m_far(far) {}

void Camera::processEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
        bool pressed = event.type == SDL_KEYDOWN;
        switch (event.key.keysym.sym) {
            case SDLK_w: m_keys[0] = pressed; break;
            case SDLK_a: m_keys[1] = pressed; break;
            case SDLK_s: m_keys[2] = pressed; break;
            case SDLK_d: m_keys[3] = pressed; break;
            case SDLK_e: m_keys[4] = pressed; break;
            case SDLK_q: m_keys[5] = pressed; break;
        }
    }

    if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
        if (event.button.button == SDL_BUTTON_RIGHT) {
            if (event.type == SDL_MOUSEBUTTONDOWN) {
                m_mouseActive = true;
                SDL_SetRelativeMouseMode(SDL_TRUE);
                SDL_ShowCursor(SDL_DISABLE);
            } else {
                m_mouseActive = false;
                SDL_SetRelativeMouseMode(SDL_FALSE);
                SDL_ShowCursor(SDL_ENABLE);
            }
        }
    }

    if (event.type == SDL_MOUSEMOTION && m_mouseActive) {
        float xoffset = event.motion.xrel * m_sensitivity;
        float yoffset = -event.motion.yrel * m_sensitivity;
        m_yaw += xoffset;
        m_pitch += yoffset;

        if (m_pitch > 89.0f) m_pitch = 89.0f;
        if (m_pitch < -89.0f) m_pitch = -89.0f;

        glm::vec3 front;
        front.x = cos(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
        front.y = sin(glm::radians(m_pitch));
        front.z = sin(glm::radians(m_yaw)) * cos(glm::radians(m_pitch));
        m_front = glm::normalize(front);
    }
}

void Camera::update(float deltaTime) {
    glm::vec3 right = glm::normalize(glm::cross(m_front, m_up));
    glm::vec3 cameraUp = glm::normalize(glm::cross(right, m_front));
    float velocity  = m_speed * deltaTime;

    if (m_keys[0]) m_position += m_front * velocity;  // W
    if (m_keys[2]) m_position -= m_front * velocity;  // S
    if (m_keys[1]) m_position -= right   * velocity;  // A
    if (m_keys[3]) m_position += right   * velocity;  // D
    if (m_keys[4]) m_position += cameraUp * velocity; // E
    if (m_keys[5]) m_position -= cameraUp * velocity; // Q
}

glm::mat4 Camera::getView() const {
    return glm::lookAt(m_position, m_position + m_front, m_up);
}

glm::mat4 Camera::getProjection() const {
    glm::mat4 proj = glm::perspective(glm::radians(m_fov), m_aspect, m_near, m_far);
    proj[1][1] *= -1; // flip Y — Vulkan's Y axis is flipped vs OpenGL
    return proj;
}