// The sight-jack's film over the frame so far (game/stageparty.cpp, JackFilm::draw).
#pragma once

#include <SDL3/SDL_gpu.h>

#include "yamipan/gpu/device.h"
#include "yamipan/rendering/frame.h"

namespace yamipan::rendering
{

class FilmOverlayPass
{
public:
    FilmOverlayPass(gpu::Device &device, SDL_GPUTextureFormat format);

    FilmOverlayPass(const FilmOverlayPass &) = delete;
    FilmOverlayPass &operator=(const FilmOverlayPass &) = delete;

    // Outside any render pass.
    void record(SDL_GPUCommandBuffer *commands, SDL_GPUTexture *scene, const FilmOverlay &film, unsigned width,
                unsigned height);

private:
    gpu::Device &m_device;
    gpu::Shader m_vertexShader;
    gpu::Shader m_fragmentShader;
    gpu::GraphicsPipeline m_pipeline;
    gpu::Sampler m_sampler;
};

} // namespace yamipan::rendering
