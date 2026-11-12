#include "yamipan/rendering/texture_store.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <span>
#include <vector>

#include "game/gsutil.h"
#include "rena/rena_2d_texture_tim2.h"
#include "rena/tim2.h"
#include "yamipan/core/logger.h"
#include "yamipan/rendering/frame.h"

namespace yamipan::rendering
{
namespace
{

// Decoded pictures are dropped, least recently bound first, past this much. One stage's UI fits many times
// over, so this only bounds a long session; an evicted picture a frame still holds lives on in that frame.
constexpr std::size_t kByteBudget = std::size_t{256} * 1024 * 1024;

constexpr std::uint8_t kClutLinearOrder = 0x80; // ClutType's bit 7
constexpr std::uint8_t kClutKindMask = 0x3f;

// CSM1 keeps an 8-bit CLUT in blocks of 32 entries with the middle two groups of 8 exchanged.
constexpr std::size_t kCsm1Block = 32;
constexpr std::size_t kCsm1Group = 8;

constexpr std::size_t kIndex4Colors = 16;
constexpr std::size_t kIndex8Colors = 256;

constexpr std::size_t kQwordBytes = 16;

struct Rgba
{
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;
    std::uint8_t a = 0;
};
static_assert(sizeof(Rgba) == 4);

using Palette = std::array<Rgba, kIndex8Colors>;

constexpr std::uint8_t kOpaque = 0xff;

// A file's alpha runs to 0x80, opaque. ui.frag multiplies the texel by the vertex colour times 255 / 128, which
// is right for RGB, but alpha goes on to the blender and has to reach 1.0 there, so it is stored doubled.
constexpr std::uint8_t expandAlpha(std::uint8_t alpha)
{
    return static_cast<std::uint8_t>(std::min(alpha * 2, 0xff));
}

constexpr std::uint8_t expandFiveBits(unsigned value)
{
    return static_cast<std::uint8_t>((value & 0x1f) * 0xff / 0x1f);
}

constexpr std::size_t bytesPerColor(int kind)
{
    switch (kind)
    {
    case Tim2Image_Rgb16:
        return 2;
    case Tim2Image_Rgb24:
        return 3;
    case Tim2Image_Rgb32:
        return 4;
    default:
        return 0;
    }
}

// The 16-bit colour's top bit stands for alpha: the game's TEXA gives it 0x80 or 0.
Rgba colorAt(const std::uint8_t *data, std::size_t index, int kind)
{
    const std::uint8_t *const color = data + index * bytesPerColor(kind);
    switch (kind)
    {
    case Tim2Image_Rgb16:
    {
        const unsigned value = color[0] | static_cast<unsigned>(color[1]) << 8;
        return {expandFiveBits(value), expandFiveBits(value >> 5), expandFiveBits(value >> 10),
                (value & 0x8000) != 0 ? kOpaque : std::uint8_t{0}};
    }
    case Tim2Image_Rgb24:
        return {color[0], color[1], color[2], kOpaque};
    case Tim2Image_Rgb32:
        return {color[0], color[1], color[2], expandAlpha(color[3])};
    default:
        return {};
    }
}

// Not a cryptographic hash, and it need not be: it sees the game's own pictures, and only has to tell them
// apart at a few bytes per cycle, since every bind pays for it.
class ContentHash
{
public:
    void add(std::uint64_t word)
    {
        m_state = std::rotl((m_state ^ word) * kMultiplier, 31);
    }

    void add(std::span<const std::uint8_t> bytes)
    {
        std::size_t offset = 0;
        for (; offset + sizeof(std::uint64_t) <= bytes.size(); offset += sizeof(std::uint64_t))
        {
            std::uint64_t word = 0;
            std::memcpy(&word, bytes.data() + offset, sizeof(word));
            add(word);
        }
        std::uint64_t tail = bytes.size();
        for (; offset < bytes.size(); offset++)
            tail = tail << 8 | bytes[offset];
        add(tail);
    }

    std::uint64_t value() const
    {
        std::uint64_t state = m_state;
        state ^= state >> 32;
        state *= kMultiplier;
        return state ^ state >> 29;
    }

private:
    static constexpr std::uint64_t kMultiplier = 0x9e3779b97f4a7c15ull;

    std::uint64_t m_state = kMultiplier;
};

// One Tim2Texture level as the file has it. Tim2Texture::setupMip has already found the pixels and the CLUT;
// the kinds and the CLUT's order only the file's picture header knows.
class Tim2Decoder
{
public:
    explicit Tim2Decoder(const Tim2Texture &texture)
        : m_width(texture.m_width), m_height(texture.m_height),
          m_pixels(static_cast<const std::uint8_t *>(texture.m_pixels)),
          m_clut(static_cast<const std::uint8_t *>(texture.m_clut)),
          m_pixelBytes(static_cast<std::size_t>(texture.m_imageQwords) * kQwordBytes),
          m_frameWidth(std::max(m_width, 1 << texture.m_tex0.TW)),
          m_frameHeight(std::max(m_height, 1 << texture.m_tex0.TH)),
          m_opaque(texture.m_tex0.TCC == 0)
    {
        if (texture.m_image != nullptr)
            m_header = Tim2GetPictureHeader(texture.m_image, 0);
    }

    bool readable() const
    {
        return m_header != nullptr && m_pixels != nullptr && m_width > 0 && m_height > 0 &&
               (bytesPerColor(m_header->imageType) != 0 || (indexed() && m_clut != nullptr));
    }

    std::uint64_t contentHash() const
    {
        ContentHash hash;
        hash.add(static_cast<std::uint64_t>(m_width) << 32 | static_cast<std::uint64_t>(m_height));
        hash.add({reinterpret_cast<const std::uint8_t *>(m_header), sizeof(*m_header)});
        hash.add({m_pixels, m_pixelBytes});
        if (m_clut != nullptr)
            hash.add({m_clut, m_header->clutSize});
        return hash.value();
    }

    // The GS reads a picture through TEX0's frame, its size rounded up to a power of two (TW, TH): a UV of 1 is the
    // far side of that frame, wherever the file's pixels end, and a repeat wraps there, and a model's UVs count in it.
    // So the Image is the frame, the picture in its corner. The GS would read on into whatever follows in its memory;
    // here the picture's last row and column run on, which keeps a filtered edge as it is.
    std::shared_ptr<Image> decode() const
    {
        const auto frameWidth = static_cast<std::size_t>(m_frameWidth);
        const auto frameHeight = static_cast<std::size_t>(m_frameHeight);
        auto image = std::make_shared<Image>();
        image->width = m_frameWidth;
        image->height = m_frameHeight;
        image->pixels.resize(frameWidth * frameHeight * sizeof(Rgba));

        const std::span<Rgba> frame(reinterpret_cast<Rgba *>(image->pixels.data()), frameWidth * frameHeight);
        if (m_frameWidth == m_width && m_frameHeight == m_height)
        {
            decodePicture(frame);
            return image;
        }

        std::vector<Rgba> picture(pixelCount());
        decodePicture(picture);
        const auto width = static_cast<std::size_t>(m_width);
        const auto height = static_cast<std::size_t>(m_height);
        for (std::size_t row = 0; row < frameHeight; row++)
        {
            const Rgba *const source = picture.data() + std::min(row, height - 1) * width;
            Rgba *const target = frame.data() + row * frameWidth;
            std::copy_n(source, width, target);
            std::fill_n(target + width, frameWidth - width, source[width - 1]);
        }
        return image;
    }

private:
    void decodePicture(std::span<Rgba> out) const
    {
        if (indexed())
            decodeIndexed(out);
        else
            decodeDirect(out);

        if (m_opaque)
        {
            for (Rgba &pixel : out)
                pixel.a = kOpaque;
        }
    }

    std::size_t pixelCount() const
    {
        return static_cast<std::size_t>(m_width) * static_cast<std::size_t>(m_height);
    }

    bool indexed() const
    {
        return m_header->imageType == Tim2Image_Index4 || m_header->imageType == Tim2Image_Index8;
    }

    // A 4-bit file can hold several rows of 16 colours; TEX0 starts at the first (no caller moves CBP yet).
    Palette palette() const
    {
        Palette colors{};
        const bool index4 = m_header->imageType == Tim2Image_Index4;
        const int kind = m_header->clutType & kClutKindMask;
        const std::size_t entrySize = bytesPerColor(kind);
        if (entrySize == 0)
            return colors;

        const std::size_t count = std::min({static_cast<std::size_t>(m_header->clutColors),
                                            static_cast<std::size_t>(m_header->clutSize) / entrySize,
                                            index4 ? kIndex4Colors : kIndex8Colors});
        for (std::size_t index = 0; index < count; index++)
            colors[index] = colorAt(m_clut, index, kind);

        if (!index4 && (m_header->clutType & kClutLinearOrder) == 0)
        {
            for (std::size_t block = 0; block + 3 * kCsm1Group <= count; block += kCsm1Block)
            {
                std::swap_ranges(colors.begin() + block + kCsm1Group, colors.begin() + block + 2 * kCsm1Group,
                                 colors.begin() + block + 2 * kCsm1Group);
            }
        }
        return colors;
    }

    void decodeIndexed(std::span<Rgba> out) const
    {
        const Palette colors = palette();
        if (m_header->imageType == Tim2Image_Index8)
        {
            for (std::size_t index = 0; index < out.size(); index++)
                out[index] = colors[m_pixels[index]];
            return;
        }
        // The low nibble is the left pixel. Lines are not padded: the file's width is the line.
        for (std::size_t index = 0; index < out.size(); index++)
        {
            const std::uint8_t pair = m_pixels[index / 2];
            out[index] = colors[(index & 1) != 0 ? pair >> 4 : pair & 0xf];
        }
    }

    void decodeDirect(std::span<Rgba> out) const
    {
        const int kind = m_header->imageType;
        for (std::size_t index = 0; index < out.size(); index++)
            out[index] = colorAt(m_pixels, index, kind);
    }

    int m_width;
    int m_height;
    const std::uint8_t *m_pixels;
    const std::uint8_t *m_clut;
    std::size_t m_pixelBytes;
    int m_frameWidth;
    int m_frameHeight;
    bool m_opaque;
    const TIM2_PICTUREHEADER *m_header = nullptr;
};

// YAMIPAN_TEX_DUMP=<dir> writes every newly decoded TIM2 file there, so tm2png.py can give a screen its
// references: nothing maps a d2b to its TIM2s offline.
void dumpTim2File(const Tim2Texture &texture, std::uint64_t key)
{
    const char *const directory = std::getenv("YAMIPAN_TEX_DUMP");
    if (directory == nullptr)
        return;

    const auto *const bytes = static_cast<const std::uint8_t *>(texture.m_image);
    const auto *const header = reinterpret_cast<const TIM2_FILEHEADER *>(bytes);
    std::size_t size = header->formatId == Tim2Format_Align128 ? std::size_t{0x80} : sizeof(TIM2_FILEHEADER);
    for (int picture = 0; picture < header->pictures; ++picture)
        size += reinterpret_cast<const TIM2_PICTUREHEADER *>(bytes + size)->totalSize;

    std::filesystem::create_directories(directory);
    std::ofstream file(std::filesystem::path(directory) / std::format("{:016x}.tm2", key), std::ios::binary);
    file.write(reinterpret_cast<const char *>(bytes), static_cast<std::streamsize>(size));
}

} // namespace

// The cache is keyed on the picture's content, the header, pixels and CLUT together, not on the texture
// object or its address. The object is no use: setImage makes a new one per draw. An address plus the header
// is not enough either: the allocator hands a freed block straight back to the next file of the same size,
// and two same-sized pictures have the same header. Hashing costs a pass over the bytes on each bind, far less
// than a decode, and it also picks up a CLUT the game rewrites in place.
std::shared_ptr<Image> TextureStore::imageOf(const Tim2Texture &texture)
{
    std::scoped_lock lock(m_lock);

    // The font binds a plain Tim2Texture, so the target's fields cannot be assumed.
    if (const auto *const gsTexture = dynamic_cast<const GsTexture *>(&texture);
        gsTexture != nullptr && gsTexture->m_isTarget)
    {
        if (!m_loggedTarget)
        {
            log::warning("TextureStore: a draw samples a render target ({:#x}); targets are not decoded",
                         gsTexture->m_targetTex0);
            m_loggedTarget = true;
        }
        return nullptr;
    }

    const Tim2Decoder decoder(texture);
    if (!decoder.readable())
    {
        if (!m_loggedUnreadable)
        {
            log::warning("TextureStore: a texture has no picture to decode");
            m_loggedUnreadable = true;
        }
        return nullptr;
    }

    const std::uint64_t key = decoder.contentHash();
    if (const auto found = m_entries.find(key); found != m_entries.end())
    {
        found->second.lastUse = ++m_uses;
        return found->second.image;
    }

    dumpTim2File(texture, key);
    std::shared_ptr<Image> image = decoder.decode();
    m_bytes += image->pixels.size();
    m_entries.emplace(key, Entry{image, ++m_uses});
    evictOver(kByteBudget);
    return image;
}

void TextureStore::evictOver(std::size_t byteBudget)
{
    while (m_bytes > byteBudget && m_entries.size() > 1)
    {
        const auto oldest =
            std::ranges::min_element(m_entries, {}, [](const auto &entry) { return entry.second.lastUse; });
        m_bytes -= static_cast<std::size_t>(oldest->second.image->width) *
                   static_cast<std::size_t>(oldest->second.image->height) * sizeof(Rgba);
        m_entries.erase(oldest);
    }
}

} // namespace yamipan::rendering
