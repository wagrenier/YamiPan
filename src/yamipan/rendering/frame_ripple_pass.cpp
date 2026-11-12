#include "yamipan/rendering/frame_ripple_pass.h"

#include <array>
#include <cstddef>

namespace yamipan::rendering
{
namespace
{

constexpr float kFrameUnits = 512.0f;

// Mirrors UiVertexUniforms in ui.vert.hlsl.
struct VertexUniforms
{
    float frameToClip[4];
    float texelToUv[4];
};

// Mirrors RippleUniforms in frame_ripple.frag.hlsl.
struct RippleUniforms
{
    float tint[4];
};

float channel(std::uint32_t color, int shift)
{
    return static_cast<float>((color >> shift) & 0xff) / 255.0f;
}

} // namespace

FrameRipplePass::FrameRipplePass(gpu::Device &device, SDL_GPUTextureFormat format) : m_device(device), m_format(format)
{
    m_vertexShader = m_device.loadShader("ui.vert", SDL_GPU_SHADERSTAGE_VERTEX, {.samplers = 0, .uniformBuffers = 1});
    m_fragmentShader = m_device.loadShader("frame_ripple.frag", SDL_GPU_SHADERSTAGE_FRAGMENT,
                                           {.samplers = 1, .uniformBuffers = 1});

    SDL_GPUSamplerCreateInfo sampler{};
    sampler.min_filter = SDL_GPU_FILTER_LINEAR;
    sampler.mag_filter = SDL_GPU_FILTER_LINEAR;
    sampler.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    sampler.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    m_sampler = m_device.createSampler(sampler);

    SDL_GPUVertexBufferDescription buffer{};
    buffer.slot = 0;
    buffer.pitch = sizeof(Vertex);
    buffer.input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX;

    const std::array<SDL_GPUVertexAttribute, 3> attributes{{
        {0, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(Vertex, x)},
        {1, 0, SDL_GPU_VERTEXELEMENTFORMAT_FLOAT2, offsetof(Vertex, u)},
        {2, 0, SDL_GPU_VERTEXELEMENTFORMAT_UBYTE4_NORM, offsetof(Vertex, color)},
    }};

    SDL_GPUColorTargetDescription target{};
    target.format = format;
    target.blend_state.enable_color_write_mask = true;
    target.blend_state.color_write_mask =
        SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G | SDL_GPU_COLORCOMPONENT_B;

    SDL_GPUGraphicsPipelineCreateInfo info{};
    info.vertex_shader = m_vertexShader.get();
    info.fragment_shader = m_fragmentShader.get();
    info.vertex_input_state.vertex_buffer_descriptions = &buffer;
    info.vertex_input_state.num_vertex_buffers = 1;
    info.vertex_input_state.vertex_attributes = attributes.data();
    info.vertex_input_state.num_vertex_attributes = attributes.size();
    info.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    info.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    info.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    info.multisample_state.sample_count = SDL_GPU_SAMPLECOUNT_1;
    info.target_info.color_target_descriptions = &target;
    info.target_info.num_color_targets = 1;

    SDL_GPUGraphicsPipeline *const pipeline = SDL_CreateGPUGraphicsPipeline(m_device.get(), &info);
    if (pipeline == nullptr)
        gpu::logGpuError("SDL_CreateGPUGraphicsPipeline(frame ripple)");
    m_pipeline = gpu::GraphicsPipeline(m_device.get(), pipeline);
}

void FrameRipplePass::ensureCopy(unsigned width, unsigned height)
{
    if (m_copy && m_copyWidth == width && m_copyHeight == height)
        return;
    m_copy = m_device.createTexture(width, height, m_format, SDL_GPU_TEXTUREUSAGE_SAMPLER);
    m_copyWidth = width;
    m_copyHeight = height;
}

void FrameRipplePass::record(SDL_GPUCommandBuffer *commands, SDL_GPUTexture *scene, SDL_GPUBuffer *vertices,
                             const DrawCall &draw, unsigned width, unsigned height)
{
    if (!m_pipeline || draw.vertexCount == 0)
        return;
    ensureCopy(width, height);

    // The sheet samples the frame and lands on it.
    SDL_GPUCopyPass *const copy = SDL_BeginGPUCopyPass(commands);
    if (copy == nullptr)
    {
        gpu::logGpuError("SDL_BeginGPUCopyPass(frame ripple)");
        return;
    }
    SDL_GPUTextureLocation source{};
    source.texture = scene;
    SDL_GPUTextureLocation destination{};
    destination.texture = m_copy.get();
    SDL_CopyGPUTextureToTexture(copy, &source, &destination, width, height, 1, false);
    SDL_EndGPUCopyPass(copy);

    SDL_GPUColorTargetInfo target{};
    target.texture = scene;
    target.load_op = SDL_GPU_LOADOP_LOAD;
    target.store_op = SDL_GPU_STOREOP_STORE;

    SDL_GPURenderPass *const pass = SDL_BeginGPURenderPass(commands, &target, 1, nullptr);
    if (pass == nullptr)
    {
        gpu::logGpuError("SDL_BeginGPURenderPass(frame ripple)");
        return;
    }
    SDL_BindGPUGraphicsPipeline(pass, m_pipeline.get());

    SDL_GPUBufferBinding binding{};
    binding.buffer = vertices;
    SDL_BindGPUVertexBuffers(pass, 0, &binding, 1);

    const SDL_GPUTextureSamplerBinding frame{m_copy.get(), m_sampler.get()};
    SDL_BindGPUFragmentSamplers(pass, 0, &frame, 1);

    // The vertices' u, v are fractions of the frame already.
    const VertexUniforms vertexUniforms{{2.0f / kFrameUnits, -2.0f / kFrameUnits, -1.0f, 1.0f}, {1.0f, 1.0f, 0.0f, 0.0f}};
    SDL_PushGPUVertexUniformData(commands, 0, &vertexUniforms, sizeof(vertexUniforms));

    constexpr float kGsOne = 128.0f;
    const RippleUniforms uniforms{{channel(draw.rippleTint, 0), channel(draw.rippleTint, 8),
                                   channel(draw.rippleTint, 16), static_cast<float>(draw.rippleTint >> 24) / kGsOne}};
    SDL_PushGPUFragmentUniformData(commands, 0, &uniforms, sizeof(uniforms));

    SDL_DrawGPUPrimitives(pass, draw.vertexCount, 1, draw.firstVertex, 0);
    SDL_EndGPURenderPass(pass);
}

} // namespace yamipan::rendering
