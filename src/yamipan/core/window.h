// The OS window. Created on the main thread, which also pumps its messages: on Windows a drag or resize
// runs a modal loop inside the pump, so nothing else may live on that thread. State changes are posted to
// it from wherever they come and waited for.
#pragma once

#include <expected>
#include <functional>
#include <string>
#include <utility>

#include <SDL3/SDL_video.h>

#include "yamipan/core/config.h"

namespace yamipan
{

// Runs `work` on the main thread and waits for it (inline when already there).
void runOnMainThread(const std::function<void()> &work);

class Window
{
public:
    static std::expected<Window, std::string> create(const DisplaySettings &settings);

    Window(Window &&other) noexcept;
    Window &operator=(Window &&) = delete;
    ~Window();

    SDL_Window *get() const { return m_window; }

    // Its size now, in pixels. Any thread.
    std::pair<int, int> pixelSize() const;

    WindowMode mode() const { return m_mode; }
    void setMode(WindowMode mode);
    void toggleFullscreen();

    // The size to come back to: while fullscreen the live size is the desktop's, and saving that
    // would lose the window's shape for good.
    std::pair<int, int> windowedSize() const { return {m_windowedWidth, m_windowedHeight}; }

    // From the event pump, on a resize.
    void noteResized();

private:
    Window(SDL_Window *window, const DisplaySettings &settings);

    SDL_Window *m_window;
    WindowMode m_mode;
    int m_windowedWidth;
    int m_windowedHeight;
};

} // namespace yamipan
