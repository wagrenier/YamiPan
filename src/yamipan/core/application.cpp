#include "yamipan/core/application.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_timer.h>

#include "yamipan/core/logger.h"
#include "yamipan/ee/kernel.h"

// The game's own main (game/gamesystem.cpp, built with main=gameMain).
int gameMain(int argc, char **argv);

namespace yamipan
{
namespace
{

Application *s_instance = nullptr;

// Long enough for the render thread to finish its frame, short enough that a game stuck in a wait cannot
// keep a closed window's process alive.
constexpr int kPumpTimeoutMs = 50;

} // namespace

Application &Application::instance()
{
    return *s_instance;
}

Application::Application(std::span<char *> arguments) : m_arguments(arguments)
{
    s_instance = this;

    // A smoke run reads stdout through a pipe and is killed at its end: nothing may wait in a buffer.
    std::setvbuf(stdout, nullptr, _IONBF, 0);

    log::open(m_paths.logFile());
    m_config.load(m_paths.configFile());
    m_paths.discoverDataFolder(m_config.paths.dataFolder);
}

Application::~Application()
{
    s_instance = nullptr;
}

bool Application::startServices()
{
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMEPAD | SDL_INIT_AUDIO))
    {
        log::error("SDL_Init: {}", SDL_GetError());
        return false;
    }

    auto window = Window::create(m_config.display);
    if (!window)
    {
        log::error("{}", window.error());
        return false;
    }
    m_window.emplace(std::move(*window));

    auto device = gpu::Device::create(m_paths);
    if (!device)
    {
        log::error("{}", device.error());
        return false;
    }
    m_device = std::move(*device);
    if (!SDL_ClaimWindowForGPUDevice(m_device->get(), m_window->get()))
    {
        log::error("SDL_ClaimWindowForGPUDevice: {}", SDL_GetError());
        return false;
    }

    m_input = std::make_unique<io::InputSystem>();
    m_input->loadBindings(m_config.ini());

    m_overlay = std::make_unique<ui::Overlay>(*this);
    m_renderer = std::make_unique<rendering::Renderer>(*m_window, *m_device, m_config, m_events, *m_overlay);
    return true;
}

void Application::startGame()
{
    if (m_gameStarted.exchange(true))
        return;

    log::info("Starting the game");
    ee::Kernel::instance().startMainThread([this] {
        std::vector<char *> arguments(m_arguments.begin(), m_arguments.end());
        const int result = gameMain(static_cast<int>(arguments.size()), arguments.data());
        log::info("The game's main returned {}", result);
    });
}

void Application::dataFolderChosen(const std::filesystem::path &folder)
{
    if (m_paths.setDataFolder(folder))
        startGame();
}

void Application::pumpEvents()
{
    SDL_Event event;
    if (!SDL_WaitEventTimeout(&event, kPumpTimeoutMs))
        return;

    do
    {
        switch (event.type)
        {
        case SDL_EVENT_QUIT:
            m_running = false;
            break;
        case SDL_EVENT_WINDOW_RESIZED:
            m_window->noteResized();
            break;
        default:
            break;
        }
        m_input->handleEvent(event);
        m_events.push(event);
    } while (SDL_PollEvent(&event));
}

void Application::quit()
{
    log::info("Quitting");

    const auto [width, height] = m_window->windowedSize();
    m_config.display.windowWidth = width;
    m_config.display.windowHeight = height;
    m_config.display.windowMode = m_window->mode();
    m_input->saveBindings(m_config.ini());
    m_config.save();

    m_renderer->stop();

    // EE threads are parked inside the game's call chains with no way out: the console never tore
    // them down either. The process simply ends over them.
    std::_Exit(0);
}

int Application::run()
{
    if (!startServices())
        return EXIT_FAILURE;

    m_renderer->start();
    if (m_paths.hasDataFolder())
        startGame();

    while (m_running)
        pumpEvents();

    quit();
    return EXIT_SUCCESS;
}

} // namespace yamipan
