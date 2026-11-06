// The frame through the ripple sheet (game/ripplesheet.cpp, RippleSheet::draw).
#pragma once

#include <SDL3/SDL_gpu.h>

#include "yamipan/gpu/device.h"
#include "yamipan/rendering/frame.h"

namespace yamipan::rendering
{

class FrameRipplePass
{
public:
    FrameRipplePass(gpu::Device &device, SDL_GPUTextureFormat format);

    FrameRipplePass(const FrameRipplePass &) = delete;
    FrameRipplePass &operator=(const FrameRipplePass &) = delete;

    // Outside any render pass. `vertices` is the frame's UI buffer; the draw's run of it is the sheet.
    void record(SDL_GPUCommandBuffer *commands, SDL_GPUTexture *scene, SDL_GPUBuffer *vertices, const DrawCall &draw,
                unsigned width, unsigned height);

private:
    void ensureCopy(unsigned width, unsigned height);

    gpu::Device &m_device;
    SDL_GPUTextureFormat m_format;
    gpu::Shader m_vertexShader;
    gpu::Shader m_fragmentShader;
    gpu::GraphicsPipeline m_pipeline;
    gpu::Sampler m_sampler;
    gpu::Texture m_copy; // the frame before the sheet, the render size
    unsigned m_copyWidth = 0;
    unsigned m_copyHeight = 0;
};

} // namespace yamipan::rendering
