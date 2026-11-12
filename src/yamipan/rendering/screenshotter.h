// Smoke-run shots, how reconstructed code is checked by eye: with YAMIPAN_SHOT_DIR set, every
// YAMIPAN_SHOT_EVERY-th present saves the composed picture, before the overlay, as shot_<frame>.png and
// last.png. The tools kill the process at the end of a run, so a shot has to be on disk as it happens.
// YAMIPAN_SHOT_WINDOW=1 takes the whole window instead, bars and overlay included, for host work.
#pragma once

#include <filesystem>
#include <optional>

#include <SDL3/SDL_gpu.h>

#include "yamipan/gpu/device.h"
#include "yamipan/rendering/display_geometry.h"

namespace yamipan::rendering
{

class Screenshotter
{
public:
    explicit Screenshotter(gpu::Device &device);

    // Copies the scene for a shot when one is due. Answers whether write() has something to do.
    bool recordIfDue(SDL_GPUCommandBuffer *commands, SDL_GPUTexture *scene, SDL_GPUTexture *window,
                     const PictureLayout &layout);

    // After the frame's commands are submitted.
    void write();

private:
    gpu::Device &m_device;
    std::optional<std::filesystem::path> m_directory;
    unsigned m_every = 120;
    unsigned m_presented = 0;
    bool m_wholeWindow = false;
    gpu::Texture m_texture;
    unsigned m_width = 0;
    unsigned m_height = 0;
};

} // namespace yamipan::rendering
