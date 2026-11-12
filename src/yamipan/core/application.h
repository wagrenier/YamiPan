// The port's root object: owns the Pan layer's services and runs the process. The main thread stays here,
// pumping the window; the game and the renderer have threads of their own (cpp-port.md "Threads").
// instance() is the one service locator, for the edge where game code, which cannot be given references,
// reaches the host. Host classes take what they need in their constructors.
#pragma once

#include <atomic>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>

#include "yamipan/core/config.h"
#include "yamipan/core/event_queue.h"
#include "yamipan/core/paths.h"
#include "yamipan/core/window.h"
#include "yamipan/gpu/device.h"
#include "yamipan/io/input_system.h"
#include "yamipan/rendering/renderer.h"
#include "yamipan/ui/overlay.h"

namespace yamipan
{

class Application
{
public:
    explicit Application(std::span<char *> arguments);
    ~Application();

    Application(const Application &) = delete;
    Application &operator=(const Application &) = delete;

    static Application &instance();

    // Runs until the window closes. Answers the process's exit code.
    int run();

    Paths &paths() { return m_paths; }
    Config &config() { return m_config; }
    Window &window() { return *m_window; }
    gpu::Device &device() { return *m_device; }
    rendering::Renderer &renderer() { return *m_renderer; }
    io::InputSystem &input() { return *m_input; }

    // The data folder panel's answer: the game starts as soon as its files are known.
    void dataFolderChosen(const std::filesystem::path &folder);

    // The [audio] volumes onto the sound chip (iop::Spu2Voices): at the game's start and from the Audio menu.
    void applyAudioSettings();

private:
    bool startServices();
    void startGame();
    void pumpEvents();
    void quit();

    std::span<char *> m_arguments;
    Paths m_paths;
    Config m_config;
    EventQueue m_events;
    std::optional<Window> m_window;
    std::unique_ptr<gpu::Device> m_device;
    std::unique_ptr<io::InputSystem> m_input;
    std::unique_ptr<ui::Overlay> m_overlay;
    std::unique_ptr<rendering::Renderer> m_renderer;
    std::atomic<bool> m_gameStarted{false};
    bool m_running = true;
};

} // namespace yamipan
