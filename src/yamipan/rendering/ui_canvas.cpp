#include "yamipan/rendering/ui_canvas.h"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

#include "yamipan/rendering/display_geometry.h"
#include "yamipan/rendering/renderer.h"
#include "yamipan/rendering/yamipan_drawui.h"

namespace yamipan::rendering
{
namespace
{

// The GS registers that decide anything here (sce_gs.h has them as macros for the game).
constexpr int kRegClamp1 = 0x08;
constexpr int kRegTex11 = 0x14;
constexpr int kRegScissor1 = 0x40;
constexpr int kRegAlpha1 = 0x42;
constexpr int kRegTest1 = 0x47;
constexpr int kRegFrame1 = 0x4c;

// PRIM's bits.
constexpr std::uint64_t kPrimTypeMask = 7;
constexpr std::uint64_t kPrimGouraud = 1 << 3;
constexpr std::uint64_t kPrimTextured = 1 << 4;
constexpr std::uint64_t kPrimBlended = 1 << 6;

// The draw env's window: coordinates are 12.4, centred on 2048, and the frame is 512 wide around that.
constexpr float kWindowUnit = 16.0f;
constexpr float kWindowOrigin = 2048.0f - kFrameWidth / 2.0f;
constexpr float kTexelUnit = 16.0f;
constexpr float kSpriteUvInset = 1.0f / 64.0f; // texels

constexpr std::uint64_t kScissorMask = 0x7ff;
constexpr std::uint32_t kFbmskRgb = 0x00ffffff;
constexpr std::uint32_t kFbmskAlpha = 0xff000000;

// ALPHA_1's selectors: A, B and D pick Cs, Cd or 0, C picks As, Ad or FIX.
constexpr int kSourceColor = 0;
constexpr int kDestinationColor = 1;
constexpr int kZero = 2;
constexpr int kSourceAlpha = 0;
constexpr int kDestinationAlpha = 1;
constexpr int kFixedAlpha = 2;

ColorMask colorMaskOf(std::uint64_t frame)
{
    const auto mask = static_cast<std::uint32_t>(frame >> 32);
    if (mask == kFbmskRgb)
        return ColorMask::AlphaOnly;
    if (mask == kFbmskAlpha)
        return ColorMask::ColorOnly;
    return ColorMask::All;
}

Scissor scissorOf(std::uint64_t value)
{
    const auto bound = [value](int shift) { return static_cast<float>((value >> shift) & kScissorMask); };
    return {bound(0), bound(32), bound(16) + 1.0f, bound(48) + 1.0f};
}

// x about the frame's centre, by a squeeze (1: as it is).
float squeezedX(float x, float squeeze)
{
    return kFrameWidth / 2.0f + (x - kFrameWidth / 2.0f) * squeeze;
}

// The clip of squeezed vertices follows them. Framed, the whole frame's clip becomes the frame as composed;
// otherwise a clip that spans the frame's width (the draw env's own) stays the whole display.
Scissor squeezed(Scissor scissor, float squeeze, bool framed)
{
    constexpr float kSlack = 1.0f;
    if (squeeze == 1.0f || (!framed && scissor.x0 <= kSlack && scissor.x1 >= kFrameWidth - kSlack))
        return scissor;
    scissor.x0 = squeezedX(scissor.x0, squeeze);
    scissor.x1 = squeezedX(scissor.x1, squeeze);
    return scissor;
}

// TEST_1: ATE, and ATST GEQUAL / GREATER against AREF are all a dropped fragment can be here.
std::uint8_t alphaTestRefOf(std::uint64_t test)
{
    constexpr std::uint64_t kAlphaTestEnable = 1;
    constexpr int kTestGequal = 5;
    constexpr int kTestGreater = 6;

    if ((test & kAlphaTestEnable) == 0)
        return 0;
    const int method = static_cast<int>((test >> 1) & 7);
    if (method != kTestGequal && method != kTestGreater)
        return 0;
    return static_cast<std::uint8_t>((test >> 4) & 0xff);
}

} // namespace

BlendMode blendModeOf(std::uint64_t alpha)
{
    const int a = static_cast<int>(alpha & 3);
    const int b = static_cast<int>((alpha >> 2) & 3);
    const int c = static_cast<int>((alpha >> 4) & 3);
    const int d = static_cast<int>((alpha >> 6) & 3);

    if (a == kSourceColor && b == kDestinationColor && d == kDestinationColor)
    {
        if (c == kDestinationAlpha)
            return BlendMode::DestinationAlpha;
        return c == kFixedAlpha ? BlendMode::FixedAlpha : BlendMode::Alpha;
    }
    if (a == kSourceColor && b == kZero && d == kDestinationColor)
        return c == kFixedAlpha ? BlendMode::FixedAdditive : BlendMode::Additive;
    if (a == kZero && b == kSourceColor && d == kDestinationColor && c == kSourceAlpha)
        return BlendMode::Subtractive;
    if (a == kZero && b == kDestinationColor && d == kDestinationColor && c == kSourceAlpha)
        return BlendMode::DestinationFade;
    // Cd * As + Cd, the effects' brightening (0x49). The blend multiplies by the colour: model.frag gives it As.
    if (a == kDestinationColor && b == kZero && d == kDestinationColor && c == kSourceAlpha)
        return BlendMode::DestinationBoost;
    return BlendMode::Alpha;
}

UiCanvas::UiCanvas(Renderer &renderer) : m_renderer(renderer) {}

UiCanvas::Layer &UiCanvas::layer(const void *list)
{
    return m_layers[list];
}

void UiCanvas::setRegister(const void *list, int reg, std::uint64_t value)
{
    std::scoped_lock lock(m_lock);
    Layer &target = layer(list);
    switch (reg)
    {
    case kRegAlpha1:
        target.alpha = value;
        break;
    case kRegClamp1:
        target.clamp = value;
        break;
    case kRegTex11:
        target.tex1 = value;
        break;
    case kRegScissor1:
        target.scissor = scissorOf(value);
        break;
    case kRegTest1:
        target.test = value;
        break;
    case kRegFrame1:
        target.frame = value;
        break;
    default:
        break;
    }
}

void UiCanvas::setPrimitive(const void *list, std::uint64_t prim, const std::uint8_t *sharedColor)
{
    std::scoped_lock lock(m_lock);
    Layer &target = layer(list);
    target.primitive = static_cast<Primitive>(prim & kPrimTypeMask);
    target.gouraud = (prim & kPrimGouraud) != 0;
    target.textured = (prim & kPrimTextured) != 0;
    target.blended = (prim & kPrimBlended) != 0;
    target.hasSharedColor = sharedColor != nullptr;
    if (sharedColor != nullptr)
    {
        target.sharedColor = static_cast<std::uint32_t>(sharedColor[0]) |
                             static_cast<std::uint32_t>(sharedColor[1]) << 8 |
                             static_cast<std::uint32_t>(sharedColor[2]) << 16 |
                             static_cast<std::uint32_t>(sharedColor[3]) << 24;
    }
    target.run.clear();
    target.runCount = 0;
}

void UiCanvas::bindImage(const void *list, std::shared_ptr<Image> image)
{
    std::scoped_lock lock(m_lock);
    layer(list).image = std::move(image);
}

float UiCanvas::squeezeFactor() const
{
    const float wider = m_renderer.layout().aspect / kFrameDisplayAspect;
    return wider > 1.0f ? 1.0f / wider : 1.0f;
}

void UiCanvas::setSqueezed(const void *list, Squeeze squeeze)
{
    // Read before the canvas's lock: the renderer has its own.
    const float factor = squeezeFactor();
    std::scoped_lock lock(m_lock);
    Layer &target = layer(list);
    target.squeeze = squeeze != Squeeze::Off ? factor : 1.0f;
    target.framed = squeeze == Squeeze::Framed;
}

void UiCanvas::begin(const void *list, int format)
{
    std::scoped_lock lock(m_lock);
    Layer &target = layer(list);
    target.format = format;
    target.run.clear();
    target.runCount = 0;
}

void UiCanvas::addVertex(const void *list, const YamiPanUiVertex &vertex)
{
    std::scoped_lock lock(m_lock);
    Layer &target = layer(list);

    Vertex converted{};
    converted.x = squeezedX(static_cast<float>(vertex.x) / kWindowUnit - kWindowOrigin, target.squeeze);
    converted.y = static_cast<float>(vertex.y) / kWindowUnit - kWindowOrigin;
    if ((target.format & YAMIPAN_UIV_UV) != 0)
    {
        converted.u = static_cast<float>(vertex.u) / kTexelUnit;
        converted.v = static_cast<float>(vertex.v) / kTexelUnit;
    }
    if ((target.format & YAMIPAN_UIV_RGBAQ) != 0)
    {
        converted.color = static_cast<std::uint32_t>(vertex.rgba[0]) |
                          static_cast<std::uint32_t>(vertex.rgba[1]) << 8 |
                          static_cast<std::uint32_t>(vertex.rgba[2]) << 16 |
                          static_cast<std::uint32_t>(vertex.rgba[3]) << 24;
    }
    else
    {
        converted.color = target.sharedColor;
    }
    assemble(target, converted);
}

DrawCall UiCanvas::stateOf(const Layer &source)
{
    DrawCall draw;
    draw.pipeline.blend = source.forcedBlend.value_or(source.blended ? blendModeOf(source.alpha) : BlendMode::None);
    draw.pipeline.mask = colorMaskOf(source.frame);
    draw.image = source.textured ? source.image : nullptr;
    draw.scissor = squeezed(source.scissor, source.squeeze, source.framed);
    draw.linearFilter = ((source.tex1 >> 5) & 1) != 0;
    draw.repeat = (source.clamp & 3) == 0;
    draw.alphaTestRef = alphaTestRefOf(source.test);
    draw.fixedAlpha = static_cast<std::uint8_t>(source.alpha >> 32);
    return draw;
}

void UiCanvas::emit(Layer &target, std::initializer_list<Vertex> triangleVertices)
{
    // No picture for a textured primitive (a render target, for now): drawing nothing beats a white sheet.
    if (target.textured && target.image == nullptr)
        return;

    DrawCall state = stateOf(target);

    const bool continues =
        !target.draws.empty() && [&] {
            const DrawCall &last = target.draws.back();
            return last.framePalette == nullptr && last.film == nullptr && !last.frameRipple &&
                   last.frameFilter == nullptr &&
                   last.pipeline == state.pipeline &&
                   last.image == state.image &&
                   last.scissor == state.scissor &&
                   last.linearFilter == state.linearFilter && last.repeat == state.repeat &&
                   last.alphaTestRef == state.alphaTestRef && last.fixedAlpha == state.fixedAlpha;
        }();

    if (!continues)
    {
        state.firstVertex = static_cast<std::uint32_t>(target.vertices.size());
        target.draws.push_back(std::move(state));
    }

    target.vertices.insert(target.vertices.end(), triangleVertices);
    target.draws.back().vertexCount += static_cast<std::uint32_t>(triangleVertices.size());
}

// The GS kicks a primitive on the vertex that completes it, whatever kind it is.
void UiCanvas::assemble(Layer &target, const Vertex &vertex)
{
    std::vector<Vertex> &run = target.run;
    run.push_back(vertex);
    target.runCount++;

    const auto flat = [&target](Vertex shaded, const Vertex &last) {
        if (!target.gouraud)
            shaded.color = last.color;
        return shaded;
    };

    const bool grouped = (target.format & YAMIPAN_UIV_GROUPED) != 0;
    switch (target.primitive)
    {
    case Primitive::Triangle:
        if (run.size() == 3)
        {
            emit(target, {flat(run[0], run[2]), flat(run[1], run[2]), run[2]});
            run.clear();
        }
        break;

    case Primitive::TriangleStrip:
        if (run.size() == 3)
        {
            emit(target, {flat(run[0], run[2]), flat(run[1], run[2]), run[2]});
            if (grouped)
                run.clear();
            else
                run.erase(run.begin());
        }
        break;

    case Primitive::TriangleFan:
        if (run.size() == 3)
        {
            emit(target, {flat(run[0], run[2]), flat(run[1], run[2]), run[2]});
            run.erase(run.begin() + 1);
        }
        break;

    case Primitive::Sprite:
        if (run.size() == 2)
        {
            const Vertex &a = run[0];
            const Vertex &b = run[1];
            Vertex topLeft = a;
            Vertex topRight = a;
            Vertex bottomLeft = a;
            Vertex bottomRight = b;
            topLeft.color = topRight.color = bottomLeft.color = b.color;
            topRight.x = b.x;
            bottomLeft.y = b.y;
            // A pixel centre can fall exactly on a sprite's edge at some render sizes: pulled in by a hair,
            // its texel stays this sprite's and not the next one's on the sheet.
            const float insetU = std::copysign(kSpriteUvInset, b.u - a.u);
            const float insetV = std::copysign(kSpriteUvInset, b.v - a.v);
            topLeft.u = bottomLeft.u = a.u + insetU;
            topRight.u = bottomRight.u = b.u - insetU;
            topLeft.v = topRight.v = a.v + insetV;
            bottomLeft.v = bottomRight.v = b.v - insetV;
            emit(target, {topLeft, topRight, bottomLeft, topRight, bottomRight, bottomLeft});
            run.clear();
        }
        break;

    case Primitive::Line:
    case Primitive::LineStrip:
        if (run.size() == 2)
        {
            // One frame unit wide, along whichever axis the line runs less.
            const Vertex &a = run[0];
            const Vertex &b = run[1];
            const bool steep = std::abs(b.y - a.y) > std::abs(b.x - a.x);
            Vertex a1 = a;
            Vertex b1 = b;
            (steep ? a1.x : a1.y) += 1.0f;
            (steep ? b1.x : b1.y) += 1.0f;
            emit(target, {a, b, a1, b, b1, a1});
            if (target.primitive == Primitive::Line)
                run.clear();
            else
                run.erase(run.begin());
        }
        break;

    case Primitive::Point:
    {
        Vertex right = vertex;
        Vertex down = vertex;
        Vertex corner = vertex;
        right.x += 1.0f;
        down.y += 1.0f;
        corner.x += 1.0f;
        corner.y += 1.0f;
        emit(target, {vertex, right, down, right, corner, down});
        run.clear();
        break;
    }
    }
}

void UiCanvas::fillFrame(Layer &target, BlendMode blend, std::uint32_t color, std::uint8_t fixedAlpha)
{
    constexpr std::uint64_t kNoAlphaTest = 0;
    const Primitive previousPrimitive = std::exchange(target.primitive, Primitive::Sprite);
    const bool previousTextured = std::exchange(target.textured, false);
    const std::uint64_t previousAlpha = std::exchange(target.alpha, static_cast<std::uint64_t>(fixedAlpha) << 32);
    const std::uint64_t previousTest = std::exchange(target.test, kNoAlphaTest);
    const std::uint64_t previousFrame = std::exchange(target.frame, static_cast<std::uint64_t>(kFbmskAlpha) << 32);
    const float previousSqueeze = std::exchange(target.squeeze, 1.0f); // the whole display, whatever was squeezed
    target.forcedBlend = blend;

    const Vertex a{0.0f, 0.0f, 0.0f, 0.0f, color};
    const Vertex b{kFrameWidth, kFrameHeight, 0.0f, 0.0f, color};
    target.run.clear();
    assemble(target, a);
    assemble(target, b);

    target.forcedBlend.reset();
    target.squeeze = previousSqueeze;
    target.frame = previousFrame;
    target.test = previousTest;
    target.alpha = previousAlpha;
    target.textured = previousTextured;
    target.primitive = previousPrimitive;
}

void UiCanvas::scaleFrame(const void *list, float factor)
{
    constexpr float kFixedOne = 128.0f;
    const auto boost = [](float amount) {
        const auto grey = static_cast<std::uint32_t>(std::clamp(amount, 0.0f, 1.0f) * 255.0f);
        return grey | grey << 8 | grey << 16 | 0xff000000u;
    };

    std::scoped_lock lock(m_lock);
    Layer &target = layer(list);
    if (factor <= 1.0f)
    {
        fillFrame(target, BlendMode::DestinationScale, 0, static_cast<std::uint8_t>(std::max(factor, 0.0f) * kFixedOne));
        return;
    }
    // A blend constant clamps at 1 on this target, so a boost goes through the source colour, twice past 2.
    if (factor > 2.0f)
    {
        fillFrame(target, BlendMode::DestinationBoost, boost(1.0f), 0);
        factor /= 2.0f;
    }
    fillFrame(target, BlendMode::DestinationBoost, boost(factor - 1.0f), 0);
}

void UiCanvas::contrastFrame(const void *list, std::uint32_t rgba)
{
    constexpr float kGsOne = 128.0f;
    const std::uint32_t alpha = rgba >> 24;
    const auto grey = static_cast<std::uint32_t>(std::min(static_cast<float>(alpha) / kGsOne, 1.0f) * 255.0f);

    std::scoped_lock lock(m_lock);
    Layer &target = layer(list);
    // Cd * (1 + As), then Cs * As off it: no fixed-function blend does both at once.
    fillFrame(target, BlendMode::DestinationBoost, grey | grey << 8 | grey << 16 | 0xff000000u, 0);
    fillFrame(target, BlendMode::Subtractive, rgba, 0);
}

std::shared_ptr<Image> UiCanvas::rampImage(const std::uint32_t *ramp)
{
    constexpr int kRampSize = 256;
    constexpr std::uint32_t kOpaque = 0xff000000u;
    constexpr int kCsm1Swapped = 0x18; // the two index bits CSM1 exchanges

    std::shared_ptr<Image> &image = m_ramps[ramp];
    if (image != nullptr)
        return image;

    image = std::make_shared<Image>();
    image->width = kRampSize;
    image->height = 1;
    image->pixels.resize(kRampSize * sizeof(std::uint32_t));
    for (int index = 0; index < kRampSize; index++)
    {
        const int bits = index & kCsm1Swapped;
        const int stored = bits == 0 || bits == kCsm1Swapped ? index : index ^ kCsm1Swapped;
        const std::uint32_t color = ramp[stored] | kOpaque;
        std::memcpy(image->pixels.data() + index * sizeof(color), &color, sizeof(color));
    }
    return image;
}

void UiCanvas::paletteFrame(const void *list, const std::uint32_t *ramp)
{
    std::scoped_lock lock(m_lock);
    Layer &target = layer(list);

    // No vertices: the renderer knows the whole frame. It ends the draw that came before.
    DrawCall draw;
    draw.framePalette = rampImage(ramp);
    draw.firstVertex = static_cast<std::uint32_t>(target.vertices.size());
    target.draws.push_back(std::move(draw));
}

void UiCanvas::filmOverlay(const void *list, FilmOverlay film)
{
    std::scoped_lock lock(m_lock);
    Layer &target = layer(list);

    DrawCall draw;
    draw.film = std::make_shared<FilmOverlay>(std::move(film));
    draw.firstVertex = static_cast<std::uint32_t>(target.vertices.size());
    target.draws.push_back(std::move(draw));
}

void UiCanvas::beginFrameFilter(const void *list, int frameBlock)
{
    std::scoped_lock lock(m_lock);
    Layer &target = layer(list);

    DrawCall draw;
    draw.frameFilter = std::make_shared<FrameFilter>();
    draw.frameFilter->frameBlock = frameBlock;
    draw.firstVertex = static_cast<std::uint32_t>(target.vertices.size());
    target.draws.push_back(std::move(draw));
}

void UiCanvas::addFrameFilterStep(const void *list, const FrameFilterStep &step)
{
    std::scoped_lock lock(m_lock);
    Layer &target = layer(list);

    // A step outside an open filter has nothing to draw over: dropped.
    if (target.draws.empty() || target.draws.back().frameFilter == nullptr)
        return;
    target.draws.back().frameFilter->steps.push_back(step);
}

void UiCanvas::rippleFrame(const void *list, const YamiPanRipple &ripple)
{
    constexpr int kFrameUnits = 512 * 16; // the GS's 1/16 pixels
    constexpr float kUnitsPerPixel = 16.0f;
    constexpr std::uint32_t kLevel = 0x80;
    constexpr std::uint32_t kGrey = 0x808080;

    const int across = ripple.columns + 1;
    const int down = ripple.rows + 1;
    const int stepX = kFrameUnits / across;
    const int stepY = kFrameUnits / down;

    // The sheet's edge stays where it is, at the level height; the points are inside it.
    const auto corner = [&](int column, int row) {
        const int x = column * stepX;
        const int y = row * stepY;
        int pushX = 0;
        int pushY = 0;
        std::uint32_t height = kLevel;
        if (column > 0 && column < across && row > 0 && row < down)
        {
            const YamiPanRipplePoint &point = ripple.points[(row - 1) * ripple.columns + (column - 1)];
            pushX = point.x;
            pushY = point.y;
            height = static_cast<std::uint32_t>(point.height) & 0xff;
        }
        return Vertex{static_cast<float>(x + pushX) / kUnitsPerPixel, static_cast<float>(y + pushY) / kUnitsPerPixel,
                      static_cast<float>(x) / kFrameUnits, static_cast<float>(y) / kFrameUnits, kGrey | height << 24};
    };

    std::scoped_lock lock(m_lock);
    Layer &target = layer(list);

    DrawCall draw;
    draw.frameRipple = true;
    draw.rippleTint = ripple.tint;
    draw.firstVertex = static_cast<std::uint32_t>(target.vertices.size());
    for (int row = 0; row < down; row++)
    {
        for (int column = 0; column < across; column++)
        {
            const Vertex topLeft = corner(column, row);
            const Vertex topRight = corner(column + 1, row);
            Vertex bottomLeft = corner(column, row + 1);
            // The ROM's own slip: this one corner of the top right cell is 0x80 higher, wrapping.
            if (row == 0 && column == ripple.columns)
                bottomLeft.color += kLevel << 24;
            const Vertex bottomRight = corner(column + 1, row + 1);
            target.vertices.insert(target.vertices.end(),
                                   {topLeft, topRight, bottomLeft, topRight, bottomLeft, bottomRight});
        }
    }
    draw.vertexCount = static_cast<std::uint32_t>(target.vertices.size()) - draw.firstVertex;
    target.draws.push_back(std::move(draw));
}

void UiCanvas::drawImage(const void *list, std::shared_ptr<Image> image, float x, float y, float squeeze)
{
    std::scoped_lock lock(m_lock);
    Layer &target = layer(list);

    const auto width = static_cast<float>(image->width);
    const auto height = static_cast<float>(image->height);
    constexpr std::uint32_t kOpaqueWhite = 0x80808080;

    const std::shared_ptr<Image> previous = std::exchange(target.image, std::move(image));
    const Primitive previousPrimitive = std::exchange(target.primitive, Primitive::Sprite);
    const bool previousTextured = std::exchange(target.textured, true);
    const bool previousBlended = std::exchange(target.blended, false);

    const Vertex a{squeezedX(x, squeeze), y, 0.0f, 0.0f, kOpaqueWhite};
    const Vertex b{squeezedX(x + width, squeeze), y + height, width, height, kOpaqueWhite};
    target.run.clear();
    assemble(target, a);
    assemble(target, b);

    target.image = previous;
    target.primitive = previousPrimitive;
    target.textured = previousTextured;
    target.blended = previousBlended;
}

void UiCanvas::drawMacroblocks(const void *list, const void *owner, int picture, const void *macroblocks, int columns,
                               int rows, float x, float y)
{
    constexpr int kBlockSize = 16;
    constexpr std::size_t kBlockRowBytes = kBlockSize * 4;
    constexpr std::size_t kBlockBytes = kBlockSize * kBlockRowBytes;
    constexpr std::byte kOpaque{0xff};

    if (m_movieImage == nullptr || m_movieOwner != owner || m_moviePicture != picture)
    {
        auto image = std::make_shared<Image>();
        image->width = columns * kBlockSize;
        image->height = rows * kBlockSize;
        image->pixels.resize(static_cast<std::size_t>(image->width) * static_cast<std::size_t>(image->height) * 4);

        const auto stride = static_cast<std::size_t>(image->width) * 4;
        const auto *const blocks = static_cast<const std::byte *>(macroblocks);
        for (int column = 0; column < columns; column++)
        {
            for (int row = 0; row < rows; row++)
            {
                const std::byte *src = blocks + static_cast<std::size_t>(column * rows + row) * kBlockBytes;
                std::byte *dst = image->pixels.data() + static_cast<std::size_t>(row * kBlockSize) * stride +
                                 static_cast<std::size_t>(column * kBlockSize) * 4;
                for (int line = 0; line < kBlockSize; line++, src += kBlockRowBytes, dst += stride)
                    std::memcpy(dst, src, kBlockRowBytes);
            }
        }
        for (std::size_t alpha = 3; alpha < image->pixels.size(); alpha += 4)
            image->pixels[alpha] = kOpaque;

        m_movieImage = std::move(image);
        m_movieOwner = owner;
        m_moviePicture = picture;
    }
    // A movie is a picture, not an effect: it keeps its shape, with the display's sides left as they are.
    drawImage(list, m_movieImage, x, y, squeezeFactor());
}

void UiCanvas::kick(std::span<const void *const> lists, std::chrono::steady_clock::time_point time)
{
    auto frame = m_renderer.takeFreeFrame();
    {
        std::scoped_lock lock(m_lock);
        frame->number = ++m_frames;
        frame->time = time;
        for (const void *const list : lists)
        {
            m_renderer.models().collect(list, *frame);

            const auto found = m_layers.find(list);
            if (found == m_layers.end())
                continue;

            Layer &source = found->second;
            const auto offset = static_cast<std::uint32_t>(frame->vertices.size());
            frame->vertices.insert(frame->vertices.end(), source.vertices.begin(), source.vertices.end());
            for (DrawCall &draw : source.draws)
            {
                draw.firstVertex += offset;
                frame->draws.push_back(std::move(draw));
            }
            source.vertices.clear();
            source.draws.clear();
        }
    }
    m_renderer.submit(std::move(frame));
}

} // namespace yamipan::rendering
