#pragma once
#include <SDL2/SDL.h>
#include <string>

class Window {
public:
    Window(const std::string& title, int width, int height);
    ~Window();

    bool pollEvents();
    SDL_Window* getHandle() const { return m_window; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

private:
    SDL_Window* m_window = nullptr;
    int m_width, m_height;
};