#include "yamipan/rendering/renderer.h"

#include <algorithm>
#include <chrono>
#include <cstring>
#include <thread>

#include <SDL3/SDL_timer.h>

#include "yamipan/core/event_queue.h"
#include "yamipan/core/logger.h"
#include "yamipan/core/window.h"
#include "yamipan/rendering/film_overlay_pass.h"
#include "yamipan/rendering/frame_palette_pass.h"
#include "yamipan/rendering/frame_ripple_pass.h"
#include "yamipan/rendering/present_pass.h"
#include "yamipan/rendering/screenshotter.h"
#include "yamipan/ui/overlay.h"

namespace yamipan::rendering
{
namespace
{

constexpr std::size_t kRenderThreadStack = 2 * 1024 * 1024;
constexpr unsigned kInitialVertexCapacity = 1024 * 1024;
constexpr auto kIdleDelay = std::chrono::milliseconds(16);

// Mirrors UiVertexUniforms in ui.vert.hlsl.
struct UiVertexUniforms
{
    float frameToClip[4];
    float texelToUv[4];
};

// Mirrors UiFragmentUniforms in ui.frag.hlsl.
struct UiFragmentUniforms
{
    float params[4];
};

SDL_GPUPresentMode sdlPresentMode(PresentMode mode)
{
    switch (mode)
    {
    case PresentMode::Mailbox:
        return SDL_GPU_PRESENTMODE_MAILBOX;
    case PresentMode::Immediate:
        return SDL_GPU_PRESENTMODE_IMMEDIATE;
    case PresentMode::VSync:
        break;
    }
    return SDL_GPU_PRESENTMODE_VSYNC;
}

SDL_GPUColorTargetBlendState blendStateOf(PipelineKey key)
{
    SDL_GPUColorTargetBlendState state{};
    state.enable_blend = key.blend != BlendMode::None;
    state.color_blend_op = SDL_GPU_BLENDOP_ADD;
    // The GS blends colour only: the frame's alpha takes the source's.
    state.src_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
    state.dst_alpha_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
    state.alpha_blend_op = SDL_GPU_BLENDOP_ADD;

    switch (key.blend)
    {
    case BlendMode::None:
    case BlendMode::Alpha:
        state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        break;
    case BlendMode::Additive:
        state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        break;
    case BlendMode::Subtractive:
        state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_SRC_ALPHA;
        state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        state.color_blend_op = SDL_GPU_BLENDOP_REVERSE_SUBTRACT;
        break;
    case BlendMode::DestinationAlpha:
        state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_DST_ALPHA;
        state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_DST_ALPHA;
        break;
    case BlendMode::FixedAlpha:
        state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_CONSTANT_COLOR;
        state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_CONSTANT_COLOR;
        break;
    case BlendMode::FixedAdditive:
        state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_CONSTANT_COLOR;
        state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        break;
    case BlendMode::DestinationScale:
        state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
        state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_CONSTANT_COLOR;
        break;
    case BlendMode::DestinationBoost:
        state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_DST_COLOR;
        state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE;
        break;
    case BlendMode::DestinationFade:
        state.src_color_blendfactor = SDL_GPU_BLENDFACTOR_ZERO;
        state.dst_color_blendfactor = SDL_GPU_BLENDFACTOR_ONE_MINUS_SRC_ALPHA;
        break;
    }

    if (key.mask != ColorMask::All)
    {
        state.enable_color_write_mask = true;
        state.color_write_mask = key.mask == ColorMask::ColorOnly
                                     ? SDL_GPU_COLORCOMPONENT_R | SDL_GPU_COLORCOMPONENT_G | SDL_GPU_COLORCOMPONENT_B
                                     : SDL_GPU_COLORCOMPONENT_A;
    }
    return state;
}

} // namespace

Renderer::Renderer(Window &window, gpu::Device &device, Config &config, EventQueue &events, ui::Overlay &overlay)
    : m_window(window), m_device(device), m_config(config), m_events(events), m_overlay(overlay), m_canvas(*this),
      m_models(*this, device),
      m_sceneFormat(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM), m_outputFormat(SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM)
{
    m_vertexShader = m_device.loadShader("ui.vert", SDL_GPU_SHADERSTAGE_VERTEX, {.samplers = 0, .uniformBuffers = 1});
    m_fragmentShader =
        m_device.loadShader("ui.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, {.samplers = 1, .uniformBuffers = 1});

    for (std::size_t index = 0; index < m_samplers.size(); index++)
    {
        const bool linear = (index & 1) != 0;
        const bool repeat = (index & 2) != 0;
        const SDL_GPUSamplerAddressMode address =
            repeat ? SDL_GPU_SAMPLERADDRESSMODE_REPEAT : SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
        const SDL_GPUFilter filter = linear ? SDL_GPU_FILTER_LINEAR : SDL_GPU_FILTER_NEAREST;

        SDL_GPUSamplerCreateInfo info{};
        info.min_filter = filter;
        info.mag_filter = filter;
        info.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
        info.address_mode_u = address;
        info.address_mode_v = address;
        info.address_mode_w = address;
        m_samplers[index] = m_device.createSampler(info);
    }

    m_white = m_device.createTexture(1, 1, SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER);
    m_present = std::make_unique<PresentPass>(m_device, m_window, m_config);
    m_framePalette = std::make_unique<FramePalettePass>(m_device, m_sceneFormat);
    m_frameRipple = std::make_unique<FrameRipplePass>(m_device, m_sceneFormat);
    m_filmOverlay = std::make_unique<FilmOverlayPass>(m_device, m_sceneFormat);
    m_screenshots = std::make_unique<Screenshotter>(m_device);
    applyPresentMode();
}

Renderer::~Renderer()
{
    stop();
}

void Renderer::start()
{
    m_statStart = SDL_GetTicks();
    m_thread = std::make_unique<os::Thread>("YamiPanRender", kRenderThreadStack, [this] { threadMain(); });
}

void Renderer::stop()
{
    m_stopping.store(true);
    m_thread.reset();
}

std::unique_ptr<Frame> Renderer::takeFreeFrame()
{
    std::scoped_lock lock(m_lock);
    if (m_free.empty())
        return std::make_unique<Frame>();
    std::unique_ptr<Frame> frame = std::move(m_free.back());
    m_free.pop_back();
    frame->clear();
    return frame;
}

void Renderer::submit(std::unique_ptr<Frame> frame)
{
    std::scoped_lock lock(m_lock);
    if (m_incoming != nullptr)
        m_free.push_back(std::move(m_incoming));
    m_incoming = std::move(frame);
}

PictureLayout Renderer::layout() const
{
    std::scoped_lock lock(m_lock);
    return m_layout;
}

Renderer::Stats Renderer::stats() const
{
    std::scoped_lock lock(m_lock);
    return m_stats;
}

void Renderer::applyPresentMode()
{
    SDL_GPUPresentMode mode = sdlPresentMode(m_config.display.presentMode);
    if (!SDL_WindowSupportsGPUPresentMode(m_device.get(), m_window.get(), mode))
        mode = SDL_GPU_PRESENTMODE_VSYNC;
    if (!SDL_SetGPUSwapchainParameters(m_device.get(), m_window.get(), SDL_GPU_SWAPCHAINCOMPOSITION_SDR, mode))
        gpu::logGpuError("SDL_SetGPUSwapchainParameters");
}

void Renderer::threadMain()
{
    m_overlay.initializeOnRenderThread(m_outputFormat);
    while (!m_stopping.load())
        renderOnce();
    SDL_WaitForGPUIdle(m_device.get());
    m_overlay.shutdownOnRenderThread();
}

void Renderer::ensureTargets(const PictureLayout &layout)
{
    constexpr SDL_GPUTextureUsageFlags kUsage = SDL_GPU_TEXTUREUSAGE_SAMPLER | SDL_GPU_TEXTUREUSAGE_COLOR_TARGET;

    if (!m_scene || m_sceneWidth != layout.renderWidth || m_sceneHeight != layout.renderHeight)
    {
        m_scene = m_device.createTexture(static_cast<unsigned>(layout.renderWidth),
                                         static_cast<unsigned>(layout.renderHeight), m_sceneFormat, kUsage);
        m_sceneWidth = layout.renderWidth;
        m_sceneHeight = layout.renderHeight;
    }
    if (!m_output || m_outputWidth != layout.windowWidth || m_outputHeight != layout.windowHeight)
    {
        m_output = m_device.createTexture(static_cast<unsigned>(layout.windowWidth),
                                          static_cast<unsigned>(layout.windowHeight), m_outputFormat, kUsage);
        m_outputWidth = layout.windowWidth;
        m_outputHeight = layout.windowHeight;
    }
    m_models.ensureDepth(layout.renderWidth, layout.renderHeight);
}

SDL_GPUGraphicsPipeline *Renderer::pipelineFor(PipelineKey key)
{
    if (const auto found = m_pipelines.find(key); found != m_pipelines.end())
        return found->second.get();

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
    target.format = m_sceneFormat;
    target.blend_state = blendStateOf(key);

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
        gpu::logGpuError("SDL_CreateGPUGraphicsPipeline");
    m_pipelines.emplace(key, gpu::GraphicsPipeline(m_device.get(), pipeline));
    return pipeline;
}

SDL_GPUSampler *Renderer::samplerFor(bool linear, bool repeat) const
{
    return m_samplers[(linear ? 1u : 0u) | (repeat ? 2u : 0u)].get();
}

void Renderer::uploadImages(SDL_GPUCopyPass *copy, Frame &frame)
{
    const auto upload = [&](Image *image) {
        if (image == nullptr || image->texture != nullptr || image->pixels.empty())
            return;

        image->texture = std::make_shared<gpu::Texture>(
            m_device.createTexture(static_cast<unsigned>(image->width), static_cast<unsigned>(image->height),
                                   SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM, SDL_GPU_TEXTUREUSAGE_SAMPLER));
        m_device.uploadTexture(copy, image->texture->get(), static_cast<unsigned>(image->width),
                               static_cast<unsigned>(image->height), image->pixels,
                               m_staging.emplace_back());
        std::vector<std::byte>().swap(image->pixels);
    };

    for (const DrawCall &draw : frame.draws)
    {
        upload(draw.image.get());
        upload(draw.framePalette.get());
        if (draw.film != nullptr)
        {
            for (const FilmCorner &corner : draw.film->corners)
            {
                upload(corner.shown.get());
                upload(corner.leaving.get());
            }
        }
    }
    for (const ModelDraw &draw : frame.modelDraws)
        upload(draw.image.get());
    for (const ModelPass &pass : frame.modelPasses)
        upload(pass.spotImage.get());
}

void Renderer::uploadVertices(SDL_GPUCopyPass *copy, const Frame &frame)
{
    const auto bytes = static_cast<unsigned>(frame.vertices.size() * sizeof(Vertex));
    if (bytes == 0)
        return;

    if (bytes > m_vertexCapacity || !m_vertexBuffer)
    {
        m_vertexCapacity = std::max({bytes * 2, m_vertexCapacity, kInitialVertexCapacity});
        m_vertexBuffer = m_device.createVertexBuffer(m_vertexCapacity);
        m_vertexUpload = m_device.createUploadBuffer(m_vertexCapacity);
    }

    void *const mapped = SDL_MapGPUTransferBuffer(m_device.get(), m_vertexUpload.get(), true);
    if (mapped == nullptr)
        return;
    std::memcpy(mapped, frame.vertices.data(), bytes);
    SDL_UnmapGPUTransferBuffer(m_device.get(), m_vertexUpload.get());

    SDL_GPUTransferBufferLocation source{};
    source.transfer_buffer = m_vertexUpload.get();

    SDL_GPUBufferRegion destination{};
    destination.buffer = m_vertexBuffer.get();
    destination.size = bytes;

    SDL_UploadToGPUBuffer(copy, &source, &destination, true);
}

void Renderer::recordScene(SDL_GPUCommandBuffer *commands, const Frame &frame, const PictureLayout &layout)
{
    // The 2D draws and the model passes take turns, as the game's lists do. A pass of models has a depth
    // target and the 2D pipelines have none, so each stretch is its own render pass.
    std::size_t nextDraw = 0;
    bool cleared = false;
    for (const ModelPass &pass : frame.modelPasses)
    {
        recordUi(commands, frame, layout, nextDraw, pass.uiDrawsBefore, !cleared);
        cleared = true;
        nextDraw = pass.uiDrawsBefore;
        m_models.recordPass(commands, frame, pass, m_scene.get(), m_white.get(), layout);
    }
    recordUi(commands, frame, layout, nextDraw, frame.draws.size(), !cleared);
}

void Renderer::recordUi(SDL_GPUCommandBuffer *commands, const Frame &frame, const PictureLayout &layout,
                        std::size_t first, std::size_t last, bool clear)
{
    // A pass over the frame reads the scene target, so it stands between render passes.
    std::size_t next = first;
    for (std::size_t index = first; index < last; index++)
    {
        const DrawCall &draw = frame.draws[index];
        if (draw.framePalette == nullptr && draw.film == nullptr && !draw.frameRipple)
            continue;
        recordDraws(commands, frame, layout, next, index, clear);
        clear = false;
        next = index + 1;

        const auto width = static_cast<unsigned>(layout.renderWidth);
        const auto height = static_cast<unsigned>(layout.renderHeight);
        if (draw.frameRipple)
            m_frameRipple->record(commands, m_scene.get(), m_vertexBuffer.get(), draw, width, height);
        else if (draw.film != nullptr)
            m_filmOverlay->record(commands, m_scene.get(), *draw.film, width, height);
        else if (draw.framePalette->texture != nullptr)
            m_framePalette->record(commands, m_scene.get(), draw.framePalette->texture->get(), width, height);
    }
    recordDraws(commands, frame, layout, next, last, clear);
}

void Renderer::recordDraws(SDL_GPUCommandBuffer *commands, const Frame &frame, const PictureLayout &layout,
                           std::size_t first, std::size_t last, bool clear)
{
    if (first == last && !clear)
        return;

    SDL_GPUColorTargetInfo target{};
    target.texture = m_scene.get();
    target.load_op = clear ? SDL_GPU_LOADOP_CLEAR : SDL_GPU_LOADOP_LOAD;
    target.store_op = SDL_GPU_STOREOP_STORE;
    target.clear_color = {0.0f, 0.0f, 0.0f, 0.0f};

    SDL_GPURenderPass *const pass = SDL_BeginGPURenderPass(commands, &target, 1, nullptr);
    if (pass == nullptr)
    {
        gpu::logGpuError("SDL_BeginGPURenderPass(scene)");
        return;
    }

    if (first != last)
    {
        SDL_GPUBufferBinding vertices{};
        vertices.buffer = m_vertexBuffer.get();
        SDL_BindGPUVertexBuffers(pass, 0, &vertices, 1);
    }

    // The frame fills the scene target, whatever its shape.
    const float scaleX = static_cast<float>(layout.renderWidth) / kFrameWidth;
    const float scaleY = static_cast<float>(layout.renderHeight) / kFrameHeight;

    for (std::size_t index = first; index < last; index++)
    {
        const DrawCall &draw = frame.draws[index];
        SDL_GPUGraphicsPipeline *const pipeline = pipelineFor(draw.pipeline);
        if (pipeline == nullptr || draw.vertexCount == 0)
            continue;
        SDL_BindGPUGraphicsPipeline(pass, pipeline);

        const Image *const image = draw.image.get();
        SDL_GPUTexture *const texture = image != nullptr && image->texture != nullptr ? image->texture->get()
                                                                                       : m_white.get();
        SDL_GPUTextureSamplerBinding binding{};
        binding.texture = texture;
        binding.sampler = samplerFor(draw.linearFilter, draw.repeat);
        SDL_BindGPUFragmentSamplers(pass, 0, &binding, 1);

        const float texelWidth = image != nullptr ? 1.0f / static_cast<float>(image->width) : 1.0f;
        const float texelHeight = image != nullptr ? 1.0f / static_cast<float>(image->height) : 1.0f;
        const UiVertexUniforms vertexUniforms{{2.0f / kFrameWidth, -2.0f / kFrameHeight, -1.0f, 1.0f},
                                              {texelWidth, texelHeight, 0.0f, 0.0f}};
        SDL_PushGPUVertexUniformData(commands, 0, &vertexUniforms, sizeof(vertexUniforms));

        // A texture modulates on the GS's scale, 0x80 as 1.0; a flat colour goes out as it is.
        const float colourScale = image != nullptr ? 255.0f / 128.0f : 1.0f;
        const UiFragmentUniforms fragmentUniforms{
            {static_cast<float>(draw.alphaTestRef) / 128.0f, colourScale, 0.0f, 0.0f}};
        SDL_PushGPUFragmentUniformData(commands, 0, &fragmentUniforms, sizeof(fragmentUniforms));

        const float fixed = static_cast<float>(draw.fixedAlpha) / 128.0f;
        const SDL_FColor constants{fixed, fixed, fixed, fixed};
        SDL_SetGPUBlendConstants(pass, constants);

        const auto clampX = [&](float x) { return std::clamp(static_cast<int>(x * scaleX), 0, layout.renderWidth); };
        const auto clampY = [&](float y) { return std::clamp(static_cast<int>(y * scaleY), 0, layout.renderHeight); };
        const SDL_Rect scissor{clampX(draw.scissor.x0), clampY(draw.scissor.y0),
                               clampX(draw.scissor.x1) - clampX(draw.scissor.x0),
                               clampY(draw.scissor.y1) - clampY(draw.scissor.y0)};
        SDL_SetGPUScissor(pass, &scissor);

        SDL_DrawGPUPrimitives(pass, draw.vertexCount, 1, draw.firstVertex, 0);
    }
    SDL_EndGPURenderPass(pass);
}

void Renderer::recordOutput(SDL_GPUCommandBuffer *commands, const PictureLayout &layout)
{
    // The picture in its rect, black around it: the bars of a shape that is not the window's.
    SDL_GPUBlitInfo blit{};
    blit.source.texture = m_scene.get();
    blit.source.w = static_cast<Uint32>(layout.renderWidth);
    blit.source.h = static_cast<Uint32>(layout.renderHeight);
    blit.destination.texture = m_output.get();
    blit.destination.x = static_cast<Uint32>(layout.picture.x);
    blit.destination.y = static_cast<Uint32>(layout.picture.y);
    blit.destination.w = static_cast<Uint32>(layout.picture.width);
    blit.destination.h = static_cast<Uint32>(layout.picture.height);
    blit.load_op = SDL_GPU_LOADOP_CLEAR;
    blit.clear_color = {0.0f, 0.0f, 0.0f, 1.0f};
    blit.filter = m_config.display.upscaleFilter == UpscaleFilter::Linear ? SDL_GPU_FILTER_LINEAR
                                                                           : SDL_GPU_FILTER_NEAREST;
    SDL_BlitGPUTexture(commands, &blit);
}

void Renderer::updateStats(bool newFrame)
{
    m_statPresents++;
    if (newFrame)
        m_statFrames++;

    const std::uint64_t now = SDL_GetTicks();
    const std::uint64_t elapsed = now - m_statStart;
    if (elapsed < 500)
        return;

    std::scoped_lock lock(m_lock);
    m_stats.presentRate = static_cast<double>(m_statPresents) * 1000.0 / static_cast<double>(elapsed);
    m_stats.gameRate = static_cast<double>(m_statFrames) * 1000.0 / static_cast<double>(elapsed);
    if (m_shown != nullptr)
    {
        m_stats.draws = m_shown->draws.size();
        m_stats.vertices = m_shown->vertices.size();
    }
    m_statPresents = 0;
    m_statFrames = 0;
    m_statStart = now;
}

void Renderer::renderOnce()
{
    const auto [windowWidth, windowHeight] = m_window.pixelSize();
    if (windowWidth <= 0 || windowHeight <= 0)
    {
        std::this_thread::sleep_for(kIdleDelay);
        return;
    }

    m_events.drain([this](const SDL_Event &event) { m_overlay.processEvent(event); });

    const PictureLayout layout = computeLayout(windowWidth, windowHeight, m_config.display);
    const bool layoutChanged = [&] {
        std::scoped_lock lock(m_lock);
        const bool changed = !(m_layout == layout);
        m_layout = layout;
        return changed;
    }();
    ensureTargets(layout);

    std::unique_ptr<Frame> incoming;
    {
        std::scoped_lock lock(m_lock);
        incoming = std::move(m_incoming);
    }

    bool newFrame = false;
    if (incoming != nullptr)
    {
        if (m_shown != nullptr)
        {
            std::scoped_lock lock(m_lock);
            m_free.push_back(std::move(m_shown));
        }
        m_shown = std::move(incoming);
        newFrame = true;
    }
    const bool redraw = newFrame || layoutChanged;

    m_overlay.beginFrame();
    m_overlay.draw(*this);
    m_overlay.endFrame();

    SDL_GPUCommandBuffer *const commands = SDL_AcquireGPUCommandBuffer(m_device.get());
    if (commands == nullptr)
    {
        gpu::logGpuError("SDL_AcquireGPUCommandBuffer");
        return;
    }

    SDL_GPUCopyPass *const copy = SDL_BeginGPUCopyPass(commands);
    if (!m_whiteUploaded)
    {
        constexpr std::array<std::byte, 4> kWhitePixel{std::byte{0xff}, std::byte{0xff}, std::byte{0xff},
                                                       std::byte{0xff}};
        m_device.uploadTexture(copy, m_white.get(), 1, 1, kWhitePixel, m_staging.emplace_back());
        m_whiteUploaded = true;
    }
    if (redraw && m_shown != nullptr)
    {
        uploadImages(copy, *m_shown);
        uploadVertices(copy, *m_shown);
        m_models.uploadVertices(copy, *m_shown);
    }
    SDL_EndGPUCopyPass(copy);

    static const Frame kEmptyFrame;
    if (redraw || m_shown == nullptr)
        recordScene(commands, m_shown != nullptr ? *m_shown : kEmptyFrame, layout);
    recordOutput(commands, layout);

    m_overlay.prepareDrawData(commands);
    {
        SDL_GPUColorTargetInfo target{};
        target.texture = m_output.get();
        target.load_op = SDL_GPU_LOADOP_LOAD;
        target.store_op = SDL_GPU_STOREOP_STORE;
        SDL_GPURenderPass *const pass = SDL_BeginGPURenderPass(commands, &target, 1, nullptr);
        if (pass != nullptr)
        {
            m_overlay.renderDrawData(commands, pass);
            SDL_EndGPURenderPass(pass);
        }
    }

    const bool shotDue = m_screenshots->recordIfDue(commands, m_scene.get(), m_output.get(), layout);

    SDL_GPUTexture *swapchain = nullptr;
    Uint32 swapchainWidth = 0;
    Uint32 swapchainHeight = 0;
    if (!SDL_WaitAndAcquireGPUSwapchainTexture(commands, m_window.get(), &swapchain, &swapchainWidth, &swapchainHeight))
    {
        gpu::logGpuError("SDL_AcquireGPUSwapchainTexture");
        SDL_CancelGPUCommandBuffer(commands);
        return;
    }
    if (swapchain != nullptr)
        m_present->record(commands, m_output.get(), swapchain, swapchainWidth, swapchainHeight);

    if (!SDL_SubmitGPUCommandBuffer(commands))
        gpu::logGpuError("SDL_SubmitGPUCommandBuffer");
    m_staging.clear();

    if (shotDue)
        m_screenshots->write();

    if (swapchain == nullptr)
        std::this_thread::sleep_for(kIdleDelay); // minimised: nothing paces this loop
    updateStats(newFrame);
}

} // namespace yamipan::rendering
