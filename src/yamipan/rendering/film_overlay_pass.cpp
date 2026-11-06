#include "yamipan/rendering/film_overlay_pass.h"

#include <array>

namespace yamipan::rendering
{
namespace
{

// Mirrors FilmUniforms in film_overlay.frag.hlsl.
struct FilmUniforms
{
    float color[4];
    float band[4];
};

float channel(std::uint32_t color, int shift)
{
    return static_cast<float>((color >> shift) & 0xff) / 255.0f;
}

} // namespace

FilmOverlayPass::FilmOverlayPass(gpu::Device &device, SDL_GPUTextureFormat format) : m_device(device)
{
    m_vertexShader = m_device.loadShader("present.vert", SDL_GPU_SHADERSTAGE_VERTEX, {});
    m_fragmentShader = m_device.loadShader("film_overlay.frag", SDL_GPU_SHADERSTAGE_FRAGMENT,
                                           {.samplers = 2, .uniformBuffers = 1});

    SDL_GPUSamplerCreateInfo sampler{};
    sampler.min_filter = SDL_GPU_FILTER_LINEAR;
    sampler.mag_filter = SDL_GPU_FILTER_LINEAR;
    sampler.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    m_sampler = m_device.createSampler(sampler);

    SDL_GPUColorTargetDescription target{};
    target.format = format;
    target.blend_state.enable_blend = true;
    target.blend_state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
    target.blend_state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
    target.blend_state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    target.blend_state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
    target.blend_state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    target.blend_state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;
    target.blend_state.enable_color_write_mask = true;
    target.blend_state.color_write_mask =
        SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G | SDL_GPU_COLORCOMPONENT_B;

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
        gpu::logGpuError("SDL_CreateGPUGraphicsPipeline(film overlay)");
    m_pipeline = gpu::GraphicsPipeline(m_device.get(), pipeline);
}

void FilmOverlayPass::record(SDL_GPUCommandBuffer *commands, SDL_GPUTexture *scene, const FilmOverlay &film,
                             unsigned width, unsigned height)
{
    if (!m_pipeline)
        return;

    SDL_GPUColorTargetInfo target{};
    target.texture = scene;
    target.load_op = SDL_GPU_LOADOP_LOAD;
    target.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass *const pass = SDL_BeginGPURenderPass(commands, &target, 1, nullptr);
    if (pass == nullptr)
    {
        gpu::logGpuError("SDL_BeginGPURenderPass(film overlay)");
        return;
    }
    SDL_BindGPUGraphicsPipeline(pass, m_pipeline.get());

    const int midX = static_cast<int>(width / 2);
    const int midY = static_cast<int>(height / 2);
    const std::array<SDL_Rect, 4> quarters{{
        {0, 0, midX, midY},
        {midX, 0, static_cast<int>(width) - midX, midY},
        {0, midY, midX, static_cast<int>(height) - midY},
        {midX, midY, static_cast<int>(width) - midX, static_cast<int>(height) - midY},
    }};

    for (std::size_t index = 0; index < quarters.size(); index++)
    {
        const FilmCorner &corner = film.corners[index];
        if (corner.shown == nullptr || corner.leaving == nullptr || corner.shown->texture == nullptr ||
            corner.leaving->texture == nullptr)
            continue;

        const std::array<SDL_GPUTextureSamplerBinding, 2> bindings{{
            {corner.shown->texture->get(), m_sampler.get()},
            {corner.leaving->texture->get(), m_sampler.get()},
        }};
        SDL_BindGPUFragmentSamplers(pass, 0, bindings.data(), static_cast<Uint32>(bindings.size()));

        const FilmUniforms uniforms{
            {channel(film.color, 0), channel(film.color, 8), channel(film.color, 16), corner.leavingAlpha},
            {static_cast<float>(film.bandStart), static_cast<float>(film.bandLength),
             static_cast<float>(film.clearBelow), static_cast<float>(film.solidAbove)},
        };
        SDL_PushGPUFragmentUniformData(commands, 0, &uniforms, sizeof(uniforms));

        SDL_SetGPUScissor(pass, &quarters[index]);
        SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    }
    SDL_EndGPURenderPass(pass);
}

} // namespace yamipan::rendering
