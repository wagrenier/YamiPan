// One game frame's draws, as the render thread takes them: plain data with no pointer back into game
// memory, so the game can go on while the GPU works, and the last frame can be shown again.
#pragma once

#include <array>
#include <compare>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "yamipan/gpu/device.h"

namespace yamipan::rendering
{

// How GS blending maps to the host's: ALPHA_1's (A - B) * C + D, by what the game actually uses.
enum class BlendMode : std::uint8_t
{
    None,
    Alpha,          // (Cs - Cd) * As + Cd
    Additive,       // Cs * As + Cd
    Subtractive,    // Cd - Cs * As
    DestinationAlpha, // (Cs - Cd) * Ad + Cd
    FixedAlpha,     // (Cs - Cd) * FIX + Cd
    FixedAdditive,  // Cs * FIX + Cd
    DestinationScale, // Cd * FIX, FIX up to 1
    DestinationBoost, // Cs * Cd + Cd; ALPHA_1 0x49, Cd * As + Cd, when the fragment hands As over as Cs
    DestinationFade, // (0 - Cd) * As + Cd
};

// ALPHA_1's selectors as the blend above.
BlendMode blendModeOf(std::uint64_t alpha);

// FRAME_1's FBMSK as the two halves the game's presets use.
enum class ColorMask : std::uint8_t
{
    All,
    ColorOnly,
    AlphaOnly,
};

struct PipelineKey
{
    BlendMode blend = BlendMode::None;
    ColorMask mask = ColorMask::All;

    auto operator<=>(const PipelineKey &) const = default;
};

// An image decoded at its source, shared by every frame that draws it, so it outlives the game's own
// texture object for as long as a frame on screen needs it.
struct Image
{
    int width = 0;
    int height = 0;
    std::vector<std::byte> pixels; // RGBA8
    std::shared_ptr<gpu::Texture> texture;
};

// What FilmOverlayPass lays over the frame: yamipan_drawui.h has the meaning of each number.
struct FilmCorner
{
    std::shared_ptr<Image> shown;
    std::shared_ptr<Image> leaving;
    float leavingAlpha = 0.0f;
};

struct FilmOverlay
{
    std::array<FilmCorner, 4> corners;
    int bandStart = 0;
    int bandLength = 1;
    int clearBelow = 0;
    int solidAbove = 0;
    std::uint32_t color = 0;
};

// One sprite of the game's frame filter (game/framefilter.cpp): a buffer, shown over `sourceWidth` x
// `sourceHeight` of it, drawn at (x, y) as `width` x `height`, kept inside 0..width x 0..height, all in
// pixels of the 512 x 512 frame. `alpha` 0x80 replaces, below it blends (Cs - Cd) * alpha / 128 + Cd.
// Buffers are the game's VRAM blocks: FrameFilter::frameBlock is the frame so far, any other a scratch
// buffer of the host's.
struct FrameFilterStep
{
    int source = 0;
    int target = 0;
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
    int sourceWidth = 0;
    int sourceHeight = 0;
    int alpha = 0x80;
};

struct FrameFilter
{
    int frameBlock = 0;
    std::vector<FrameFilterStep> steps;
};

// In frame units; vertices in texels.
struct Vertex
{
    float x, y;
    float u, v;
    std::uint32_t color; // the GS's RGBAQ bytes: 0x80 is 1.0
};
static_assert(sizeof(Vertex) == 20);

struct Scissor
{
    float x0 = 0.0f;
    float y0 = 0.0f;
    float x1 = 512.0f;
    float y1 = 512.0f;

    bool operator==(const Scissor &) const = default;
};

struct DrawCall
{
    PipelineKey pipeline;
    std::shared_ptr<Image> image; // null: untextured
    Scissor scissor;
    bool linearFilter = false;
    bool repeat = false;
    std::uint8_t alphaTestRef = 0; // fragments below it are dropped; 0 keeps them all
    std::uint8_t fixedAlpha = 0x80;
    std::shared_ptr<Image> framePalette; // not null: no draw, the frame so far goes through this 256 x 1 ramp
    std::shared_ptr<FilmOverlay> film; // not null: no draw, this goes over the frame so far
    bool frameRipple = false; // this draw's vertices are FrameRipplePass's sheet, not drawn as they are
    std::shared_ptr<FrameFilter> frameFilter; // not null: no draw, FrameFilterPass's sprites over the frame
    std::uint32_t rippleTint = 0;
    std::uint32_t firstVertex = 0;
    std::uint32_t vertexCount = 0;
};

// A model's vertex, lit and in the world: the render thread only projects it.
struct ModelVertex
{
    float x, y, z;
    float u, v;
    std::uint32_t color; // 0x80 is 1.0
};
static_assert(sizeof(ModelVertex) == 24);

// A vertex of the spot's pass: the same place, its picture projected from the light.
struct ModelSpotVertex
{
    float x, y, z;
    float s, t, q;
    std::uint32_t color; // the vertex's lit colour; alpha is how much the light reaches it, 0x80 is 1.0
};
static_assert(sizeof(ModelSpotVertex) == 28);

// A shadow's pass has the same shape: q 1, colour black, alpha how strong the shadow is here, 0x80 is 1.0.
using ModelShadowVertex = ModelSpotVertex;

// A rigid model's vertex as its file has it, kept on the GPU from the first frame that draws it: model_static.vert
// places and lights it every frame, as VU1 did, so the game thread no longer touches it.
struct ModelStaticVertex
{
    float x, y, z;
    float nx, ny, nz;            // as the file has them; a NaN one is replaced (ModelRenderer)
    std::int16_t u, v;           // 4096 = 1.0
    std::uint8_t color[4];       // r g b a, 0x80 = 1.0
};
static_assert(sizeof(ModelStaticVertex) == 32);

// What places and lights a run of cached batches: an instance of model_static.vert. The matrix is the world one
// transposed, so that each row is what one axis of the result is a dot product with.
struct ModelInstance
{
    float place[3][4];           // x' = place[0] . (x, y, z, 1); a normal takes .xyz alone, unnormalised as VU1
    float params[4];             // scroll u, scroll v (added to every UV); the reflection's scale; ModelInstanceFlag bits
    float ambient[4];            // xyz
    float lightColors[3][4];     // xyz
    float lightDirections[3][4]; // xyz
};
static_assert(sizeof(ModelInstance) == 176);

// The bits of ModelInstance::params[3], held as a float (model_static.hlsli has the same).
enum class ModelInstanceFlag : std::uint32_t
{
    Unlit = 1,     // the vertex colour as it is
    Point = 2,     // the pass's point light adds to it
    PointFlat = 4, // whichever way the vertex faces
};

// A block of GPU memory holding cached batches: vertices and the 16-bit indices of their triangles. The render thread
// creates the buffers and fills them; the frames that draw from a chunk hold it, so it outlives the cache's flush.
struct ModelGeometryChunk
{
    gpu::Buffer vertices;
    gpu::Buffer indices;
};

// One cached batch: its triangles in a chunk, and the instance that places and lights them.
struct ModelBatchDraw
{
    const ModelGeometryChunk *chunk = nullptr; // kept alive by Frame::modelChunks
    std::uint32_t firstIndex = 0;
    std::uint32_t indexCount = 0;
    std::int32_t vertexOffset = 0;
    std::uint32_t instance = 0;       // into Frame::modelInstances
    bool culled = false;              // faces turned from the eye are not drawn
    bool mirrored = false;            // the instance's matrix turns a face's front around
    bool spotted = false;             // the spot's pass goes over it
    std::uint8_t shadows = 0;         // bit n: the pass's shadow n lays itself over it
};

struct ModelDraw
{
    std::shared_ptr<Image> image; // null: untextured
    bool blended = false; // over what is drawn, by `blend`; otherwise opaque
    std::uint32_t firstVertex = 0;
    std::uint32_t vertexCount = 0;
    std::uint32_t firstSpotVertex = 0; // into Frame::modelSpotVertices, drawn over this draw
    std::uint32_t spotVertexCount = 0;
    std::uint32_t firstShadowVertex = 0; // into Frame::modelShadowVertices, drawn over this draw, before the spot
    std::uint32_t shadowVertexCount = 0;
    std::uint32_t firstBatch = 0; // cached batches, into Frame::modelBatches: each drawn, then their shadows, spots
    std::uint32_t batchCount = 0; // not 0: this draw is cached batches, and has no vertices of its own
    BlendMode blend = BlendMode::Alpha;
    int fixedFog = -1; // an effect's GS FOG, 0..255, for all it draws; -1 the pass's fog by depth
    bool writesDepth = true; // ZBUF's ZMSK clear: a material's, blended or not; not an effect's or a ghost's
    bool reflection = false; // cached batches drawn as their reflection (VU1 0x77f), not their colour
};

// What the vertex shaders take of a pass besides its projection: yamipan_drawmodel.h has what each is.
struct ModelPassLights
{
    std::array<float, 4> eye{};
    std::array<float, 4> pointPosition{};  // w: the weight
    std::array<float, 4> pointColor{};     // w: the slope
    std::array<float, 4> pointShade{};
    std::array<float, 16> spotWorldToLight{};
    std::array<float, 4> spotScale{};      // x, y; z the limit
    std::array<float, 4> spotFalloff{};
    std::array<std::array<float, 16>, 7> shadowProjections{}; // kYamiPanModelMaxShadows
};

// One pass of models, drawn in the frame's order among the 2D draws: before draws[uiDrawsBefore].
struct ModelPass
{
    std::array<float, 16> viewScreen{}; // the game's, in rows: the projection is finished at draw time
    float centerX = 2048.0f;
    float centerY = 2048.0f;
    float halfWidth = 256.0f;
    float halfHeight = 256.0f;
    float depthMin = 1.0f;
    float depthMax = 16777215.0f;
    std::array<float, 3> fogColor{};
    float fogA = 255.0f;
    float fogB = 0.0f;
    float fogNearValue = 255.0f;
    float fogFarValue = 255.0f;
    bool clearDepth = true;
    std::shared_ptr<Image> spotImage; // the lamp's picture
    std::shared_ptr<Image> shadowImage; // shadow_00
    ModelPassLights lights;
    std::uint32_t uiDrawsBefore = 0;
    std::uint32_t firstDraw = 0;
    std::uint32_t drawCount = 0;
};

struct Frame
{
    std::vector<Vertex> vertices;
    std::vector<DrawCall> draws;
    std::vector<ModelVertex> modelVertices;
    std::vector<ModelSpotVertex> modelSpotVertices;
    std::vector<ModelShadowVertex> modelShadowVertices;
    std::vector<ModelInstance> modelInstances;
    std::vector<ModelBatchDraw> modelBatches;
    std::vector<std::shared_ptr<ModelGeometryChunk>> modelChunks; // what modelBatches draw from, kept alive
    std::vector<ModelDraw> modelDraws;
    std::vector<ModelPass> modelPasses;
    std::uint64_t number = 0;

    void clear()
    {
        vertices.clear();
        draws.clear();
        modelVertices.clear();
        modelSpotVertices.clear();
        modelShadowVertices.clear();
        modelInstances.clear();
        modelBatches.clear();
        modelChunks.clear();
        modelDraws.clear();
        modelPasses.clear();
    }
};

} // namespace yamipan::rendering
