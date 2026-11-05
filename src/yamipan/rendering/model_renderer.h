// The game's 3D draw module: what VU1 and the GS do with a model. rena/rena_draw.cpp keeps the EE's decisions
// and hands over batches, read in place in the model file; this transforms and lights them, and draws them
// with a depth buffer. No VU1 and no GS memory: a batch is decoded at its source.
//
// The game side runs on the game thread and fills layers like UiCanvas does, one per packet list, joined
// into the Frame at the kick. The GPU side runs on the render thread, from what is in the Frame.
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include <SDL3/SDL_gpu.h>

#include "yamipan/gpu/device.h"
#include "yamipan/rendering/display_geometry.h"
#include "yamipan/rendering/frame.h"
#include "yamipan/rendering/yamipan_drawmodel.h"

namespace yamipan::rendering
{

class Renderer;

class ModelRenderer
{
public:
    ModelRenderer(Renderer &renderer, gpu::Device &device);

    // From the game thread.
    void beginPass(const void *list, const YamiPanModelPass &pass);
    void setMaterial(const void *list, const YamiPanModelMaterial &material);
    void setLights(const void *list, const YamiPanModelLights &lights);
    void drawBatch(const void *list, const float world[4][4], const YamiPanModelBatch &batch);
    void endPass(const void *list);
    void forgetTextures();

    // The kick: moves the layer's passes to the end of the frame.
    void collect(const void *list, Frame &frame);

    // From the render thread.
    void ensureDepth(int width, int height);
    void uploadVertices(SDL_GPUCopyPass *copy, const Frame &frame);
    void recordPass(SDL_GPUCommandBuffer *commands, const Frame &frame, const ModelPass &pass, SDL_GPUTexture *scene,
                    SDL_GPUTexture *white, const PictureLayout &layout);

private:
    struct Layer
    {
        std::vector<ModelVertex> vertices;
        std::vector<ModelSpotVertex> spotVertices;
        std::vector<ModelShadowVertex> shadowVertices;
        std::vector<ModelDraw> draws;
        std::vector<ModelPass> passes;
        std::vector<ModelVertex> batch; // one batch's vertices, each once: a strip reuses them
        std::vector<int> triangles;     // the batch's faces that were kept, as indices into batch
        std::vector<ModelShadowVertex> shadowBatch; // the same for a shadow
        YamiPanModelLights lights{};
        YamiPanModelPointLight point{};
        YamiPanModelSpotLight spot{};
        YamiPanModelShadows shadows{};
        std::array<float, 3> eye{};
        std::shared_ptr<Image> image;
        float scrollU = 0.0f;
        float scrollV = 0.0f;
        bool blended = false;
        bool unlit = false;
        bool pointFlat = false;
        bool noSpot = false;
        bool noShadow = false;
        bool open = false;
    };

    // A growing GPU vertex buffer and what fills it.
    struct VertexStream
    {
        gpu::Buffer buffer;
        gpu::TransferBuffer upload;
        unsigned capacity = 0;
    };

    std::shared_ptr<Image> imageOf(const void *texture);
    void uploadStream(SDL_GPUCopyPass *copy, VertexStream &stream, const void *data, unsigned bytes);
    SDL_GPUGraphicsPipeline *pipelineFor(bool blended);
    SDL_GPUGraphicsPipeline *createProjectedPipeline(const SDL_GPUColorTargetDescription &target);
    SDL_GPUGraphicsPipeline *spotPipeline();
    SDL_GPUGraphicsPipeline *shadowPipeline();

    Renderer &m_renderer;
    gpu::Device &m_device;

    std::mutex m_lock;
    std::unordered_map<const void *, Layer> m_layers;
    std::unordered_map<const void *, std::shared_ptr<Image>> m_images; // by Tim2Texture: decoding hashes the picture

    gpu::Shader m_vertexShader;
    gpu::Shader m_fragmentShader;
    gpu::Shader m_spotVertexShader;
    gpu::Shader m_spotFragmentShader;
    std::map<bool, gpu::GraphicsPipeline> m_pipelines;
    gpu::GraphicsPipeline m_spotPipeline;
    gpu::GraphicsPipeline m_shadowPipeline;
    gpu::Texture m_depth;
    int m_depthWidth = 0;
    int m_depthHeight = 0;
    VertexStream m_vertices;
    VertexStream m_spotVertices;
    VertexStream m_shadowVertices;
};

} // namespace yamipan::rendering
