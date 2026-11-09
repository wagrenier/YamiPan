// Smooth motion. The ROM makes a frame every two fields (25 a second on PAL) and the render thread presents more
// often than that. Between two game frames it draws the newer one with the cameras, the lights' places and every
// model's placement (its instance matrix, its bones) moved part of the way from the older frame's: the picture runs
// one game frame behind and moves at the monitor's rate. What is drawn is still the newer frame's
// (roadmap-framerate.md, option A). The effects' particles move with the blended camera and, paired by effect and
// slot, on their own. Not blended: what the game places on its own thread (the far passes' culled batches) and what it
// projects to 2D itself.
#pragma once

#include <chrono>
#include <optional>
#include <vector>

#include "yamipan/rendering/frame.h"

namespace yamipan::rendering
{

class FrameBlend
{
public:
    using Clock = std::chrono::steady_clock;

    // How far from `previous` to `current` the picture at `now` is, 0..1: the time since `current`'s field over the
    // time between the two. Nothing when they are not a pair: not consecutive, or a hitch (a load) between them.
    static std::optional<float> progress(const Frame &previous, const Frame &current, Clock::time_point now);

    // `current`'s passes with each camera (the world -> screen matrix and the eye) moved from `previous`'s by
    // `alpha`. False when the passes do not pair up, or when the camera cut: a jump or a turn no game frame makes.
    static bool blendPasses(const Frame &previous, const Frame &current, float alpha, std::vector<ModelPass> &out);

    // `current`'s instances and palettes with each one that pairs with one of `previous` (ModelPlacementKey) moved
    // from it by `alpha`. One that has no partner, or whose partner is too far off to be the same thing a frame
    // earlier, stays as it is. Answers how many matrices were blended.
    static std::size_t blendPlacements(const Frame &previous, const Frame &current, float alpha,
                                       std::vector<ModelInstance> &instances,
                                       std::vector<std::array<float, 16>> &palettes);

    // `current`'s placed vertices with each effect quad that pairs with one of `previous` (owner, slot) moved from it
    // by `alpha`: the older quad carried into the newer frame's view space through the world, then blended there.
    // blendPasses has already moved the effects' passes with the camera.
    struct QuadBlend
    {
        std::size_t blended = 0;
        float meanStep = 0.0f; // how far a blended quad's first corner moved in the frame, after the carry
    };
    static QuadBlend blendQuads(const Frame &previous, const Frame &current, float alpha,
                                std::vector<ModelVertex> &vertices);
};

} // namespace yamipan::rendering
