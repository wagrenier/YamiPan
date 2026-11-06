#include "yamipan/rendering/frame_palette_pass.h"

#include <algorithm>
#include <array>

namespace yamipan::rendering
{
namespace
{

SDL_GPUSamplerCreateInfo clampedSampler(SDL_GPUFilter filter)
{
    SDL_GPUSamplerCreateInfo info{};
    info.min_filter = filter;
    info.mag_filter = filter;
    info.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    info.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    return info;
}

} // namespace

FramePalettePass::FramePalettePass(gpu::Device &device, SDL_GPUTextureFormat format)
    : m_device(device), m_format(format)
{
    m_vertexShader = m_device.loadShader("present.vert", SDL_GPU_SHADERSTAGE_VERTEX, {});
    m_fragmentShader = m_device.loadShader("frame_palette.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, {.samplers = 2});
    m_frameSampler = m_device.createSampler(clampedSampler(SDL_GPU_FILTER_LINEAR));
    m_rampSampler = m_device.createSampler(clampedSampler(SDL_GPU_FILTER_NEAREST));

    SDL_GPUColorTargetDescription target{};
    target.format = m_format;

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
        gpu::logGpuError("SDL_CreateGPUGraphicsPipeline(frame palette)");
    m_pipeline = gpu::GraphicsPipeline(m_device.get(), pipeline);
}

void FramePalettePass::ensureTarget(unsigned width, unsigned height)
{
    if (m_half && m_halfWidth == width && m_halfHeight == height)
        return;
    m_half = m_device.createTexture(width, height, m_format,
                                    SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
    m_halfWidth = width;
    m_halfHeight = height;
}

void FramePalettePass::record(SDL_GPUCommandBuffer *commands, SDL_GPUTexture *scene, SDL_GPUTexture *ramp,
                              unsigned width, unsigned height)
{
    if (!m_pipeline)
        return;

    // Half of whatever the frame renders at, as the ROM's is half of its own: the softness goes with the
    // picture and the shape stays any aspect's.
    const unsigned halfWidth = std::max(width / 2, 1u);
    const unsigned halfHeight = std::max(height / 2, 1u);
    ensureTarget(halfWidth, halfHeight);

    SDL_GPUColorTargetInfo target{};
    target.texture = m_half.get();
    target.load_op = SDL_GPU_LOADOP_DONT_CARE;
    target.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass *const pass = SDL_BeginGPURenderPass(commands, &target, 1, nullptr);
    if (pass == nullptr)
    {
        gpu::logGpuError("SDL_BeginGPURenderPass(frame palette)");
        return;
    }

    SDL_BindGPUGraphicsPipeline(pass, m_pipeline.get());
    const std::array<SDL_GPUTextureSamplerBinding, 2> bindings{{
        {scene, m_frameSampler.get()},
        {ramp, m_rampSampler.get()},
    }};
    SDL_BindGPUFragmentSamplers(pass, 0, bindings.data(), static_cast<Uint32>(bindings.size()));
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    SDL_EndGPURenderPass(pass);

    SDL_GPUBlitInfo blit{};
    blit.source.texture = m_half.get();
    blit.source.w = halfWidth;
    blit.source.h = halfHeight;
    blit.destination.texture = scene;
    blit.destination.w = width;
    blit.destination.h = height;
    blit.load_op = SDL_GPU_LOADOP_DONT_CARE;
    blit.filter = SDL_GPU_FILTER_LINEAR;
    SDL_BlitGPUTexture(commands, &blit);
}

} // namespace yamipan::rendering
