// The game's 3D draw module: what VU1 and the GS do with a model. rena/rena_draw.cpp keeps the EE's decisions
// and hands over batches, read in place in the model file; this transforms and lights them, and draws them
// with a depth buffer. No VU1 and no GS memory: a batch is decoded at its source.
//
// A rigid batch goes to the GPU once, the first frame that draws it, cached by where its data lies in the model
// file; from then on a frame only records which batch, with an instance that places and lights it, and
// model_static.vert does VU1's work. A skinned batch (a palette) is still placed and lit here, on the game thread,
// and uploaded whole each frame.
//
// The game side runs on the game thread and fills layers like UiCanvas does, one per packet list, joined
// into the Frame at the kick. The GPU side runs on the render thread, from what is in the Frame.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <tuple>
#include <unordered_map>
#include <utility>
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
    void setEffect(const void *list, const YamiPanModelEffect &effect);
    void drawQuad(const void *list, const YamiPanModelQuad &quad);
    void endPass(const void *list);
    // A model took a texture, which is when a stage's pack comes in at the addresses of the last one: the lookups by
    // texture object and the cached batches both start over.
    void forgetTextures();

    // The kick: moves the layer's passes to the end of the frame.
    void collect(const void *list, Frame &frame);

    // From the render thread.
    void ensureDepth(int width, int height);
    // The frame's vertices and instances, and the batches cached since the last call, whatever frame they are for.
    void uploadVertices(SDL_GPUCopyPass *copy, const Frame &frame);
    void recordPass(SDL_GPUCommandBuffer *commands, const Frame &frame, const ModelPass &pass, SDL_GPUTexture *scene,
                    SDL_GPUTexture *white, const PictureLayout &layout);

private:
    // Where a batch's data lies in the model file. Nothing writes a loaded file's batches (a VCM fade picks another
    // colour set, a scroll goes through VIF's row), so the same key is the same geometry for as long as the file
    // stays; a list's other colour or UV set is another key.
    struct BatchKey
    {
        const float *positions = nullptr;
        const float *normals = nullptr;
        const std::uint8_t *colors = nullptr;
        const std::int16_t *uvs = nullptr;
        int count = 0;
        bool strip = false;

        bool operator==(const BatchKey &) const = default;
    };

    struct BatchKeyHash
    {
        std::size_t operator()(const BatchKey &key) const;
    };

    // A batch on the GPU: its triangles, and its box in the mesh's space, which the spot's and shadows' tests take.
    struct CachedBatch
    {
        std::shared_ptr<ModelGeometryChunk> chunk;
        std::uint32_t firstIndex = 0;
        std::uint32_t indexCount = 0;
        std::int32_t vertexOffset = 0;
        std::array<float, 3> low{};
        std::array<float, 3> high{};
        std::uint64_t fingerprint = 0; // a few of its values: a different file at the same addresses is not reused
    };

    // Batches cached on the game thread and not yet on the GPU: the render thread takes them at its next upload.
    struct PendingGeometry
    {
        std::shared_ptr<ModelGeometryChunk> chunk;
        std::uint32_t firstVertex = 0;
        std::uint32_t firstIndex = 0;
        std::vector<ModelStaticVertex> vertices;
        std::vector<std::uint16_t> indices;
    };

    struct Layer
    {
        std::vector<ModelVertex> vertices;
        std::vector<ModelSpotVertex> spotVertices;
        std::vector<ModelShadowVertex> shadowVertices;
        std::vector<ModelInstance> instances;
        std::vector<ModelBatchDraw> batches;
        std::vector<std::shared_ptr<ModelGeometryChunk>> chunks; // what `batches` draw from
        std::vector<ModelDraw> draws;
        std::vector<ModelPass> passes;
        std::vector<ModelVertex> batch; // one batch's vertices, each once: a strip reuses them
        std::vector<ModelVertex> reflectionBatch;    // the same for the reflection's pass
        std::vector<ModelVertex> reflectionVertices; // the faces kept of the material so far
        std::vector<ModelBatchDraw> reflectionBatches; // the same, of its cached batches
        std::vector<int> triangles;     // the batch's faces that were kept, as indices into batch
        std::vector<ModelShadowVertex> shadowBatch; // the same for a shadow
        YamiPanModelLights lights{};
        YamiPanModelPointLight point{};
        YamiPanModelSpotLight spot{};
        YamiPanModelShadows shadows{};
        std::array<float, 3> eye{};
        std::shared_ptr<Image> image;
        std::shared_ptr<Image> reflection;
        float reflectionScale = 0.0f;
        float scrollU = 0.0f;
        float scrollV = 0.0f;
        bool blended = false;
        BlendMode blend = BlendMode::Alpha;
        int fixedFog = -1;
        bool unlit = false;
        bool pointFlat = false;
        bool noSpot = false;
        bool noShadow = false;
        bool writesDepth = true;
        bool open = false;
    };

    // A growing GPU vertex buffer and what fills it.
    struct VertexStream
    {
        gpu::Buffer buffer;
        gpu::TransferBuffer upload;
        unsigned capacity = 0;
    };

    // What a projected pass blends like (model_renderer.cpp has each one's GS registers).
    enum class ProjectedPass : std::uint8_t
    {
        Spot,
        Shadow,
    };

    std::shared_ptr<Image> imageOf(const void *texture);
    // The material's reflection goes in as one draw over its faces.
    static void endReflection(Layer &layer);

    // A batch placed and lit here, on the game thread: a skinned one (VU1 0x07f and on), or a rigid one too large to
    // cache.
    void drawPlacedBatch(Layer &layer, const float *matrix, const YamiPanModelBatch &batch);
    // A rigid one, cached: only its instance and which faces of it each pass takes are worked out per frame.
    void drawCachedBatch(Layer &layer, const float *matrix, const YamiPanModelBatch &batch);
    // The batch on the GPU, cached on first sight; null if it cannot be (more vertices than a chunk holds).
    const CachedBatch *cachedBatch(const YamiPanModelBatch &batch);
    // Drops every cached batch: what frames hold stays alive until they go.
    void forgetBatches();

    void uploadStream(SDL_GPUCopyPass *copy, VertexStream &stream, const void *data, unsigned bytes);
    void uploadGeometry(SDL_GPUCopyPass *copy);
    SDL_GPUGraphicsPipeline *pipelineFor(BlendMode blend, bool writesDepth); // None: opaque
    SDL_GPUGraphicsPipeline *createProjectedPipeline(const SDL_GPUColorTargetDescription &target);
    SDL_GPUGraphicsPipeline *spotPipeline();
    SDL_GPUGraphicsPipeline *shadowPipeline();
    SDL_GPUGraphicsPipeline *createStaticPipeline(SDL_GPUShader *vertexShader, SDL_GPUShader *fragmentShader,
                                                  const SDL_GPUColorTargetDescription &target, bool writesDepth,
                                                  SDL_GPUCullMode cull);
    SDL_GPUGraphicsPipeline *staticPipelineFor(BlendMode blend, bool writesDepth, SDL_GPUCullMode cull);
    SDL_GPUGraphicsPipeline *staticProjectedPipeline(ProjectedPass pass, SDL_GPUCullMode cull);

    Renderer &m_renderer;
    gpu::Device &m_device;

    std::mutex m_lock;
    std::unordered_map<const void *, Layer> m_layers;
    std::unordered_map<const void *, std::shared_ptr<Image>> m_images; // by Tim2Texture: decoding hashes the picture
    std::unordered_map<BatchKey, CachedBatch, BatchKeyHash> m_batches;
    std::shared_ptr<ModelGeometryChunk> m_chunk; // where new batches go
    std::uint32_t m_chunkVertices = 0;           // how much of it they fill
    std::uint32_t m_chunkIndices = 0;
    std::size_t m_chunkCount = 0;                // since the last flush

    std::mutex m_pendingLock; // the game thread fills m_pending, the render thread empties it
    std::vector<PendingGeometry> m_pending;

    gpu::Shader m_vertexShader;
    gpu::Shader m_fragmentShader;
    gpu::Shader m_spotVertexShader;
    gpu::Shader m_spotFragmentShader;
    gpu::Shader m_staticVertexShader;
    gpu::Shader m_staticProjectedVertexShader;
    std::map<std::pair<BlendMode, bool>, gpu::GraphicsPipeline> m_pipelines; // by blend and depth writing
    gpu::GraphicsPipeline m_spotPipeline;
    gpu::GraphicsPipeline m_shadowPipeline;
    std::map<std::tuple<BlendMode, bool, SDL_GPUCullMode>, gpu::GraphicsPipeline> m_staticPipelines;
    std::map<std::pair<ProjectedPass, SDL_GPUCullMode>, gpu::GraphicsPipeline> m_staticProjectedPipelines;
    gpu::Texture m_depth;
    int m_depthWidth = 0;
    int m_depthHeight = 0;
    VertexStream m_vertices;
    VertexStream m_spotVertices;
    VertexStream m_shadowVertices;
    VertexStream m_instances;
    gpu::TransferBuffer m_geometryUpload; // the last upload's, released once the next one starts
};

} // namespace yamipan::rendering
