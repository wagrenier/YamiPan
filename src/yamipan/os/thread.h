// A named host thread with a chosen stack size, which std::thread cannot give. The name is what gdb and
// the tools look for (YamiPanGame); the stack matters because the game's frames are not small.
#pragma once

#include <cstddef>
#include <functional>
#include <memory>
#include <string>

struct SDL_Thread;

namespace yamipan::os
{

class Thread
{
public:
    Thread(const std::string &name, std::size_t stackBytes, std::function<void()> body);
    ~Thread();

    Thread(const Thread &) = delete;
    Thread &operator=(const Thread &) = delete;

    // Waits for the body to end. The destructor joins a thread nobody joined or detached.
    void join();

    // Lets the thread run on alone, for the EE threads the process simply ends over.
    void detach();

private:
    struct Start;

    SDL_Thread *m_thread = nullptr;
};

} // namespace yamipan::os
