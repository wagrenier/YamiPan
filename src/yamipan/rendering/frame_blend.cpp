#include "yamipan/rendering/frame_blend.h"

#include <algorithm>
#include <cmath>
#include <unordered_map>

namespace yamipan::rendering
{
namespace
{

// Two frames further apart than this had a hitch between them (a load, a stall): the newer one is shown as it is.
// The ROM's frames are 40 ms apart on PAL and 60 ms when one is late.
constexpr auto kLongestBlend = std::chrono::milliseconds(100);

// A camera, light or part that moves or turns more than this in one game frame cut, or is not the same thing. The
// world is in centimetres: a running character covers about 16 a frame, a quick turn about 15 degrees.
constexpr float kCutDistance = 300.0f;
constexpr float kCutCosine = 0.766f;  // cos 40 degrees: a camera
constexpr float kPartCosine = 0.0f;   // cos 90 degrees: a part or a bone, which can swing fast
constexpr float kShadowChange = 0.05f; // a shadow's projection changing more than this share is another shadow

using Vec3 = std::array<float, 3>;

float dot(const Vec3 &a, const Vec3 &b)
{
    return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

float distance(const Vec3 &a, const Vec3 &b)
{
    const Vec3 step{b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    return std::sqrt(dot(step, step));
}

// An affine placement: where the three axes go, and the origin.
struct Placement
{
    std::array<Vec3, 3> axes{};
    Vec3 origin{};
};

// sceVu0 layout: a point is a row, rows 0..2 the axes, row 3 the translation.
Placement fromVu0(const float *matrix)
{
    Placement out;
    for (std::size_t axis = 0; axis < 3; axis++)
    {
        for (std::size_t lane = 0; lane < 3; lane++)
            out.axes[axis][lane] = matrix[axis * 4 + lane];
    }
    for (std::size_t lane = 0; lane < 3; lane++)
        out.origin[lane] = matrix[12 + lane];
    return out;
}

void toVu0(const Placement &placement, float *matrix)
{
    for (std::size_t axis = 0; axis < 3; axis++)
    {
        for (std::size_t lane = 0; lane < 3; lane++)
            matrix[axis * 4 + lane] = placement.axes[axis][lane];
    }
    for (std::size_t lane = 0; lane < 3; lane++)
        matrix[12 + lane] = placement.origin[lane];
}

// ModelInstance::place is the same matrix with rows and columns swapped (model_renderer.cpp, drawCachedBatch).
Placement fromInstance(const ModelInstance &instance)
{
    Placement out;
    for (std::size_t axis = 0; axis < 3; axis++)
    {
        for (std::size_t lane = 0; lane < 3; lane++)
            out.axes[axis][lane] = instance.place[lane][axis];
    }
    for (std::size_t lane = 0; lane < 3; lane++)
        out.origin[lane] = instance.place[lane][3];
    return out;
}

void toInstance(const Placement &placement, ModelInstance &instance)
{
    for (std::size_t axis = 0; axis < 3; axis++)
    {
        for (std::size_t lane = 0; lane < 3; lane++)
            instance.place[lane][axis] = placement.axes[axis][lane];
    }
    for (std::size_t lane = 0; lane < 3; lane++)
        instance.place[lane][3] = placement.origin[lane];
}

// Whether two placements a frame apart can be the same part: near, and no axis turned past a right angle or
// changed its length by half.
bool samePart(const Placement &before, const Placement &after)
{
    if (!(distance(before.origin, after.origin) <= kCutDistance))
        return false;
    for (std::size_t axis = 0; axis < 3; axis++)
    {
        const float from = std::sqrt(dot(before.axes[axis], before.axes[axis]));
        const float to = std::sqrt(dot(after.axes[axis], after.axes[axis]));
        if (from == 0.0f && to == 0.0f)
            continue;
        if (!(from > 0.0f && to > 0.0f) || to > from * 2.0f || from > to * 2.0f)
            return false;
        if (dot(before.axes[axis], after.axes[axis]) < kPartCosine * from * to)
            return false;
    }
    return true;
}

// The axes blended and put back to their blended length, so a part that turns keeps its size halfway (a plain blend
// of two turned axes is shorter than either); the origin in a straight line.
Placement mix(const Placement &before, const Placement &after, float alpha)
{
    Placement out;
    for (std::size_t axis = 0; axis < 3; axis++)
    {
        Vec3 &blended = out.axes[axis];
        for (std::size_t lane = 0; lane < 3; lane++)
            blended[lane] = before.axes[axis][lane] + (after.axes[axis][lane] - before.axes[axis][lane]) * alpha;
        const float from = std::sqrt(dot(before.axes[axis], before.axes[axis]));
        const float to = std::sqrt(dot(after.axes[axis], after.axes[axis]));
        const float length = std::sqrt(dot(blended, blended));
        if (length > 0.0f)
        {
            const float scale = (from + (to - from) * alpha) / length;
            for (float &lane : blended)
                lane *= scale;
        }
    }
    for (std::size_t lane = 0; lane < 3; lane++)
        out.origin[lane] = before.origin[lane] + (after.origin[lane] - before.origin[lane]) * alpha;
    return out;
}

// Where a world -> local placement (sceVu0 layout) puts the local origin in the world: p with p * axes + origin = 0,
// by Cramer's rule. Nothing for a flat one.
std::optional<Vec3> worldOriginOf(const float *matrix)
{
    double m[3][3]; // m[j][k]: unknown k's weight in equation j
    double right[3];
    for (int j = 0; j < 3; j++)
    {
        for (int k = 0; k < 3; k++)
            m[j][k] = matrix[k * 4 + j];
        right[j] = -static_cast<double>(matrix[12 + j]);
    }
    const auto determinant = [](const double (&a)[3][3]) {
        return a[0][0] * (a[1][1] * a[2][2] - a[1][2] * a[2][1]) - a[0][1] * (a[1][0] * a[2][2] - a[1][2] * a[2][0]) +
               a[0][2] * (a[1][0] * a[2][1] - a[1][1] * a[2][0]);
    };
    const double whole = determinant(m);
    if (!(std::abs(whole) > 1e-12))
        return std::nullopt;
    Vec3 out{};
    for (int k = 0; k < 3; k++)
    {
        double replaced[3][3];
        for (int j = 0; j < 3; j++)
        {
            for (int column = 0; column < 3; column++)
                replaced[j][column] = column == k ? right[j] : m[j][column];
        }
        out[static_cast<std::size_t>(k)] = static_cast<float>(determinant(replaced) / whole);
    }
    return out;
}

// Which way a pass looks: how its w grows with the world position (the matrix is in rows, x y z w out). A pass with
// no perspective has none.
std::optional<Vec3> forwardOf(const ModelPass &pass)
{
    Vec3 forward{pass.viewScreen[0 * 4 + 3], pass.viewScreen[1 * 4 + 3], pass.viewScreen[2 * 4 + 3]};
    const float length = std::sqrt(dot(forward, forward));
    if (!(length > 0.0f))
        return std::nullopt;
    for (float &axis : forward)
        axis /= length;
    return forward;
}

bool cameraCut(const ModelPass &before, const ModelPass &after)
{
    const Vec3 from{before.lights.eye[0], before.lights.eye[1], before.lights.eye[2]};
    const Vec3 to{after.lights.eye[0], after.lights.eye[1], after.lights.eye[2]};
    if (!(distance(from, to) <= kCutDistance))
        return true;

    const auto lookFrom = forwardOf(before);
    const auto lookTo = forwardOf(after);
    if (lookFrom.has_value() != lookTo.has_value())
        return true;
    return lookFrom && dot(*lookFrom, *lookTo) < kCutCosine;
}

template <std::size_t N>
void mixInto(std::array<float, N> &out, const std::array<float, N> &before, float alpha, std::size_t count = N)
{
    for (std::size_t element = 0; element < count; element++)
        out[element] = before[element] + (out[element] - before[element]) * alpha;
}

// A shadow's world -> picture matrix a frame apart: the same shadow when no element moved by more than a small share
// of the largest. Another character's is far off in its translation.
bool sameShadow(const std::array<float, 16> &before, const std::array<float, 16> &after)
{
    float largest = 0.0f;
    float change = 0.0f;
    for (std::size_t element = 0; element < 16; element++)
    {
        largest = std::max({largest, std::abs(before[element]), std::abs(after[element])});
        change = std::max(change, std::abs(after[element] - before[element]));
    }
    return largest > 0.0f && change <= largest * kShadowChange;
}

// The pass's lights that follow things: the point light, the spot (the lamp a character holds), the shadows.
void mixLights(ModelPassLights &out, const ModelPassLights &before, float alpha)
{
    const Vec3 pointFrom{before.pointPosition[0], before.pointPosition[1], before.pointPosition[2]};
    const Vec3 pointTo{out.pointPosition[0], out.pointPosition[1], out.pointPosition[2]};
    if (distance(pointFrom, pointTo) <= kCutDistance)
        mixInto(out.pointPosition, before.pointPosition, alpha, 3);

    const auto spotFrom = worldOriginOf(before.spotWorldToLight.data());
    const auto spotTo = worldOriginOf(out.spotWorldToLight.data());
    if (spotFrom && spotTo && distance(*spotFrom, *spotTo) <= kCutDistance && before.spotScale == out.spotScale)
        mixInto(out.spotWorldToLight, before.spotWorldToLight, alpha);

    for (std::size_t shadow = 0; shadow < out.shadowProjections.size(); shadow++)
    {
        if (sameShadow(before.shadowProjections[shadow], out.shadowProjections[shadow]))
            mixInto(out.shadowProjections[shadow], before.shadowProjections[shadow], alpha);
    }
}

// The n-th placement of one key in a frame.
struct Slot
{
    ModelPlacementKey key;
    std::uint32_t occurrence = 0;

    bool operator==(const Slot &) const = default;
};

struct SlotHash
{
    std::size_t operator()(const Slot &slot) const
    {
        const std::hash<const void *> pointer;
        std::size_t hash = pointer(slot.key.owner);
        hash = hash * 31 + pointer(slot.key.geometry);
        return hash * 31 + slot.occurrence;
    }
};

struct KeyHash
{
    std::size_t operator()(const ModelPlacementKey &key) const
    {
        const std::hash<const void *> pointer;
        return pointer(key.owner) * 31 + pointer(key.geometry);
    }
};

// Each keyed entry's slot, in order.
template <class Entry, class KeyOf>
std::vector<Slot> slotsOf(const std::vector<Entry> &entries, KeyOf keyOf)
{
    std::unordered_map<ModelPlacementKey, std::uint32_t, KeyHash> seen;
    std::vector<Slot> slots;
    slots.reserve(entries.size());
    for (const Entry &entry : entries)
    {
        const ModelPlacementKey key = keyOf(entry);
        slots.push_back({key, seen[key]++});
    }
    return slots;
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
    for (std::size_t index = 0; index < out.size(); index++)
    {
        ModelPass &pass = out[index];
        const ModelPass &from = before[index];
        mixInto(pass.viewScreen, from.viewScreen, alpha);
        mixInto(pass.lights.eye, from.lights.eye, alpha, 3);
        mixLights(pass.lights, from.lights, alpha);
    }
    return true;
}

std::size_t FrameBlend::blendPlacements(const Frame &previous, const Frame &current, float alpha,
                                        std::vector<ModelInstance> &instances,
                                        std::vector<std::array<float, 16>> &palettes)
{
    instances.assign(current.modelInstances.begin(), current.modelInstances.end());
    palettes.assign(current.modelPalettes.begin(), current.modelPalettes.end());
    std::size_t blended = 0;

    // The parts: an instance's matrix places a rigid part (a skinned one's is the unit, its palette places it).
    if (previous.modelInstanceKeys.size() == previous.modelInstances.size() &&
        current.modelInstanceKeys.size() == current.modelInstances.size())
    {
        const auto identity = [](const ModelPlacementKey &key) { return key; };
        const std::vector<Slot> earlierSlots = slotsOf(previous.modelInstanceKeys, identity);
        std::unordered_map<Slot, std::size_t, SlotHash> earlier;
        for (std::size_t index = 0; index < earlierSlots.size(); index++)
            earlier.emplace(earlierSlots[index], index);

        const std::vector<Slot> slots = slotsOf(current.modelInstanceKeys, identity);
        for (std::size_t index = 0; index < slots.size(); index++)
        {
            if (slots[index].key.owner == nullptr)
                continue;
            const auto found = earlier.find(slots[index]);
            if (found == earlier.end())
                continue;
            const Placement from = fromInstance(previous.modelInstances[found->second]);
            const Placement to = fromInstance(instances[index]);
            if (!samePart(from, to))
                continue;
            toInstance(mix(from, to, alpha), instances[index]);
            blended++;
        }
    }

    // The bones: a palette run pairs with the earlier frame's run of the same key and length, matrix by matrix.
    const auto runKey = [](const ModelPaletteRun &run) { return run.key; };
    const std::vector<Slot> earlierSlots = slotsOf(previous.modelPaletteRuns, runKey);
    std::unordered_map<Slot, std::size_t, SlotHash> earlier;
    for (std::size_t index = 0; index < earlierSlots.size(); index++)
        earlier.emplace(earlierSlots[index], index);

    const std::vector<Slot> slots = slotsOf(current.modelPaletteRuns, runKey);
    for (std::size_t index = 0; index < slots.size(); index++)
    {
        const ModelPaletteRun &run = current.modelPaletteRuns[index];
        if (run.key.owner == nullptr)
            continue;
        const auto found = earlier.find(slots[index]);
        if (found == earlier.end())
            continue;
        const ModelPaletteRun &before = previous.modelPaletteRuns[found->second];
        if (before.count != run.count || before.first + before.count > previous.modelPalettes.size() ||
            run.first + run.count > palettes.size())
            continue;
        for (std::uint32_t bone = 0; bone < run.count; bone++)
        {
            const Placement from = fromVu0(previous.modelPalettes[before.first + bone].data());
            float *const matrix = palettes[run.first + bone].data();
            const Placement to = fromVu0(matrix);
            if (!samePart(from, to))
                continue;
            toVu0(mix(from, to, alpha), matrix);
            blended++;
        }
    }
    return blended;
}

} // namespace yamipan::rendering
