#include "yamipan/core/window.h"

#include <format>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>

#include "yamipan/core/logger.h"

namespace yamipan
{

void runOnMainThread(const std::function<void()> &work)
{
    const auto call = [](void *data) { (*static_cast<const std::function<void()> *>(data))(); };
    if (!SDL_RunOnMainThread(call, const_cast<std::function<void()> *>(&work), true))
        log::error("SDL_RunOnMainThread: {}", SDL_GetError());
}

std::expected<Window, std::string> Window::create(const DisplaySettings &settings)
{
    SDL_Window *const window = SDL_CreateWindow("YamiPan", settings.windowWidth, settings.windowHeight,
                                                SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    if (window == nullptr)
        return std::unexpected(std::format("SDL_CreateWindow: {}", SDL_GetError()));

    Window result(window, settings);
    result.setMode(settings.windowMode);
    return result;
}

Window::Window(SDL_Window *window, const DisplaySettings &settings)
    : m_window(window), m_mode(WindowMode::Windowed), m_windowedWidth(settings.windowWidth),
      m_windowedHeight(settings.windowHeight)
{
}

Window::Window(Window &&other) noexcept
    : m_window(std::exchange(other.m_window, nullptr)), m_mode(other.m_mode),
      m_windowedWidth(other.m_windowedWidth), m_windowedHeight(other.m_windowedHeight)
{
}

Window::~Window()
{
    if (m_window != nullptr)
        SDL_DestroyWindow(m_window);
}

std::pair<int, int> Window::pixelSize() const
{
    int width = 0;
    int height = 0;
    SDL_GetWindowSizeInPixels(m_window, &width, &height);
    return {width, height};
}

void Window::setMode(WindowMode mode)
{
    runOnMainThread([this, mode] {
        // Borderless only: no video mode of its own, so alt-tab is instant and the desktop stays as
        // it is. A low render resolution is the render-resolution setting's job.
        if (mode == WindowMode::Borderless)
            SDL_SetWindowFullscreenMode(m_window, nullptr);
        if (!SDL_SetWindowFullscreen(m_window, mode == WindowMode::Borderless))
        {
            log::error("SDL_SetWindowFullscreen: {}", SDL_GetError());
            return;
        }
        // The change lands some frames later otherwise, and the next frame would be recorded at a
        // size thrown away a frame on.
        SDL_SyncWindow(m_window);
        m_mode = mode;
    });
}

void Window::toggleFullscreen()
{
    setMode(m_mode == WindowMode::Windowed ? WindowMode::Borderless : WindowMode::Windowed);
}

void Window::noteResized()
{
    if (m_mode != WindowMode::Windowed)
        return;
    SDL_GetWindowSize(m_window, &m_windowedWidth, &m_windowedHeight);
}

} // namespace yamipan
