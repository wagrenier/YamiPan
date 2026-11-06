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
    DestinationBoost, // Cs * Cd + Cd
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

struct ModelDraw
{
    std::shared_ptr<Image> image; // null: untextured
    bool blended = false; // over what is drawn, no depth written, by `blend`; otherwise opaque
    std::uint32_t firstVertex = 0;
    std::uint32_t vertexCount = 0;
    std::uint32_t firstSpotVertex = 0; // into Frame::modelSpotVertices, drawn over this draw
    std::uint32_t spotVertexCount = 0;
    std::uint32_t firstShadowVertex = 0; // into Frame::modelShadowVertices, drawn over this draw, before the spot
    std::uint32_t shadowVertexCount = 0;
    BlendMode blend = BlendMode::Alpha;
    int fixedFog = -1; // an effect's GS FOG, 0..255, for all it draws; -1 the pass's fog by depth
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
        modelDraws.clear();
        modelPasses.clear();
    }
};

} // namespace yamipan::rendering
