// THE source of the display's size and shape: any aspect ratio is the port's first feature, and everything
// that needs to know how the picture sits reads one PictureLayout, computed once per present. Nothing in
// host code may assume 4:3 or the game's 512x512 frame on its own.
//
// FS2 draws into a 512x512 frame that the console stretches across the screen: pixels are not square, 4:3
// and anamorphic 16:9 are the same buffer on different sets, and the game's own UI matrices already take the
// screen's shape as a parameter. So the host frame always fills the picture rect, at whatever shape that has,
// and the shape goes to the game (uiAspect) rather than being fought with bars.
#pragma once

#include "yamipan/core/config.h"

namespace yamipan::rendering
{

// The game's frame, in the units its draws use.
inline constexpr float kFrameWidth = 512.0f;
inline constexpr float kFrameHeight = 512.0f;

// What a picture of the game's own shape looks like on the set it was composed for.
inline constexpr float kFrameDisplayAspect = 4.0f / 3.0f;

struct Rect
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool operator==(const Rect &) const = default;
};

struct PictureLayout
{
    int windowWidth = 0;
    int windowHeight = 0;
    float aspect = kFrameDisplayAspect; // the composed shape: the window's in AspectMode::Auto
    Rect picture;                       // where it lands in the window, bars around it if the shapes differ
    int renderWidth = 0;                // pixels the frame is rasterised into
    int renderHeight = 0;

    bool operator==(const PictureLayout &) const = default;
};

// The picture for a window of this many pixels. Pure: the same input always gives the same layout.
PictureLayout computeLayout(int windowWidth, int windowHeight, const DisplaySettings &settings);

} // namespace yamipan::rendering
