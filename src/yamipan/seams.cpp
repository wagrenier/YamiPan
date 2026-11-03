// The seams game code calls (cpp-port.md "Scope"): thin forwarders, so the game's side never changes.
#include <chrono>
#include <memory>
#include <span>
#include <thread>

#include "rena/rena_2d_texture_tim2.h"
#include "yamipan/core/application.h"
#include "yamipan/os/yamipan_time.h"
#include "yamipan/rendering/renderer.h"
#include "yamipan/rendering/ui_canvas.h"
#include "yamipan/rendering/yamipan_drawui.h"
#include "yamipan/yamipan_memory.h"

namespace
{

yamipan::rendering::UiCanvas &canvas()
{
    return yamipan::Application::instance().renderer().canvas();
}

} // namespace

void YamiPan_Sleep(unsigned int milliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

void YamiPan_SleepNs(uint64_t nanoseconds)
{
    std::this_thread::sleep_for(std::chrono::nanoseconds(nanoseconds));
}

void *YamiPan_GetHostPointer(uintptr_t)
{
    return nullptr; // no emulated EE memory
}

void YamiPan_DrawUiReg(const void *list, int reg, uint64_t value)
{
    canvas().setRegister(list, reg, value);
}

void YamiPan_DrawUiPrim(const void *list, uint64_t prim, const uint8_t *rgbaq)
{
    canvas().setPrimitive(list, prim, rgbaq);
}

void YamiPan_DrawUiTexture(const void *list, const void *owner)
{
    if (owner == nullptr)
        return; // the bound one stays

    // Every owner the game hands over is a Tim2Texture: a GsTexture (the drawer) or the font's own.
    yamipan::rendering::Renderer &renderer = yamipan::Application::instance().renderer();
    renderer.canvas().bindImage(list, renderer.textures().imageOf(*static_cast<const Tim2Texture *>(owner)));
}

void YamiPan_DrawUiBegin(const void *list, int format)
{
    canvas().begin(list, format);
}

void YamiPan_DrawUiVertex(const void *list, const YamiPanUiVertex *vertex)
{
    canvas().addVertex(list, *vertex);
}

void YamiPan_DrawUiMovie(const void *, const void *, const void *, int, int, int, int, int, int)
{
    // The movie path arrives with libmpeg.
}

void YamiPan_DrawUiKick(const void *const *lists, int count)
{
    canvas().kick(std::span(lists, static_cast<std::size_t>(count)));
}
