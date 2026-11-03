#include "yamipan/rendering/ui_canvas.h"

#include <cmath>
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
    return BlendMode::Alpha;
}

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
    converted.x = static_cast<float>(vertex.x) / kWindowUnit - kWindowOrigin;
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
    draw.pipeline.blend = source.blended ? blendModeOf(source.alpha) : BlendMode::None;
    draw.pipeline.mask = colorMaskOf(source.frame);
    draw.image = source.textured ? source.image : nullptr;
    draw.scissor = source.scissor;
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
            return last.pipeline == state.pipeline && last.image == state.image && last.scissor == state.scissor &&
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
            topRight.u = b.u;
            bottomLeft.y = b.y;
            bottomLeft.v = b.v;
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

void UiCanvas::drawImage(const void *list, std::shared_ptr<Image> image, float x, float y)
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

    const Vertex a{x, y, 0.0f, 0.0f, kOpaqueWhite};
    const Vertex b{x + width, y + height, width, height, kOpaqueWhite};
    target.run.clear();
    assemble(target, a);
    assemble(target, b);

    target.image = previous;
    target.primitive = previousPrimitive;
    target.textured = previousTextured;
    target.blended = previousBlended;
}

void UiCanvas::kick(std::span<const void *const> lists)
{
    auto frame = m_renderer.takeFreeFrame();
    {
        std::scoped_lock lock(m_lock);
        frame->number = ++m_frames;
        for (const void *const list : lists)
        {
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
