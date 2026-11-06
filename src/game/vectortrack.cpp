/* ==========================================================================
 *  game/vectortrack.cpp
 *
 *  VectorTrack, VectorKey and VectorFade (game/gamehooks.h): keyed vectors,
 *  0x299d90..0x29a730, cut out of the run unassigned@00299d90, and the fade
 *  0x296c10..0x296d00 beside them. The file name is the port's. SceneRender's
 *  tracks are linear; the lights' colour tracks are Bezier curves, whose
 *  control points computeControls works out from the neighbouring keys.
 * ======================================================================== */
#include <string.h>

#include "game/gamehooks.h"
#include "game/vu0math.h"

// FS2 0x00296c10
// callcheck: scaleVec4

void VectorFade::step()
{
    // PC: the ROM's 0 / 0 is the FPU's largest float; nothing asks for it
    // once a set has keys (the transition's frames are 30).
    const float t = frames > 0 ? (float)frame / (float)frames : 1.0f;
    for (int lane = 0; lane < 4; lane++)
        value[lane] = from[lane] + (to[lane] - from[lane]) * t;
    if (frame < frames)
        frame++;
}

// FS2 0x00299d90
// callcheck: sample (keyAt inline in the ROM)

void VectorTrack::step()
{
    constexpr float kPi = 3.1415927f;
    constexpr float kTwoPi = 6.2831855f;

    if (count < 2) {
        done = 1;
        return;
    }

    VectorKey *key = keyAt(index);
    frame++;
    if (key->frames <= frame) {
        if (index < count - 2) {
            index++;
            frame = 0;
            key = key->next;
        } else if (!loop) {
            done = 1;
            frame = key->frames;
        } else {
            index = 0;
            frame = 0;
            key = keys;
        }
    }
    sample(key);

    if (curve == 2) {
        for (int lane = 0; lane < 3; lane++) {
            while (kPi < value[lane])
                value[lane] -= kTwoPi;
            while (value[lane] < -kPi)
                value[lane] += kTwoPi;
        }
    }
}

// FS2 0x00299f40
// callcheck: sample

void VectorTrack::reset()
{
    frame = 0;
    index = 0;
    done = 0;
    if (count > 1)
        sample(keys);
}

// FS2 0x00299f90
// callcheck: scaleVec4, addScaledVec4, addScaledVec4, addScaledVec4, scaleVec4, addScaledVec3, addScaledVec3, addScaledVec3, scaleVec4

void VectorTrack::sample(VectorKey *key)
{
    const VectorKey *next = key->next;
    const float t = (float)frame / (float)key->frames;

    if (linear) {
        for (int lane = 0; lane < 4; lane++)
            value[lane] = key->value[lane] + (next->value[lane] - key->value[lane]) * t;
        return;
    }

    const float u = 1.0f - t;
    scaleVec4(u * u * u, value, key->value);
    if (curve == 1) {
        addScaledVec4(t * t * t, value, value, next->value);
        addScaledVec4(t * u * 3.0f * u, value, value, key->ctrlOut);
        addScaledVec4(t * u * 3.0f * t, value, value, next->ctrlIn);
    } else {
        addScaledVec3(t * t * t, value, value, next->value);
        addScaledVec3(t * u * 3.0f * u, value, value, key->ctrlOut);
        addScaledVec3(t * u * 3.0f * t, value, value, next->ctrlIn);
        value[3] = 1.0f;
    }
}

// a + dir * s, all four lanes.
static inline void pointAlong(sceVu0FVECTOR out, const sceVu0FVECTOR a, const sceVu0FVECTOR dir, float s)
{
    addScaledVec4(s, out, a, dir);
}

// FS2 0x0029a160
// callcheck: divideVec4, vecDirection, vecDirection, vecDistance, scaleVec4, vecDirection, vecDistance, scaleVec4, vecDistance, scaleVec4, vecDirection, vecDistance, scaleVec4 (divideVec4 and scaleVec4 inside pointAlong)

void VectorTrack::computeControls()
{
    constexpr float kThird = 3.0f;

    if (count < 2)
        return;

    sceVu0FVECTOR dir;
    for (VectorKey *key = keys; key != nullptr; key = key->next) {
        if (key->prev == nullptr) {
            for (int lane = 0; lane < 4; lane++)
                key->ctrlOut[lane] = key->value[lane] + (key->next->value[lane] - key->value[lane]) / kThird;
        }
        if (key->next == nullptr) {
            VectorKey *prev = key->prev;
            if (prev->prev == nullptr)
                vecDirection(dir, prev->value, key->value);
            else
                vecDirection(dir, prev->ctrlOut, key->value);
            pointAlong(key->ctrlIn, key->value, dir, vecDistance(prev->value, key->value) / kThird);
        } else if (key->prev != nullptr) {
            VectorKey *prev = key->prev;
            vecDirection(dir, key->next->value, prev->value);
            pointAlong(key->ctrlOut, key->value, dir, vecDistance(key->next->value, key->value) / kThird);
            pointAlong(key->ctrlIn, key->value, dir, -(vecDistance(prev->value, key->value) / kThird));
            if (prev->prev == nullptr) {
                vecDirection(dir, key->ctrlIn, prev->value);
                pointAlong(prev->ctrlOut, prev->value, dir, vecDistance(prev->value, key->value) / kThird);
            }
        }
    }
}

// FS2 0x0029a370
// callcheck: append, setTime

void VectorTrack::addKey(VectorKey *key, int time, bool computeFrames)
{
    append(key, computeFrames);
    key->setTime(time);
}

// FS2 0x0029a3b0
// callcheck: vecDistance, computeControls

void VectorTrack::append(VectorKey *key, bool computeFrames)
{
    key->next = nullptr;
    key->prev = nullptr;
    if (keys == nullptr) {
        keys = key;
    } else {
        VectorKey *tail = keys;
        while (tail->next != nullptr)
            tail = tail->next;
        tail->next = key;
        key->prev = tail;
        if (computeFrames && tail->frames == 0) {
            tail->frames = (int)(vecDistance(key->value, tail->value) / 4.0f);
            if (tail->frames < 1)
                tail->frames = 1;
        }
    }
    count++;
    if (computeFrames)
        computeControls();
}

// FS2 0x0029a4c0
// callcheck: none

VectorKey *VectorTrack::keyAt(int keyIndex)
{
    VectorKey *key = keys;
    for (int i = 0; i < keyIndex; i++) {
        if (key == nullptr)
            return nullptr;
        key = key->next;
    }
    return key;
}

// FS2 0x0029a5b0
// callcheck: none

void VectorKey::setTime(int time)
{
    if (time < 0)
        return;

    int now = 0;
    for (const VectorKey *key = prev; key != nullptr; key = key->prev)
        now += key->frames;
    if (now == time || prev == nullptr)
        return;

    if (time < now) {
        const int earlier = now - time;
        if (earlier < prev->frames) {
            prev->frames -= earlier;
            if (next != nullptr)
                frames += earlier;
        } else {
            if (next != nullptr)
                frames += prev->frames - 1;
            prev->frames = 1;
        }
        return;
    }

    const int later = time - now;
    if (later < frames) {
        prev->frames += later;
        frames -= later;
    } else if (next == nullptr) {
        prev->frames += later;
    } else {
        prev->frames += frames - 1;
        frames = 1;
    }
}
