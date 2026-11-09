#include "yamipan/rendering/frame_blend.h"

#include <algorithm>
#include <cmath>

namespace yamipan::rendering
{
namespace
{

// Two frames further apart than this had a hitch between them (a load, a stall): the newer one is shown as it is.
// The ROM's frames are 40 ms apart on PAL and 60 ms when one is late.
constexpr auto kLongestBlend = std::chrono::milliseconds(100);

// A camera that moves or turns more than this in one game frame cut. The world is in centimetres: a running
// character covers about 16 a frame, a quick turn about 15 degrees.
constexpr float kCutDistance = 300.0f;
constexpr float kCutCosine = 0.766f; // cos 40 degrees

// Which way a pass looks: how its w grows with the world position (the matrix is in rows, x y z w out). A pass with
// no perspective has none.
std::optional<std::array<float, 3>> forwardOf(const ModelPass &pass)
{
    std::array<float, 3> forward{pass.viewScreen[0 * 4 + 3], pass.viewScreen[1 * 4 + 3], pass.viewScreen[2 * 4 + 3]};
    const float length = std::sqrt(forward[0] * forward[0] + forward[1] * forward[1] + forward[2] * forward[2]);
    if (!(length > 0.0f))
        return std::nullopt;
    for (float &axis : forward)
        axis /= length;
    return forward;
}

bool cameraCut(const ModelPass &before, const ModelPass &after)
{
    float distanceSquared = 0.0f;
    for (std::size_t axis = 0; axis < 3; axis++)
    {
        const float step = after.lights.eye[axis] - before.lights.eye[axis];
        distanceSquared += step * step;
    }
    if (!(distanceSquared <= kCutDistance * kCutDistance))
        return true;

    const auto from = forwardOf(before);
    const auto to = forwardOf(after);
    if (from.has_value() != to.has_value())
        return true;
    if (from && (*from)[0] * (*to)[0] + (*from)[1] * (*to)[1] + (*from)[2] * (*to)[2] < kCutCosine)
        return true;
    return false;
}

} // namespace

std::optional<float> FrameBlend::progress(const Frame &previous, const Frame &current, Clock::time_point now)
{
    if (current.number != previous.number + 1)
        return std::nullopt;
    const Clock::duration interval = current.time - previous.time;
    if (interval <= Clock::duration::zero() || interval > kLongestBlend)
        return std::nullopt;
    const double elapsed = std::chrono::duration<double>(now - current.time).count();
    const double fraction = elapsed / std::chrono::duration<double>(interval).count();
    return static_cast<float>(std::clamp(fraction, 0.0, 1.0));
}

bool FrameBlend::blendPasses(const Frame &previous, const Frame &current, float alpha, std::vector<ModelPass> &out)
{
    const std::vector<ModelPass> &before = previous.modelPasses;
    const std::vector<ModelPass> &after = current.modelPasses;
    if (after.empty() || before.size() != after.size())
        return false;
    for (std::size_t index = 0; index < after.size(); index++)
    {
        if (before[index].clearDepth != after[index].clearDepth || cameraCut(before[index], after[index]))
            return false;
    }

    // The matrix takes a world point to the screen linearly, so blending it moves every point in a straight line
    // between its two places in view space instead of along the arc a turn takes. For a quick turn (15 degrees in a
    // frame) the points keep their direction within a pixel and come at most 1 % nearer halfway. A proper rotation
    // blend needs the view and the projection passed apart through the seam.
    out.assign(after.begin(), after.end());
    const auto mix = [alpha](float from, float to) { return from + (to - from) * alpha; };
    for (std::size_t index = 0; index < out.size(); index++)
    {
        ModelPass &pass = out[index];
        const ModelPass &from = before[index];
        for (std::size_t element = 0; element < pass.viewScreen.size(); element++)
            pass.viewScreen[element] = mix(from.viewScreen[element], pass.viewScreen[element]);
        for (std::size_t axis = 0; axis < 3; axis++)
            pass.lights.eye[axis] = mix(from.lights.eye[axis], pass.lights.eye[axis]);
    }
    return true;
}

} // namespace yamipan::rendering
