// The game's 2D draw module. game/uidrawer.cpp & co keep every decision FS2's drawer makes and hand them
// over as the GS register values they would have packed; this turns those values into host draws. No GS
// emulation: registers it has no use for are ignored, and nothing ever goes through GS memory.
//
// Each packet list the game draws into is a layer, recorded apart (a list can be reopened during a frame).
// At the frame's end Vif1PktManage::Kick names the lists that hold draws, in chain order, and the layers are
// joined into one Frame for the render thread.
#pragma once

#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <unordered_map>
#include <vector>

#include "yamipan/rendering/frame.h"

struct YamiPanUiVertex;

namespace yamipan::rendering
{

class Renderer;

class UiCanvas
{
public:
    explicit UiCanvas(Renderer &renderer);

    void setRegister(const void *list, int reg, std::uint64_t value);
    void setPrimitive(const void *list, std::uint64_t prim, const std::uint8_t *sharedColor);
    void bindImage(const void *list, std::shared_ptr<Image> image);
    void begin(const void *list, int format);
    void addVertex(const void *list, const YamiPanUiVertex &vertex);

    // A movie picture, or any image the game replaces each frame: `width` x `height` RGBA8 pixels drawn
    // at (x, y) in the frame.
    void drawImage(const void *list, std::shared_ptr<Image> image, float x, float y);

    // Ends the frame: these layers, in this order, go to the renderer.
    void kick(std::span<const void *const> lists);

private:
    enum class Primitive : std::uint8_t
    {
        Point,
        Line,
        LineStrip,
        Triangle,
        TriangleStrip,
        TriangleFan,
        Sprite,
    };

    struct Layer
    {
        // The GS context, as far as it decides anything here.
        std::uint64_t alpha = 0;
        std::uint64_t clamp = 0;
        std::uint64_t tex1 = 0;
        std::uint64_t test = 0;
        std::uint64_t frame = 0;
        Scissor scissor;
        std::shared_ptr<Image> image;

        Primitive primitive = Primitive::Sprite;
        bool gouraud = false;
        bool textured = false;
        bool blended = false;
        bool hasSharedColor = false;
        std::uint32_t sharedColor = 0;
        int format = 0;
        std::vector<Vertex> run; // the vertices of the open primitive
        unsigned runCount = 0;

        std::vector<Vertex> vertices;
        std::vector<DrawCall> draws;
    };

    Layer &layer(const void *list);
    void emit(Layer &target, std::initializer_list<Vertex> triangleVertices);
    void assemble(Layer &target, const Vertex &vertex);
    static DrawCall stateOf(const Layer &layer);

    Renderer &m_renderer;
    std::mutex m_lock;
    std::unordered_map<const void *, Layer> m_layers;
    std::uint64_t m_frames = 0;
};

} // namespace yamipan::rendering
