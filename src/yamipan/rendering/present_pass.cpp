#include "yamipan/rendering/present_pass.h"

#include "yamipan/core/window.h"

namespace yamipan::rendering
{
namespace
{

// Mirrors PresentUniforms in present.frag.hlsl.
struct PresentUniforms
{
    float grade[4];
    float hdr[4];
    float output[4];
    float target[4];
};

constexpr float kOutputSdr = 0.0f;
constexpr float kDitherStep = 1.0f / 255.0f;
constexpr float kExpansionShoulder = 2.0f;

} // namespace

PresentPass::PresentPass(gpu::Device &device, Window &window, const Config &config)
    : m_device(device), m_window(window), m_config(config)
{
    m_vertexShader = m_device.loadShader("present.vert", SDL_GPU_SHADERSTAGE_VERTEX, {});
    m_fragmentShader =
        m_device.loadShader("present.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, {.samplers = 1, .uniformBuffers = 1});

    SDL_GPUSamplerCreateInfo info{};
    info.min_filter = SDL_GPU_FILTER_NEAREST;
    info.mag_filter = SDL_GPU_FILTER_NEAREST;
    info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    m_sampler = m_device.createSampler(info);
}

bool PresentPass::gradeIsIdentity() const
{
    const DisplaySettings &display = m_config.display;
    return display.gradeBrightness == 1.0f && display.gradeContrast == 1.0f && display.gradeGamma == 1.0f &&
           display.gradeSaturation == 1.0f;
}

SDL_GPUGraphicsPipeline *PresentPass::pipelineFor(SDL_GPUTextureFormat format)
{
    if (m_pipeline && m_pipelineFormat == format)
        return m_pipeline.get();

    SDL_GPUColorTargetDescription target{};
    target.format = format;

    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = m_vertexShader.get();
    info.fragment_shader = m_fragmentShader.get();
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    info.target_info.color_target_descriptions = &target;
    info.target_info.num_color_targets = 1;

    SDL_GPUGraphicsPipeline *const pipeline = SDL_CreateGPUGraphicsPipeline(m_device.get(), &info);
    if (pipeline == nullptr)
        gpu::logGpuError("SDL_CreateGPUGraphicsPipeline(present)");
    m_pipeline = gpu::GraphicsPipeline(m_device.get(), pipeline);
    m_pipelineFormat = format;
    return pipeline;
}

void PresentPass::record(SDL_GPUCommandBuffer *commands, SDL_GPUTexture *source, SDL_GPUTexture *swapchain,
                         unsigned width, unsigned height)
{
    if (gradeIsIdentity())
    {
        SDL_GPUBlitInfo blit{};
        blit.source.texture = source;
        blit.source.w = width;
        blit.source.h = height;
        blit.destination.texture = swapchain;
        blit.destination.w = width;
        blit.destination.h = height;
        blit.load_op = SDL_GPU_LOADOP_DONT_CARE;
        blit.filter = SDL_GPU_FILTER_NEAREST;
        SDL_BlitGPUTexture(commands, &blit);
        return;
    }

    SDL_GPUGraphicsPipeline *const pipeline =
        pipelineFor(SDL_GetGPUSwapchainTextureFormat(m_device.get(), m_window.get()));
    if (pipeline == nullptr)
        return;

    SDL_GPUColorTargetInfo target{};
    target.texture = swapchain;
    target.load_op = SDL_GPU_LOADOP_DONT_CARE;
    target.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass *const pass = SDL_BeginGPURenderPass(commands, &target, 1, nullptr);
    if (pass == nullptr)
        return;

    const DisplaySettings &display = m_config.display;
    const PresentUniforms uniforms{
        {display.gradeBrightness, display.gradeContrast, display.gradeGamma, display.gradeSaturation},
        {1.0f, 1.0f, 0.0f, 0.0f},
        {kOutputSdr, kDitherStep, kExpansionShoulder, 0.0f},
        {static_cast<float>(width), static_cast<float>(height), 1.0f / static_cast<float>(width),
         1.0f / static_cast<float>(height)},
    };

    SDL_BindGPUGraphicsPipeline(pass, pipeline);
    SDL_PushGPUFragmentUniformData(commands, 0, &uniforms, sizeof(uniforms));

    SDL_GPUTextureSamplerBinding binding{};
    binding.texture = source;
    binding.sampler = m_sampler.get();
    SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);

    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    SDL_EndGPURenderPass(pass);
}

} // namespace yamipan::rendering
