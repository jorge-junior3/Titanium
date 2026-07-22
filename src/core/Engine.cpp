#include "Engine.h"
#include "../renderer/Renderer.h"
#include "../editor/EditorPanels.h"
#include "../editor/Gizmo.h"
#include <glm/gtc/matrix_transform.hpp>
#include <SDL2/SDL.h>
#include <algorithm>
#include "imgui.h"
#include "backends/imgui_impl_vulkan.h"
#include "backends/imgui_impl_sdl2.h"
#include <iostream>

Engine::Engine() {
    m_window   = std::make_unique<Window>("MyEngine", 1280, 720);
    m_camera   = std::make_unique<Camera>(45.0f, 1280.0f / 720.0f, 0.1f, 100.0f);
    m_renderer = std::make_unique<Renderer>(m_window->getHandle());
}

void Engine::run() {
    Uint32 lastTime = SDL_GetTicks();
    static GizmoState g_gizmo;

    while (true) {
        Uint32 now       = SDL_GetTicks();
        float  deltaTime = (now - lastTime) / 1000.0f;
        lastTime         = now;

        // ---- events ----
        SDL_Event event;
        bool quit = false;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) quit = true;
            if (event.type == SDL_WINDOWEVENT &&
                event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                m_renderer->recreateSwapchain();
            }
            m_camera->processEvent(event);
            ImGui_ImplSDL2_ProcessEvent(&event);

            if (event.type == SDL_KEYDOWN && !event.key.repeat) {
                const bool wantsKeyboard = ImGui::GetIO().WantCaptureKeyboard;
                if (!wantsKeyboard) {
                    switch (event.key.keysym.sym) {
                        case SDLK_q:
                            editor::setGizmoMode(GizmoMode::Translate);
                            break;
                        case SDLK_w:
                            editor::setGizmoMode(GizmoMode::Rotate);
                            break;
                        case SDLK_e:
                            editor::setGizmoMode(GizmoMode::Scale);
                            break;
                        case SDLK_n: {
                            const auto& nodes = m_renderer->getSceneNodes();
                            int newIndex = static_cast<int>(nodes.size());
                            m_renderer->addCubeNode("Cube " + std::to_string(newIndex + 1));
                            editor::setSelectedNode(newIndex);
                            editor::refreshSelectedNodeState(*m_renderer);
                            break;
                        }
                        case SDLK_DELETE:
                            if (editor::getSelectedNode() >= 0) {
                                m_renderer->removeSceneNode(editor::getSelectedNode());
                                editor::setSelectedNode(-1);
                            }
                            break;
                        default:
                            break;
                    }
                }
            }
        }
        if (quit) break;

        m_camera->update(deltaTime);

        // ---- UBO ----
        UniformBufferObject ubo{};
        ubo.model            = glm::mat4(1.0f);
        ubo.view             = m_camera->getView();
        ubo.proj             = m_camera->getProjection();
        ubo.camPos           = glm::vec4(m_camera->getPosition(), 1.0f);
        ubo.lightSpaceMatrix = m_renderer->getLightSpaceMatrix();

        glm::vec3 ld = m_renderer->getDirectionalLightDir();
        glm::vec3 lc = m_renderer->getDirectionalLightColor();
        ubo.lightDir   = glm::vec4(m_renderer->getNormalizeLightDir() ?
                                   glm::normalize(ld) : ld, 0.0f);
        ubo.lightColor = glm::vec4(lc * m_renderer->getLightIntensity(), 0.0f);

        auto atmosphere  = m_renderer->getAtmosphereSettings();
        ubo.skyColor     = glm::vec4(atmosphere.skyColor, 1.0f);
        ubo.fogColor     = glm::vec4(atmosphere.fogColor, 1.0f);
        ubo.fogParams    = glm::vec4(atmosphere.fogDensity,
                                     atmosphere.enabled ? 1.0f : 0.0f, 0.0f, 0.0f);

        m_renderer->updateUniformBuffer(ubo);

        // ---- ImGui frame — ONE per game loop iteration ----
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        editor::drawEditorPanels(*m_renderer);

        // ---- gizmo interaction ----
        const auto& nodes      = m_renderer->getSceneNodes();
        int         selected   = editor::getSelectedNode();
        bool        hasSelection = selected >= 0 &&
                                   selected < (int)nodes.size();

        if (hasSelection) {
            const auto& node    = nodes[selected];
            glm::vec3   nodePos = node.position;

            auto gizmoVerts = buildGizmoGeometry(editor::getGizmoMode());
            m_renderer->updateGizmoBuffers(gizmoVerts);
            m_renderer->setGizmoNodePos(nodePos);

            int mx, my;
            Uint32 mouseState = SDL_GetMouseState(&mx, &my);
            int w, h;
            SDL_GetWindowSize(m_window->getHandle(), &w, &h);

            Ray ray = buildRay((float)mx, (float)my, (float)w, (float)h,
                               m_camera->getView(), m_camera->getProjection());

            float tX = editor::getGizmoMode() == GizmoMode::Rotate
                       ? rayCircleHit(ray, nodePos, GizmoAxis::X)
                       : rayAxisHit(ray, nodePos, GizmoAxis::X, 0.12f);
            float tY = editor::getGizmoMode() == GizmoMode::Rotate
                       ? rayCircleHit(ray, nodePos, GizmoAxis::Y)
                       : rayAxisHit(ray, nodePos, GizmoAxis::Y, 0.12f);
            float tZ = editor::getGizmoMode() == GizmoMode::Rotate
                       ? rayCircleHit(ray, nodePos, GizmoAxis::Z)
                       : rayAxisHit(ray, nodePos, GizmoAxis::Z, 0.12f);

            GizmoAxis hovered = GizmoAxis::None;
            float     bestT   = 1e9f;
            if (tX > 0 && tX < bestT) { bestT = tX; hovered = GizmoAxis::X; }
            if (tY > 0 && tY < bestT) { bestT = tY; hovered = GizmoAxis::Y; }
            if (tZ > 0 && tZ < bestT) { bestT = tZ; hovered = GizmoAxis::Z; }

            g_gizmo.hoveredAxis = hovered;
            m_renderer->setGizmoHoveredAxis(
                hovered == GizmoAxis::None ? -1 :
                hovered == GizmoAxis::X    ?  0 :
                hovered == GizmoAxis::Y    ?  1 : 2);

            bool mouseDown = (mouseState & SDL_BUTTON(SDL_BUTTON_LEFT)) != 0;
            bool overImGui = ImGui::GetIO().WantCaptureMouse;

            if (mouseDown && !overImGui && !g_gizmo.dragging &&
                hovered != GizmoAxis::None) {
                g_gizmo.dragging      = true;
                g_gizmo.activeAxis    = hovered;
                g_gizmo.dragOrigin    = nodePos;
                g_gizmo.dragScaleOrigin = node.scale;
                g_gizmo.dragRotationOrigin = node.rotation;
                g_gizmo.dragStartMouse = glm::vec2((float)mx, (float)my);

                auto axisVec = [](GizmoAxis axis) {
                    switch (axis) {
                        case GizmoAxis::X: return glm::vec3(1,0,0);
                        case GizmoAxis::Y: return glm::vec3(0,1,0);
                        case GizmoAxis::Z: return glm::vec3(0,0,1);
                        default: return glm::vec3(0.0f);
                    }
                };

                if (editor::getGizmoMode() == GizmoMode::Rotate) {
                    glm::vec3 circlePoint;
                    if (rayCircleClosestPoint(ray, nodePos, hovered, &circlePoint)) {
                        g_gizmo.dragStart = circlePoint;
                    } else {
                        glm::vec3 planePoint;
                        if (rayPlaneIntersect(ray, nodePos, axisVec(hovered), &planePoint)) {
                            g_gizmo.dragStart = planePoint;
                        } else {
                            g_gizmo.dragStart = nodePos + axisVec(hovered) * 0.01f;
                        }
                    }
                } else {
                    glm::vec3 axisPoint;
                    if (rayAxisClosestPoint(ray, nodePos, hovered, &axisPoint, 1000.0f)) {
                        g_gizmo.dragStart = axisPoint;
                    } else {
                        g_gizmo.dragStart = nodePos;
                    }
                }
            }

            if (g_gizmo.dragging) {
                if (!mouseDown) {
                    g_gizmo.dragging   = false;
                    g_gizmo.activeAxis = GizmoAxis::None;
                } else {
                    auto axisVec = [](GizmoAxis axis) {
                        switch (axis) {
                            case GizmoAxis::X: return glm::vec3(1,0,0);
                            case GizmoAxis::Y: return glm::vec3(0,1,0);
                            case GizmoAxis::Z: return glm::vec3(0,0,1);
                            default: return glm::vec3(0.0f);
                        }
                    };

                    switch (editor::getGizmoMode()) {
                        case GizmoMode::Translate: {
                            glm::vec3 current;
                            if (rayAxisClosestPoint(ray, g_gizmo.dragOrigin,
                                                     g_gizmo.activeAxis, &current, 1000.0f)) {
                                glm::vec3 delta   = current - g_gizmo.dragStart;
                                glm::vec3 newPos  = g_gizmo.dragOrigin + delta;
                                m_renderer->setSceneNodeTransform(selected, newPos,
                                                                  node.rotation, node.scale);
                                editor::refreshSelectedNodeState(*m_renderer);
                            }
                            break;
                        }
                        case GizmoMode::Rotate: {
                            glm::vec3 currentCircle;
                            if (rayCircleClosestPoint(ray, g_gizmo.dragOrigin,
                                                      g_gizmo.activeAxis,
                                                      &currentCircle,
                                                      0.82f, 0.08f)) {
                                glm::vec3 startVec = glm::normalize(g_gizmo.dragStart - g_gizmo.dragOrigin);
                                glm::vec3 curVec   = glm::normalize(currentCircle - g_gizmo.dragOrigin);
                                if (glm::length(startVec) > 1e-6f && glm::length(curVec) > 1e-6f) {
                                    float cross = glm::dot(glm::cross(startVec, curVec), axisVec(g_gizmo.activeAxis));
                                    float angle = std::atan2(glm::length(glm::cross(startVec, curVec)), glm::dot(startVec, curVec));
                                    if (cross < 0.0f) angle = -angle;

                                    glm::vec3 newRot = g_gizmo.dragRotationOrigin;
                                    float angleDeg   = angle * 180.0f / 3.14159265358979323846f;
                                    if (g_gizmo.activeAxis == GizmoAxis::X) newRot.x += angleDeg;
                                    if (g_gizmo.activeAxis == GizmoAxis::Y) newRot.y += angleDeg;
                                    if (g_gizmo.activeAxis == GizmoAxis::Z) newRot.z += angleDeg;
                                    m_renderer->setSceneNodeTransform(selected, node.position, newRot, node.scale);
                                    editor::refreshSelectedNodeState(*m_renderer);
                                }
                            }
                            break;
                        }
                        case GizmoMode::Scale: {
                            glm::vec3 current;
                            if (rayAxisClosestPoint(ray, g_gizmo.dragOrigin,
                                                     g_gizmo.activeAxis, &current, 1000.0f)) {
                                glm::vec3 axisDir = axisVec(g_gizmo.activeAxis);
                                float startDist = glm::dot(g_gizmo.dragStart - g_gizmo.dragOrigin, axisDir);
                                float curDist   = glm::dot(current - g_gizmo.dragOrigin, axisDir);
                                float delta     = curDist - startDist;
                                glm::vec3 newScale = g_gizmo.dragScaleOrigin;
                                if (g_gizmo.activeAxis == GizmoAxis::X) newScale.x = std::max(0.01f, g_gizmo.dragScaleOrigin.x + delta * 0.5f);
                                if (g_gizmo.activeAxis == GizmoAxis::Y) newScale.y = std::max(0.01f, g_gizmo.dragScaleOrigin.y + delta * 0.5f);
                                if (g_gizmo.activeAxis == GizmoAxis::Z) newScale.z = std::max(0.01f, g_gizmo.dragScaleOrigin.z + delta * 0.5f);
                                m_renderer->setSceneNodeTransform(selected, node.position, node.rotation, newScale);
                                editor::refreshSelectedNodeState(*m_renderer);
                            }
                            break;
                        }
                    }
                }
            }
        } else {
            m_renderer->updateGizmoBuffers({});
            m_renderer->setGizmoNodePos(glm::vec3(0));
            m_renderer->setGizmoHoveredAxis(-1);
            g_gizmo.dragging   = false;
            g_gizmo.activeAxis = GizmoAxis::None;
        }

        // ---- end ImGui frame ----
        ImGui::Render();

        m_renderer->drawFrame();
    }
}