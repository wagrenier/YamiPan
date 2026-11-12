#include "yamipan/rendering/display_geometry.h"

#include <algorithm>
#include <cmath>

namespace yamipan::rendering
{
namespace
{

// SDL_GPU has no query for this; 16384 is the floor across every backend.
constexpr int kMaxTextureDimension = 16384;

float composedAspect(int windowWidth, int windowHeight, const DisplaySettings &settings)
{
    switch (settings.aspectMode)
    {
    case AspectMode::Ratio4x3:
        return 4.0f / 3.0f;
    case AspectMode::Ratio16x9:
        return 16.0f / 9.0f;
    case AspectMode::Ratio16x10:
        return 16.0f / 10.0f;
    case AspectMode::Ratio21x9:
        return 21.0f / 9.0f;
    case AspectMode::Custom:
        return settings.aspectRatio;
    case AspectMode::Auto:
        break;
    }
    return windowHeight > 0 ? static_cast<float>(windowWidth) / static_cast<float>(windowHeight)
                            : kFrameDisplayAspect;
}

// The largest rect of this shape inside the window, centred. One function places the picture and shapes
// the render target, so the two can never disagree.
Rect fitted(int windowWidth, int windowHeight, float aspect)
{
    double width = windowWidth;
    double height = width / aspect;
    if (height > windowHeight)
    {
        height = windowHeight;
        width = height * aspect;
    }

    Rect rect;
    rect.width = static_cast<int>(std::lround(width));
    rect.height = static_cast<int>(std::lround(height));
    rect.x = (windowWidth - rect.width) / 2;
    rect.y = (windowHeight - rect.height) / 2;
    return rect;
}

} // namespace

PictureLayout computeLayout(int windowWidth, int windowHeight, const DisplaySettings &settings)
{
    PictureLayout layout;
    layout.windowWidth = windowWidth;
    layout.windowHeight = windowHeight;
    layout.aspect = composedAspect(windowWidth, windowHeight, settings);

    // Auto skips the fit: it would round, come back a pixel short and open a hairline bar.
    layout.picture = settings.aspectMode == AspectMode::Auto ? Rect{0, 0, windowWidth, windowHeight}
                                                             : fitted(windowWidth, windowHeight, layout.aspect);

    double width = layout.picture.width;
    double height = layout.picture.height;
    switch (settings.renderMode)
    {
    case RenderResolution::NativePs2:
        // The frame's lines (more on a shape taller than 4:3), at the composed shape: PS2 pixel density
        // on any window.
        height = kFrameHeight * std::max(1.0, static_cast<double>(kFrameDisplayAspect) / layout.aspect) *
                 settings.renderScale;
        width = height * layout.aspect;
        break;
    case RenderResolution::WindowScale:
        width *= settings.renderScale;
        height *= settings.renderScale;
        break;
    case RenderResolution::MatchWindow:
        break;
    }

    layout.renderWidth = std::clamp(static_cast<int>(std::lround(width)), 1, kMaxTextureDimension);
    layout.renderHeight = std::clamp(static_cast<int>(std::lround(height)), 1, kMaxTextureDimension);
    return layout;
}

} // namespace yamipan::rendering
