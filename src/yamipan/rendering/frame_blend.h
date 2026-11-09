// Smooth motion, step one: the camera. The ROM makes a frame every two fields (25 a second on PAL) and the render
// thread presents more often than that. Between two game frames it draws the newer one with each model pass's
// camera moved part of the way from the older frame's: the picture runs one game frame behind and the world turns
// and slides at the monitor's rate. What is drawn, and where the objects stand, is still the newer frame's
// (roadmap-framerate.md, option A; the objects and bones are its next step).
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
};

} // namespace yamipan::rendering
