#include "Engine.h"
#include "../renderer/Renderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <SDL2/SDL.h>
#include <iostream>

Engine::Engine() {
    m_window   = std::make_unique<Window>("MyEngine", 1280, 720);
    m_camera   = std::make_unique<Camera>(45.0f, 1280.0f / 720.0f, 0.1f, 100.0f);
    m_renderer = std::make_unique<Renderer>(m_window->getHandle());
}

void Engine::run() {
    Uint32 lastTime = SDL_GetTicks();

    while (true) {
        Uint32 now       = SDL_GetTicks();
        float  deltaTime = (now - lastTime) / 1000.0f;
        lastTime         = now;

        SDL_Event event;
        bool quit = false;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) quit = true;
            m_camera->processEvent(event);
        }
        if (quit) break;

        m_camera->update(deltaTime);

        UniformBufferObject ubo{};
        ubo.model  = glm::mat4(1.0f);
        ubo.view   = m_camera->getView();
        ubo.proj   = m_camera->getProjection();
        ubo.camPos = glm::vec4(m_camera->getPosition(), 1.0f);  // ← must be inside this block
        ubo.lightSpaceMatrix = m_renderer->getLightSpaceMatrix();
        // directional light incoming from above and angled towards +X,+Z
        ubo.lightDir = glm::vec4(glm::normalize(glm::vec3(0.3f, -1.0f, 0.2f)), 0.0f);
        ubo.lightColor = glm::vec4(12.0f, 11.2f, 10.4f, 0.0f);

        m_renderer->updateUniformBuffer(ubo);
        m_renderer->drawFrame();
    }
}