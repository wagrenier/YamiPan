// The port's one output transform: the finished SDR picture, graded, in whatever encoding the swapchain
// runs. With the grade at identity and no HDR it is a plain copy and costs nothing.
#pragma once

#include <SDL3/SDL_gpu.h>

#include "yamipan/core/config.h"
#include "yamipan/gpu/device.h"

namespace yamipan
{
class Window;
}

namespace yamipan::rendering
{

class PresentPass
{
public:
    PresentPass(gpu::Device &device, Window &window, const Config &config);

    void record(SDL_GPUCommandBuffer *commands, SDL_GPUTexture *source, SDL_GPUTexture *swapchain, unsigned width,
                unsigned height);

private:
    bool gradeIsIdentity() const;
    SDL_GPUGraphicsPipeline *pipelineFor(SDL_GPUTextureFormat format);

    gpu::Device &m_device;
    Window &m_window;
    const Config &m_config;
    gpu::Shader m_vertexShader;
    gpu::Shader m_fragmentShader;
    gpu::GraphicsPipeline m_pipeline;
    SDL_GPUTextureFormat m_pipelineFormat = SDL_GPU_TEXTUREFORMAT_INVALID;
    gpu::Sampler m_sampler;
};

} // namespace yamipan::rendering
