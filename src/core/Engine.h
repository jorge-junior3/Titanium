#pragma once
#include "Window.h"
#include "Camera.h"
#include "../renderer/Renderer.h"
#include <memory>

class Engine {
public:
    Engine();
    void run();

private:
    std::unique_ptr<Window>   m_window;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<Camera>   m_camera;
};