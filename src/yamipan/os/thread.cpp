#include "yamipan/os/thread.h"

#include <stdexcept>
#include <utility>

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_thread.h>

#include "yamipan/core/crash_report.h"

namespace yamipan::os
{

struct Thread::Start
{
    std::function<void()> body;

    static int SDLCALL run(void *data)
    {
        const std::unique_ptr<Start> start(static_cast<Start *>(data));
        crash::installThread(); // the fault handlers are per thread under Windows' CRT
        start->body();
        return 0;
    }
};

Thread::Thread(const std::string &name, std::size_t stackBytes, std::function<void()> body)
{
    auto start = std::make_unique<Start>(std::move(body));

    const SDL_PropertiesID properties = SDL_CreateProperties();
    SDL_SetPointerProperty(properties, SDL_PROP_THREAD_CREATE_ENTRY_FUNCTION_POINTER,
                           reinterpret_cast<void *>(&Start::run));
    SDL_SetStringProperty(properties, SDL_PROP_THREAD_CREATE_NAME_STRING, name.c_str());
    SDL_SetNumberProperty(properties, SDL_PROP_THREAD_CREATE_STACKSIZE_NUMBER,
                          static_cast<Sint64>(stackBytes));
    SDL_SetPointerProperty(properties, SDL_PROP_THREAD_CREATE_USERDATA_POINTER, start.get());
    m_thread = SDL_CreateThreadWithProperties(properties);
    SDL_DestroyProperties(properties);

    if (m_thread == nullptr)
        throw std::runtime_error(SDL_GetError());
    start.release(); // the thread owns it now
}

Thread::~Thread()
{
    join();
}

void Thread::join()
{
    if (m_thread != nullptr)
        SDL_WaitThread(std::exchange(m_thread, nullptr), nullptr);
}

void Thread::detach()
{
    if (m_thread != nullptr)
        SDL_DetachThread(std::exchange(m_thread, nullptr));
}

} // namespace yamipan::os
