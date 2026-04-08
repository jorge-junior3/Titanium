#include "Window.h"
#include <stdexcept>

Window::Window(const std::string& title, int width, int height)
    : m_width(width), m_height(height) {
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        throw std::runtime_error(SDL_GetError());

    m_window = SDL_CreateWindow(
        title.c_str(),
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_VULKAN | SDL_WINDOW_SHOWN
    );

    if (!m_window)
        throw std::runtime_error(SDL_GetError());
}

Window::~Window() {
    SDL_DestroyWindow(m_window);
    SDL_Quit();
}

bool Window::pollEvents() {
    SDL_Event event;
    while (SDL_PollEvent(&event))
        if (event.type == SDL_QUIT) return false;
    return true;
}