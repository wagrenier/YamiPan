#include "yamipan/rendering/frame_filter_pass.h"

#include <algorithm>
#include <cmath>

namespace yamipan::rendering
{
namespace
{

constexpr float kFrameUnits = 512.0f; // the game's frame, any render size
constexpr float kGsOne = 128.0f;      // alpha 0x80 is 1.0

// Mirrors FilterUniforms in frame_filter.frag.hlsl.
struct FilterUniforms
{
    float area[4]; // xy: the source's shown part over the whole texture; z: the step's alpha
};

} // namespace

FrameFilterPass::FrameFilterPass(gpu::Device &device, SDL_GPUTextureFormat format)
    : m_device(device), m_format(format)
{
    m_vertexShader = m_device.loadShader("present.vert", SDL_GPU_SHADERSTAGE_VERTEX, {});
    m_fragmentShader =
        m_device.loadShader("frame_filter.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, {.samplers = 1, .uniformBuffers = 1});

    SDL_GPUSamplerCreateInfo sampler{};
    sampler.min_filter = SDL_GPU_FILTER_LINEAR;
    sampler.mag_filter = SDL_GPU_FILTER_LINEAR;
    sampler.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    m_sampler = m_device.createSampler(sampler);

    // ALPHA_1 0x44: (Cs - Cd) * As + Cd; the GS writes As itself into the target's alpha.
    SDL_GPUColorTargetDescription target{};
    target.format = m_format;
    target.blend_state.enable_blend = true;
    target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    target.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    target.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
    target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

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
        gpu::logGpuError("SDL_CreateGPUGraphicsPipeline(frame filter)");
    m_pipeline = gpu::GraphicsPipeline(m_device.get(), pipeline);
}

SDL_GPUTexture *FrameFilterPass::bufferOf(int block, const FrameFilter &filter, SDL_GPUTexture *scene)
{
    if (block == filter.frameBlock)
        return scene;
    gpu::Texture &texture = m_scratch[block];
    if (!texture)
        texture = m_device.createTexture(m_scratchWidth, m_scratchHeight, m_format,
                                         SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
    return texture.get();
}

void FrameFilterPass::record(SDL_GPUCommandBuffer *commands, SDL_GPUTexture *scene, const FrameFilter &filter,
                             unsigned width, unsigned height)
{
    if (!m_pipeline || filter.steps.empty())
        return;
    if (m_scratchWidth != width || m_scratchHeight != height)
    {
        m_scratch.clear();
        m_scratchWidth = width;
        m_scratchHeight = height;
    }

    const float scaleX = static_cast<float>(width) / kFrameUnits;
    const float scaleY = static_cast<float>(height) / kFrameUnits;
    for (const FrameFilterStep &step : filter.steps)
    {
        SDL_GPUTexture *const source = bufferOf(step.source, filter, scene);
        SDL_GPUTexture *const target = bufferOf(step.target, filter, scene);
        // The ROM never reads the buffer it draws; a host texture cannot be both either.
        if (source == nullptr || target == nullptr || source == target)
            continue;

        SDL_GPUColorTargetInfo targetInfo{};
        targetInfo.texture = target;
        targetInfo.load_op = SDL_GPU_LOADOP_LOAD;
        targetInfo.store_op = SDL_GPU_STOREOP_STORE;

        SDL_GPURenderPass *const pass = SDL_BeginGPURenderPass(commands, &targetInfo, 1, nullptr);
        if (pass == nullptr)
        {
            gpu::logGpuError("SDL_BeginGPURenderPass(frame filter)");
            return;
        }

        // The sprite where the step puts it; the scissor keeps what lands past 0..width x 0..height out.
        const SDL_GPUViewport viewport{static_cast<float>(step.x) * scaleX, static_cast<float>(step.y) * scaleY,
                                       static_cast<float>(step.width) * scaleX,
                                       static_cast<float>(step.height) * scaleY, 0.0f, 1.0f};
        SDL_SetGPUViewport(pass, &viewport);
        const SDL_Rect scissor{0, 0,
                               std::clamp(static_cast<int>(std::lround(step.width * scaleX)), 0,
                                          static_cast<int>(width)),
                               std::clamp(static_cast<int>(std::lround(step.height * scaleY)), 0,
                                          static_cast<int>(height))};
        SDL_SetGPUScissor(pass, &scissor);

        SDL_BindGPUGraphicsPipeline(pass, m_pipeline.get());
        const SDL_GPUTextureSamplerBinding binding{source, m_sampler.get()};
        SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);
        const FilterUniforms uniforms{{static_cast<float>(step.sourceWidth) / kFrameUnits,
                                       static_cast<float>(step.sourceHeight) / kFrameUnits,
                                       static_cast<float>(step.alpha) / kGsOne, 0.0f}};
        SDL_PushGPUFragmentUniformData(commands, 0, &uniforms, sizeof(uniforms));
        SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
        SDL_EndGPURenderPass(pass);
    }
}

} // namespace yamipan::rendering
