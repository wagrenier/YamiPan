// The Pan side of textures: the game's TIM2 pictures decoded at their source to RGBA8, ready for the render
// thread to turn into GPU textures (Renderer::uploadImages). Nothing passes through GS memory: no swizzle, no
// CLUT upload, no replay of the game's transfers.
#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>

class Tim2Texture; // rena/rena_2d_texture_tim2.h

namespace yamipan::rendering
{

struct Image;

class TextureStore
{
public:
    TextureStore() = default;
    TextureStore(const TextureStore &) = delete;
    TextureStore &operator=(const TextureStore &) = delete;

    // The picture of `texture`, decoded during the call: UiDrawer::setImage deletes its temporary texture
    // object right after the bind, so nothing here may keep a pointer into it. The answer shares its pixels
    // with every other frame that draws the same picture. Null when there is no picture to show: a render
    // target (not decoded, see open-items.md "Next up" item 8), or a file that did not load.
    std::shared_ptr<Image> imageOf(const Tim2Texture &texture);

private:
    struct Entry
    {
        std::shared_ptr<Image> image;
        std::uint64_t lastUse = 0;
    };

    void evictOver(std::size_t byteBudget);

    std::mutex m_lock;
    std::unordered_map<std::uint64_t, Entry> m_entries; // by the picture's content, see imageOf
    std::size_t m_bytes = 0;
    std::uint64_t m_uses = 0;
    bool m_loggedTarget = false;
    bool m_loggedUnreadable = false;
};

} // namespace yamipan::rendering
