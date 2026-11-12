#include "yamipan/rendering/screenshotter.h"

#include <cmath>
#include <cstdlib>
#include <cstring>
#include <format>
#include <vector>

#include <SDL3/SDL_surface.h>

#include "yamipan/core/logger.h"
#include "yamipan/core/paths.h"

namespace yamipan::rendering
{
namespace
{

constexpr SDL_GPUTextureFormat kShotFormat = SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM;
constexpr unsigned kBytesPerPixel = 4;

} // namespace

Screenshotter::Screenshotter(gpu::Device &device) : m_device(device)
{
    if (const char *const directory = std::getenv("YAMIPAN_SHOT_DIR"); directory != nullptr && directory[0] != '\0')
        m_directory = pathFromUtf8(directory);
    if (const char *const every = std::getenv("YAMIPAN_SHOT_EVERY"); every != nullptr && std::atoi(every) > 0)
        m_every = static_cast<unsigned>(std::atoi(every));
    m_wholeWindow = std::getenv("YAMIPAN_SHOT_WINDOW") != nullptr;
}

bool Screenshotter::recordIfDue(SDL_GPUCommandBuffer *commands, SDL_GPUTexture *scene, SDL_GPUTexture *window,
                                const PictureLayout &layout)
{
    if (!m_directory || ++m_presented % m_every != 0)
        return false;

    // At the frame's own height, the composed shape: a size the tools can look at, whatever the window.
    const auto height = static_cast<unsigned>(m_wholeWindow ? layout.windowHeight : static_cast<int>(kFrameHeight));
    const auto width = m_wholeWindow ? static_cast<unsigned>(layout.windowWidth)
                                     : static_cast<unsigned>(std::lround(kFrameHeight * layout.aspect));
    if (!m_texture || m_width != width || m_height != height)
    {
        m_texture = m_device.createTexture(width, height, kShotFormat,
                                           SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
        m_width = width;
        m_height = height;
    }

    SDL_GPUBlitInfo blit{};
    blit.source.texture = m_wholeWindow ? window : scene;
    blit.source.w = static_cast<Uint32>(m_wholeWindow ? layout.windowWidth : layout.renderWidth);
    blit.source.h = static_cast<Uint32>(m_wholeWindow ? layout.windowHeight : layout.renderHeight);
    blit.destination.texture = m_texture.get();
    blit.destination.w = width;
    blit.destination.h = height;
    blit.load_op = SDL_GPU_LOADOP_DONT_CARE;
    blit.filter = SDL_GPU_FILTER_LINEAR;
    SDL_BlitGPUTexture(commands, &blit);
    return true;
}

void Screenshotter::write()
{
    const unsigned bytes = m_width * m_height * kBytesPerPixel;
    gpu::TransferBuffer download = m_device.createDownloadBuffer(bytes);
    if (!download)
        return;

    SDL_GPUCommandBuffer *const commands = SDL_AcquireGPUCommandBuffer(m_device.get());
    if (commands == nullptr)
        return;
    SDL_GPUCopyPass *const copy = SDL_BeginGPUCopyPass(commands);

    SDL_GPUTextureRegion region{};
    region.texture = m_texture.get();
    region.w = m_width;
    region.h = m_height;
    region.d = 1;

    SDL_GPUTextureTransferInfo transfer{};
    transfer.transfer_buffer = download.get();
    transfer.pixels_per_row = m_width;
    transfer.rows_per_layer = m_height;

    SDL_DownloadFromGPUTexture(copy, &region, &transfer);
    SDL_EndGPUCopyPass(copy);

    SDL_GPUFence *const fence = SDL_SubmitGPUCommandBufferAndAcquireFence(commands);
    if (fence == nullptr)
        return;
    SDL_WaitForGPUFences(m_device.get(), true, &fence, 1);
    SDL_ReleaseGPUFence(m_device.get(), fence);

    const auto *const mapped = static_cast<const unsigned char *>(SDL_MapGPUTransferBuffer(m_device.get(), download.get(), false));
    if (mapped == nullptr)
        return;
    std::vector<unsigned char> pixels(mapped, mapped + bytes);
    SDL_UnmapGPUTransferBuffer(m_device.get(), download.get());

    // The frame carries the PS2's alpha; a screenshot is opaque.
    for (std::size_t index = 3; index < pixels.size(); index += kBytesPerPixel)
        pixels[index] = 0xff;

    SDL_Surface *const surface = SDL_CreateSurfaceFrom(static_cast<int>(m_width), static_cast<int>(m_height),
                                                       SDL_PIXELFORMAT_RGBA32, pixels.data(),
                                                       static_cast<int>(m_width * kBytesPerPixel));
    if (surface == nullptr)
        return;

    const auto named = *m_directory / std::format("shot_{:05}.png", m_presented);
    const auto latest = *m_directory / "last.png";
    if (!SDL_SavePNG(surface, pathToUtf8(named).c_str()) || !SDL_SavePNG(surface, pathToUtf8(latest).c_str()))
        gpu::logGpuError("SDL_SavePNG");
    SDL_DestroySurface(surface);
}

} // namespace yamipan::rendering
