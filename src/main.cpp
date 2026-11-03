// The process entry. The game's own main is gameMain (game/gamesystem.cpp), run on the EE's main thread.
#include <span>

#include <SDL3/SDL_main.h>

#include "yamipan/core/application.h"

int main(int argc, char **argv)
{
    yamipan::Application application(std::span(argv, static_cast<std::size_t>(argc)));
    return application.run();
}
