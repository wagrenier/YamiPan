// The host's ImGui layer: a menu bar over the game (F1, or L3 + R1 on a pad) and the windows it opens.
// Everything here runs on the render thread. Each feature is a Panel: what it adds to the menu bar and, if
// it has one, its window.
#pragma once

#include <memory>
#include <vector>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gpu.h>

namespace yamipan
{
class Application;
}

namespace yamipan::rendering
{
class Renderer;
}

namespace yamipan::ui
{

class Panel
{
public:
    virtual ~Panel() = default;

    // Inside a menu bar: its menu, if it has one.
    virtual void drawMenu() {}

    // Each frame, whether or not the bar is open: its window, if open.
    virtual void drawWindow() {}

    // An event the panel takes for itself (a rebinding waiting for a key). Answers true when it did.
    virtual bool handleEvent(const SDL_Event &) { return false; }
};

class Overlay
{
public:
    explicit Overlay(Application &application);
    ~Overlay();

    Overlay(const Overlay &) = delete;
    Overlay &operator=(const Overlay &) = delete;

    bool initializeOnRenderThread(SDL_GPUTextureFormat targetFormat);
    void shutdownOnRenderThread();

    void processEvent(const SDL_Event &event);
    void beginFrame();
    void draw(rendering::Renderer &renderer);
    void endFrame();

    void prepareDrawData(SDL_GPUCommandBuffer *commands);
    void renderDrawData(SDL_GPUCommandBuffer *commands, SDL_GPURenderPass *pass);

private:
    bool menuChordPressed(const SDL_Event &event);

    Application &m_application;
    std::vector<std::unique_ptr<Panel>> m_panels;
    bool m_initialized = false;
    bool m_frameReady = false;
    bool m_menuVisible = false;
    bool m_chordLatched = false;
};

} // namespace yamipan::ui
