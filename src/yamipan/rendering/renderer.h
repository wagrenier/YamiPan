// The render thread: game frames in, pictures out. It owns the GPU's work, ImGui and the present, and
// keeps the window alive on its own: when no new game frame has come it shows the last one again, so the
// overlay, resizing and fullscreen all work through a load, and a vsync stall never holds the EE side. (The
// siblings render on the game thread. FS2 flips from its frame-sync thread, which must never block.)
//
// The picture goes through two targets: the scene, the game's frame at the render resolution, and the
// output, the window's size, where the scene lands in its picture rect with the overlay on top. The output
// keeps one format for the renderer's life, because ImGui bakes it into its pipeline; the present pass
// then writes whatever the swapchain is running.
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <vector>

#include <SDL3/SDL_gpu.h>

#include "yamipan/core/config.h"
#include "yamipan/gpu/device.h"
#include "yamipan/os/thread.h"
#include "yamipan/rendering/display_geometry.h"
#include "yamipan/rendering/frame.h"
#include "yamipan/rendering/texture_store.h"
#include "yamipan/rendering/ui_canvas.h"

namespace yamipan
{
class EventQueue;
class Window;
}

namespace yamipan::ui
{
class Overlay;
}

namespace yamipan::rendering
{

class PresentPass;
class Screenshotter;

class Renderer
{
public:
    Renderer(Window &window, gpu::Device &device, Config &config, EventQueue &events, ui::Overlay &overlay);
    ~Renderer();

    void start();
    void stop();

    UiCanvas &canvas() { return m_canvas; }
    TextureStore &textures() { return m_textures; }

    // A frame to fill, with its storage from a frame already shown.
    std::unique_ptr<Frame> takeFreeFrame();

    // Hands a finished frame over. Only the newest matters: one not yet shown is overtaken.
    void submit(std::unique_ptr<Frame> frame);

    // The layout of the last present, from any thread. Read this, never the window, for the shape.
    PictureLayout layout() const;

    struct Stats
    {
        double presentRate = 0.0;
        double gameRate = 0.0;
        std::size_t draws = 0;
        std::size_t vertices = 0;
    };
    Stats stats() const;

    // Applies the settings the swapchain depends on (from the render thread: the overlay).
    void applyPresentMode();

    SDL_GPUTextureFormat outputFormat() const { return m_outputFormat; }

private:
    void threadMain();
    void renderOnce();
    void ensureTargets(const PictureLayout &layout);
    void uploadImages(SDL_GPUCopyPass *copy, Frame &frame);
    void uploadVertices(SDL_GPUCopyPass *copy, const Frame &frame);
    void recordScene(SDL_GPUCommandBuffer *commands, const Frame &frame, const PictureLayout &layout);
    void recordOutput(SDL_GPUCommandBuffer *commands, const PictureLayout &layout);
    SDL_GPUGraphicsPipeline *pipelineFor(PipelineKey key);
    SDL_GPUSampler *samplerFor(bool linear, bool repeat) const;
    void updateStats(bool newFrame);

    Window &m_window;
    gpu::Device &m_device;
    Config &m_config;
    EventQueue &m_events;
    ui::Overlay &m_overlay;
    UiCanvas m_canvas;
    TextureStore m_textures;

    SDL_GPUTextureFormat m_sceneFormat;
    SDL_GPUTextureFormat m_outputFormat;
    gpu::Texture m_scene;
    gpu::Texture m_output;
    gpu::Texture m_white;
    int m_sceneWidth = 0;
    int m_sceneHeight = 0;
    int m_outputWidth = 0;
    int m_outputHeight = 0;

    gpu::Shader m_vertexShader;
    gpu::Shader m_fragmentShader;
    std::map<PipelineKey, gpu::GraphicsPipeline> m_pipelines;
    std::array<gpu::Sampler, 4> m_samplers;

    gpu::Buffer m_vertexBuffer;
    gpu::TransferBuffer m_vertexUpload;
    unsigned m_vertexCapacity = 0;
    std::vector<gpu::TransferBuffer> m_staging;
    bool m_whiteUploaded = false;

    std::unique_ptr<PresentPass> m_present;
    std::unique_ptr<Screenshotter> m_screenshots;

    // The frame on screen, kept to draw again when the scene target changes size.
    std::unique_ptr<Frame> m_shown;

    mutable std::mutex m_lock;
    std::unique_ptr<Frame> m_incoming;
    std::vector<std::unique_ptr<Frame>> m_free;
    PictureLayout m_layout;
    Stats m_stats;

    std::uint64_t m_lastFrameNumber = 0;
    std::uint64_t m_statPresents = 0;
    std::uint64_t m_statFrames = 0;
    std::uint64_t m_statStart = 0;

    std::atomic<bool> m_stopping{false};
    std::unique_ptr<os::Thread> m_thread;
};

} // namespace yamipan::rendering
