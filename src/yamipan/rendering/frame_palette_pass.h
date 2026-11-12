// A whole-frame pass that replaces the picture: the frame so far through a 256-colour ramp, by its
// brightness. The game's views of the past and of a few characters (game/stageparty.cpp).
#pragma once

#include <SDL3/SDL_gpu.h>

#include "yamipan/gpu/device.h"

namespace yamipan::rendering
{

class FramePalettePass
{
public:
    FramePalettePass(gpu::Device &device, SDL_GPUTextureFormat format);

    FramePalettePass(const FramePalettePass &) = delete;
    FramePalettePass &operator=(const FramePalettePass &) = delete;

    // Outside any render pass: the scene target is read, then written.
    void record(SDL_GPUCommandBuffer *commands, SDL_GPUTexture *scene, SDL_GPUTexture *ramp, unsigned width,
                unsigned height);

private:
    void ensureTarget(unsigned width, unsigned height);

    gpu::Device &m_device;
    SDL_GPUTextureFormat m_format;
    gpu::Shader m_vertexShader;
    gpu::Shader m_fragmentShader;
    gpu::GraphicsPipeline m_pipeline;
    gpu::Sampler m_frameSampler;
    gpu::Sampler m_rampSampler;
    gpu::Texture m_half;
    unsigned m_halfWidth = 0;
    unsigned m_halfHeight = 0;
};

} // namespace yamipan::rendering
