// One game frame's draws, as the render thread takes them: plain data with no pointer back into game
// memory, so the game can go on while the GPU works, and the last frame can be shown again.
#pragma once

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
};

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
    std::uint32_t firstVertex = 0;
    std::uint32_t vertexCount = 0;
};

struct Frame
{
    std::vector<Vertex> vertices;
    std::vector<DrawCall> draws;
    std::uint64_t number = 0;

    void clear()
    {
        vertices.clear();
        draws.clear();
    }
};

} // namespace yamipan::rendering
