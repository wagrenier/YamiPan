// The seams game code calls (cpp-port.md "Scope"): thin forwarders, so the game's side never changes.
#include <chrono>
#include <memory>
#include <span>

#include "game/movieplayer.h"
#include "rena/rena_2d_texture_tim2.h"
#include "yamipan/core/application.h"
#include "yamipan/ee/interrupts.h"
#include "yamipan/ee/kernel.h"
#include "yamipan/os/timed_waiter.h"
#include "yamipan/os/yamipan_time.h"
#include "yamipan/rendering/renderer.h"
#include "yamipan/rendering/ui_canvas.h"
#include "yamipan/rendering/yamipan_drawmodel.h"
#include "yamipan/rendering/yamipan_drawshadow.h"
#include "yamipan/rendering/yamipan_drawui.h"
#include "yamipan/yamipan_memory.h"

namespace
{

yamipan::rendering::UiCanvas &canvas()
{
    return yamipan::Application::instance().renderer().canvas();
}

yamipan::rendering::ModelRenderer &models()
{
    return yamipan::Application::instance().renderer().models();
}

} // namespace

void YamiPan_ModelBeginPass(const void *list, const YamiPanModelPass *pass)
{
    models().beginPass(list, *pass);
}

void YamiPan_ModelSetMaterial(const void *list, const YamiPanModelMaterial *material)
{
    models().setMaterial(list, *material);
}

void YamiPan_ModelSetLights(const void *list, const YamiPanModelLights *lights)
{
    models().setLights(list, *lights);
}

void YamiPan_ModelDrawBatch(const void *list, const float world[4][4], const YamiPanModelBatch *batch)
{
    models().drawBatch(list, world, *batch);
}

void YamiPan_ModelSetEffect(const void *list, const YamiPanModelEffect *effect)
{
    models().setEffect(list, *effect);
}

void YamiPan_ModelDrawQuad(const void *list, const YamiPanModelQuad *quad)
{
    models().drawQuad(list, *quad);
}

void YamiPan_ModelEndPass(const void *list)
{
    models().endPass(list);
}

void YamiPan_ModelForgetTextures()
{
    models().forgetTextures();
}

// Not std::this_thread::sleep_for: under MinGW a 1 ms one takes 15.6 ms (os/timed_waiter.h).
void YamiPan_Sleep(unsigned int milliseconds)
{
    yamipan::os::TimedWaiter::sleepFor(std::chrono::milliseconds(milliseconds));
}

void YamiPan_SleepNs(uint64_t nanoseconds)
{
    yamipan::os::TimedWaiter::sleepFor(std::chrono::nanoseconds(nanoseconds));
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

void YamiPan_DrawUiSqueeze(const void *list, int on)
{
    canvas().setSqueezed(list, on != 0);
}

float YamiPan_DrawUiSqueezeFactor(void)
{
    return canvas().squeezeFactor();
}

void YamiPan_DrawUiFrameScale(const void *list, float factor)
{
    canvas().scaleFrame(list, factor);
}

void YamiPan_DrawUiFrameContrast(const void *list, uint32_t rgba)
{
    canvas().contrastFrame(list, rgba);
}

void YamiPan_DrawUiFramePalette(const void *list, const uint32_t *ramp)
{
    canvas().paletteFrame(list, ramp);
}

void YamiPan_DrawUiFrameRipple(const void *list, const YamiPanRipple *ripple)
{
    canvas().rippleFrame(list, *ripple);
}

void YamiPan_DrawUiFrameFilterBegin(const void *list, int frameBlock)
{
    canvas().beginFrameFilter(list, frameBlock);
}

void YamiPan_DrawUiFrameFilterStep(const void *list, const YamiPanFrameFilterStep *step)
{
    canvas().addFrameFilterStep(list, {
        .source = step->source,
        .target = step->target,
        .x = step->x,
        .y = step->y,
        .width = step->width,
        .height = step->height,
        .sourceWidth = step->sourceWidth,
        .sourceHeight = step->sourceHeight,
        .alpha = step->alpha,
    });
}

void YamiPan_DrawUiFilm(const void *list, const YamiPanFilm *film)
{
    yamipan::rendering::Renderer &renderer = yamipan::Application::instance().renderer();
    const auto imageOf = [&renderer](const void *owner) {
        return renderer.textures().imageOf(*static_cast<const Tim2Texture *>(owner));
    };

    yamipan::rendering::FilmOverlay overlay;
    for (std::size_t index = 0; index < overlay.corners.size(); index++)
    {
        const YamiPanFilmCorner &corner = film->corners[index];
        overlay.corners[index] = {imageOf(corner.shown), imageOf(corner.leaving), corner.leavingAlpha};
    }
    overlay.bandStart = film->bandStart;
    overlay.bandLength = film->bandLength;
    overlay.clearBelow = film->clearBelow;
    overlay.solidAbove = film->solidAbove;
    overlay.color = film->color;
    renderer.canvas().filmOverlay(list, std::move(overlay));
}

void YamiPan_DrawUiBegin(const void *list, int format)
{
    canvas().begin(list, format);
}

void YamiPan_DrawUiVertex(const void *list, const YamiPanUiVertex *vertex)
{
    canvas().addVertex(list, *vertex);
}

void YamiPan_DrawUiMovie(const void *list, const void *owner, const void *macroblocks, int columns, int rows, int tbp,
                         int, int x, int y)
{
    // MoviePlayer::prime sends its first picture to the movie's own buffer, which nothing shows; only the
    // widget's draw (the frame's buffer) is seen.
    constexpr int kMovieFrameTbp = 0x800;
    if (tbp != kMovieFrameTbp)
        return;
    const int picture = static_cast<const MoviePlayer *>(owner)->m_mpeg.frameCount;
    canvas().drawMacroblocks(list, owner, picture, macroblocks, columns, rows, static_cast<float>(x),
                             static_cast<float>(y));
}

// The boss stage's shadow target: empty until the 3D host module exists, like
// the rena stubs that would draw into it.
void YamiPan_ShadowBegin(const void *, int)
{
}

void YamiPan_ShadowBlit(const void *, const YamiPanShadowBlit *)
{
}

void YamiPan_ShadowEnd(const void *, const unsigned char *)
{
}

void YamiPan_DrawUiKick(const void *const *lists, int count)
{
    // The frame is kicked right after the field that ended its wait (GameSystem::flipFrame).
    canvas().kick(std::span(lists, static_cast<std::size_t>(count)),
                  yamipan::ee::Kernel::instance().interrupts().lastFieldTime());
}
