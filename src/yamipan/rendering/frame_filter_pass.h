// The game's frame filter (game/framefilter.cpp, gsFrameFilter): the frame shrunk, blurred and grown back
// in sprites between buffers. The ROM's buffers are VRAM blocks past the frame; here each block is a
// scratch texture at the frame's render size, so the blur goes with the picture at any size or aspect.
#pragma once

#include <map>

#include <SDL3/SDL_gpu.h>

#include "yamipan/gpu/device.h"
#include "yamipan/rendering/frame.h"

namespace yamipan::rendering
{

class FrameFilterPass
{
public:
    FrameFilterPass(gpu::Device &device, SDL_GPUTextureFormat format);

    FrameFilterPass(const FrameFilterPass &) = delete;
    FrameFilterPass &operator=(const FrameFilterPass &) = delete;

    // Outside any render pass: one render pass per step, the scene target standing for the frame block.
    void record(SDL_GPUCommandBuffer *commands, SDL_GPUTexture *scene, const FrameFilter &filter, unsigned width,
                unsigned height);

private:
    SDL_GPUTexture *bufferOf(int block, const FrameFilter &filter, SDL_GPUTexture *scene);

    gpu::Device &m_device;
    SDL_GPUTextureFormat m_format;
    gpu::Shader m_vertexShader;
    gpu::Shader m_fragmentShader;
    gpu::GraphicsPipeline m_pipeline;
    gpu::Sampler m_sampler;
    std::map<int, gpu::Texture> m_scratch; // by the game's block
    unsigned m_scratchWidth = 0;
    unsigned m_scratchHeight = 0;
};

} // namespace yamipan::rendering
