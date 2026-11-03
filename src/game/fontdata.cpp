// game/fontdata.cpp -- fonts: the "knj" glyph table and the printers that
// draw printf text with it, one GS sprite per glyph (game/fontdata.h).
//
// PC: the ROM's printers write the sprites as REGLIST data (PRIM, RGBAQ,
// UV, XYZ2, UV, XYZ2) into a packet whose GIF run the caller opened, and
// their state as A+D writes; here the same values go to the host UI draw
// module (yamipan/rendering/yamipan_drawui.h), keyed by that packet. The
// texture is bound by its owner: no upload into GS memory.

#include "game/fontdata.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "game/gfxtarget.h"
#include "game/gsutil.h"
#include "sdk/libgraph.h"
#include "sdk/libvifpk.h"
#include "sdk/scetypes.h"
#include "yamipan/rendering/yamipan_drawui.h"

// The printers format into a 520-byte stack buffer (vsprintf, unbounded,
// as the ROM).
constexpr int kPrintBufferSize = 520;

// A new FontPrinter's spacing: 2 pixels off each advance.
constexpr int kFontDefaultSpacing = 0x20;

// A glyph's sprite: textured, blended, UV in texels.
constexpr u_long kPrimGlyph = SCE_GS_SET_PRIM(6, 0, 1, 0, 1, 0, 1, 0, 0);
// The Z of the sprite's two XYZ2 writes (the far depth; the second
// corner's is all ones).
constexpr u_int kGlyphZ0 = 0xff0fffff;
constexpr u_int kGlyphZ1 = 0xffffffff;

// setDrawState's state: normal blend, TEXA (alpha 0 below / 0x80 above,
// AEM), alpha > 0 passes with depth always.
constexpr u_long kFontAlpha = SCE_GS_SET_ALPHA(0, 1, 0, 1, 0);
constexpr u_long kFontTexa = SCE_GS_SET_TEXA(0, 1, 0x80);
constexpr u_long kFontTest = SCE_GS_SET_TEST(1, 6, 0, 0, 0, 0, 1, 1);
// TEX1_1: LCM 1; the bilinear flag goes to MMAG (bit 5) and MMIN (bit 6:
// the ROM shifts the whole byte, so only 0 / 1 make sense).
constexpr u_long kFontTex1Lcm = 1;
constexpr int    kTex1MmagShift = 5;
constexpr int    kTex1MminShift = 6;

// One glyph as the ROM's six REGLIST words: PRIM, RGBAQ (Q 1.0), UV and
// XYZ2 of the top-left, UV of the bottom-right texel corner and XYZ2
// (x1, y1).
static void putGlyphSprite(sceVif1Packet *packet, u_int rgba, const FontGlyph &glyph,
                           int x0, int y0, int x1, int y1)
{
    u_char rgbaq[4] = { (u_char)rgba, (u_char)(rgba >> 8), (u_char)(rgba >> 16),
                        (u_char)(rgba >> 24) };
    YamiPan_DrawUiPrim(packet, kPrimGlyph, rgbaq);
    YamiPan_DrawUiBegin(packet, YAMIPAN_UIV_UV);
    YamiPanUiVertex corner = {};
    corner.u = (uint16_t)glyph.u;
    corner.v = (uint16_t)glyph.v;
    corner.x = (uint16_t)x0;
    corner.y = (uint16_t)y0;
    corner.z = kGlyphZ0;
    YamiPan_DrawUiVertex(packet, &corner);
    corner.u = (uint16_t)(glyph.u + glyph.w);
    corner.v = (uint16_t)(glyph.v + glyph.h);
    corner.x = (uint16_t)x1;
    corner.y = (uint16_t)y1;
    corner.z = kGlyphZ1;
    YamiPan_DrawUiVertex(packet, &corner);
}

// ──────────────────────────────────────────────────────────────────────
// TextPrinter: the base's deleting dtor only restores its vtable.
// FS2 0x001fcb40
// callcheck: operator_delete (the deleting dtor's delete of this)

TextPrinter::~TextPrinter()
{
}

// ──────────────────────────────────────────────────────────────────────
// SpritePrinter (vtable 0x315fc0, never constructed). The ROM fetches a
// glyph through slot 1 (+0xc) of the object's own vtable, which here is
// printScaled: only a FontPrinter-shaped object could have run these. The
// port answers the end of the text, so they draw nothing.

const char *SpritePrinter::nextGlyph(const char * /*text*/, FontGlyph * /*glyph*/)
{
    return nullptr;
}

// FS2 0x001fcb90

int SpritePrinter::query2()
{
    return 0;
}

// FS2 0x001fcbb0

int SpritePrinter::query3()
{
    return 0;
}

// FS2 0x001fcbc0

int SpritePrinter::query4()
{
    return 0;
}

// FS2 0x001fcbd0

int SpritePrinter::query5()
{
    return 0;
}

// FS2 0x001fcbe0

int SpritePrinter::query6()
{
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// One sprite per glyph from pixel (x, y), no origin and no 12.4 shift, no
// newline or control handling; -1 without a packet. The second corner is
// x + (x + w), the position counted twice (kept as the ROM writes it).
// FS2 0x001fcbf0
// callcheck: sceVif1PkAddGsData (PC: YamiPan_DrawUiPrim / Vertex)

int SpritePrinter::printScaled(float scale, int x, int y, const char *fmt, ...)
{
    if (m_packet == nullptr)
        return -1;
    char buf[kPrintBufferSize];
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
    const char *text = buf;
    int drawn = 0;
    for (;;) {
        FontGlyph glyph;
        text = nextGlyph(text, &glyph);
        if (text == nullptr)
            break;
        int w = (int)((float)glyph.w * scale);
        int h = (int)((float)glyph.h * scale);
        putGlyphSprite(m_packet, m_rgba, glyph, x, y, x + (x + w), y + (y + h));
        x += w;
        drawn++;
    }
    return drawn;
}

// ──────────────────────────────────────────────────────────────────────
// printScaled at scale 1, with the same doubled second corner.
// FS2 0x001fce40
// callcheck: sceVif1PkAddGsData (PC: YamiPan_DrawUiPrim / Vertex)

int SpritePrinter::print(int x, int y, const char *fmt, ...)
{
    if (m_packet == nullptr)
        return -1;
    char buf[kPrintBufferSize];
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
    const char *text = buf;
    int drawn = 0;
    for (;;) {
        FontGlyph glyph;
        text = nextGlyph(text, &glyph);
        if (text == nullptr)
            break;
        putGlyphSprite(m_packet, m_rgba, glyph, x, y, x + (x + glyph.w), y + (y + glyph.h));
        drawn++;
        x += glyph.w;
    }
    return drawn;
}

// ──────────────────────────────────────────────────────────────────────
// FontPrinter: the font's cell width and line height in pixels.
// FS2 0x001fd060

short FontPrinter::cellWidth()
{
    if (m_font == nullptr)
        return 0;
    return m_font->m_cellWidth >> 4;
}

// FS2 0x001fd080

short FontPrinter::lineHeight()
{
    if (m_font == nullptr)
        return 0;
    return m_font->m_lineHeight >> 4;
}

// FS2 0x001fd0a0

int FontPrinter::countLines(const char *text)
{
    if (m_font == nullptr)
        return 0;
    return m_font->countLinesMb(text);
}

// ──────────────────────────────────────────────────────────────────────
// Takes a texture someone else owns (the owned one goes first), at
// `block`, or the texture area's block for a negative one.
// FS2 0x001fd0d0

void FontPrinter::setTexture(Tim2Texture *texture, int block)
{
    if (m_image != nullptr && m_texture != nullptr)
        delete m_texture;
    m_image = nullptr;
    m_texture = texture;
    m_block = block;
    if (m_block < 0)
        m_block = gfxGetVramBlock(GfxVram_Texture);
}

// ──────────────────────────────────────────────────────────────────────
// Owns a texture of the TIM2 image (the image pointer marks it owned).
// FS2 0x001fd160
// callcheck: operator_new (the new expression)

void FontPrinter::setImage(const void *image, int block)
{
    if (m_image != nullptr && m_texture != nullptr)
        delete m_texture;
    m_image = nullptr;
    m_texture = nullptr;
    m_image = image;
    m_texture = new Tim2Texture(image);
    m_block = block;
    if (m_block < 0)
        m_block = gfxGetVramBlock(GfxVram_Texture);
}

// FS2 0x001fd210

void FontPrinter::releaseTexture()
{
    if (m_image != nullptr && m_texture != nullptr)
        delete m_texture;
    m_image = nullptr;
    m_texture = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Keeps the font when it starts with "knj" (its glyph count with it).
// FS2 0x001fd260

u_char FontPrinter::setFont(const FontData *font)
{
    m_font = font;
    if (m_font == nullptr)
        return 0;
    if (strncmp(m_font->m_magic, kFontMagic, sizeof(kFontMagic)) == 0) {
        m_glyphCount = m_font->m_count;
        return 1;
    }
    m_font = nullptr;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// The texture into GS memory at m_block, then a TEXFLUSH.
// FS2 0x001fd2c0
// callcheck: upload, sceVif1PkCnt, sceVif1PkAlign, sceVif1PkOpenDirectCode, sceVif1PkOpenGifTag, sceVif1PkAddGsAD, sceVif1PkCloseGifTag, sceVif1PkCloseDirectCode, sceVif1PkCloseDmaTag (PC: no GS memory; the host decodes the texture at its source when setDrawState binds it, YamiPan_DrawUiReg for the flush)

void FontPrinter::uploadTexture(sceVif1Packet *packet)
{
    YamiPan_DrawUiReg(packet, SCE_GS_TEXFLUSH, 0);
}

// ──────────────────────────────────────────────────────────────────────
// The sprites of print / printScaled: start at (origin + x, y) in 1/16
// pixel, centred by the flags (x on each line's own width, y on half the
// first line), each glyph scaled from that corner, advance w - spacing,
// line step lineHeight - lineSpacing; the end position is kept. The
// centring widths are the unscaled textWidth (as the ROM).

int FontPrinter::drawText(float scale, int x, int y, const char *text)
{
    int drawn = 0;
    int penX = (x + m_originX) * 16;
    int penY = (y + m_originY) * 16;
    m_startX = penX;
    m_startY = penY;
    int lineH = (int)((float)m_font->m_lineHeight * scale);
    int lineStep = lineH - m_lineSpacing;
    if (m_flags & TextPrinterFlag_CenterY)
        penY -= lineH / 2;
    if (m_flags & TextPrinterFlag_CenterX)
        penX -= textWidth(text) / 2;
    for (;;) {
        signed char c = (signed char)*text;
        if (c == '\n') {
            penY += lineStep;
            penX = m_startX;
            text++;
            if (m_flags & TextPrinterFlag_CenterX)
                penX -= textWidth(text) / 2;
            continue;
        }
        if (c >= 1 && c < ' ') {
            text++;
            continue;
        }
        FontGlyph glyph;
        text = nextGlyph(text, &glyph);
        if (text == nullptr)
            break;
        int w = (int)((float)glyph.w * scale);
        int h = (int)((float)glyph.h * scale);
        putGlyphSprite(m_packet, m_rgba, glyph, penX, penY, penX + w, penY + h);
        drawn++;
        penX += w - m_spacing;
    }
    m_endX = penX;
    m_endY = penY;
    return drawn;
}

// ──────────────────────────────────────────────────────────────────────
// printf with the glyphs, advance and line height times scale.
// FS2 0x001fd370
// callcheck: sceVif1PkAddGsData (PC: YamiPan_DrawUiPrim / Vertex in drawText)

int FontPrinter::printScaled(float scale, int x, int y, const char *fmt, ...)
{
    char buf[kPrintBufferSize];
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
    return drawText(scale, x, y, buf);
}

// ──────────────────────────────────────────────────────────────────────
// printf at (x, y) from the origin; answers the sprites drawn.
// FS2 0x001fd6f0
// callcheck: sceVif1PkAddGsData (PC: YamiPan_DrawUiPrim / Vertex in drawText)

int FontPrinter::print(int x, int y, const char *fmt, ...)
{
    char buf[kPrintBufferSize];
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
    return drawText(1.0f, x, y, buf);
}

// ──────────────────────────────────────────────────────────────────────
// The first `count` lines' widths in pixels (textWidth >> 4) after the
// formatting; answers the line count. Spaces and control bytes are not
// looked up.
// FS2 0x001fda30

int FontPrinter::measureLines(int *widths, int count, const char *fmt, ...)
{
    // PC: the ROM zeroes before its null test (a null table with a
    // positive count writes through null).
    if (widths != nullptr) {
        for (int i = 0; i < count; i++)
            widths[i] = 0;
    }
    if (widths == nullptr)
        return 0;
    if (fmt == nullptr)
        return 0;
    char buf[kPrintBufferSize];
    va_list args;
    va_start(args, fmt);
    vsprintf(buf, fmt, args);
    va_end(args);
    const char *text = buf;
    if (count > 0)
        widths[0] = textWidth(text) >> 4;
    int lines = 1;
    for (;;) {
        signed char c = (signed char)*text;
        if (c == '\n') {
            text++;
            if (lines < count)
                widths[lines] = textWidth(text) >> 4;
            lines++;
            continue;
        }
        if (c == ' ' || (c >= 1 && c < ' ')) {
            text++;
            continue;
        }
        FontGlyph glyph;
        text = nextGlyph(text, &glyph);
        if (text == nullptr)
            return lines;
    }
}

// FS2 0x001fdce0

int FontPrinter::textWidth(const char *text)
{
    return m_font->lineWidthFixedMb(text, m_spacing);
}

// ──────────────────────────────────────────────────────────────────────
// The sprites' state: normal blend, TEXA, the filter, the alpha test, the
// texture and a TEXFLUSH.
// FS2 0x001fdcf0
// callcheck: putTex0, sceVif1PkCnt, sceVif1PkAlign, sceVif1PkOpenDirectCode, sceVif1PkOpenGifTag, sceVif1PkAddGsAD, sceVif1PkCloseGifTag, sceVif1PkCloseDirectCode, sceVif1PkCloseDmaTag (PC: YamiPan_DrawUiReg; the texture by its owner instead of TEX0_1 at m_block)

void FontPrinter::setDrawState(sceVif1Packet *packet, u_char bilinear)
{
    YamiPan_DrawUiReg(packet, SCE_GS_ALPHA_1, kFontAlpha);
    YamiPan_DrawUiReg(packet, SCE_GS_TEXA, kFontTexa);
    YamiPan_DrawUiReg(packet, SCE_GS_TEX1_1,
                      (u_long)bilinear << kTex1MmagShift | kFontTex1Lcm
                          | (u_long)bilinear << kTex1MminShift);
    YamiPan_DrawUiReg(packet, SCE_GS_TEST_1, kFontTest);
    YamiPan_DrawUiTexture(packet, m_texture);
    YamiPan_DrawUiReg(packet, SCE_GS_TEXFLUSH, 0);
}

// FS2 0x001fdff0

const char *FontPrinter::nextGlyph(const char *text, FontGlyph *glyph)
{
    if (*text == '\0')
        return nullptr;
    return m_font->nextGlyphMb(text, glyph);
}

// ──────────────────────────────────────────────────────────────────────
// The owned texture goes (releaseTexture inline), then the base.
// FS2 0x001fe020
// callcheck: operator_delete (the deleting dtor's delete of this)

FontPrinter::~FontPrinter()
{
    if (m_image != nullptr && m_texture != nullptr)
        delete m_texture;
    m_image = nullptr;
    m_texture = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// A printer at origin (x, y) pixels: no font or texture, the texture
// area's block chosen later (-1), spacing 2 pixels.
// FS2 0x001fe0c0

FontPrinter::FontPrinter(short x, short y)
    : TextPrinter(x, y)
{
    m_font = nullptr;
    m_image = nullptr;
    m_texture = nullptr;
    m_block = -1;
    m_glyphCount = 0;
    m_startX = x << 4;
    m_startY = y << 4;
    m_endX = x << 4;
    m_endY = y << 4;
    m_spacing = kFontDefaultSpacing;
    m_lineSpacing = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FontData: the entry of `code`, or the fallback after the last. The ROM's
// binary search (a do-while: one probe even with no entries).

const FontGlyphEntry *FontData::find(u_short code) const
{
    int lo = 0;
    int hi = m_count;
    do {
        int mid = (hi + lo) >> 1;
        u_short probe = m_entries[mid].code;
        if (probe == code)
            return &m_entries[mid];
        if (code < probe)
            hi = mid;
        else
            lo = mid + 1;
    } while (hi != lo);
    return &m_entries[m_count];
}

// ──────────────────────────────────────────────────────────────────────
// 1 + the newlines of wide text.
// FS2 0x001fe150

int FontData::countLines(const u_short *text) const
{
    if (text == nullptr)
        return 0;
    int lines = 1;
    for (;;) {
        u_short c = *text;
        if (c == 0)
            return lines;
        if (c == '\n') {
            text++;
            lines++;
            continue;
        }
        if (c < 0x20) {
            text++;
            continue;
        }
        FontGlyph glyph;
        text = nextGlyph(text, &glyph);
        if (text == nullptr)
            return lines;
    }
}

// ──────────────────────────────────────────────────────────────────────
// 1 + the newlines of byte text; spaces and control bytes skipped.
// FS2 0x001fe1e0

int FontData::countLinesMb(const char *text) const
{
    if (text == nullptr)
        return 0;
    int lines = 1;
    for (;;) {
        signed char c = (signed char)*text;
        if (c == '\n') {
            lines++;
            text++;
            continue;
        }
        if (c == ' ' || (c >= 1 && c < ' ')) {
            text++;
            continue;
        }
        FontGlyph glyph;
        text = nextGlyphMb(text, &glyph);
        if (text == nullptr)
            return lines;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The width in pixels of wide text up to its newline.
// FS2 0x001fe2b0

float FontData::lineWidth(float spacing, const u_short *text) const
{
    float width = spacing;
    for (;;) {
        u_short c = *text;
        if (c == 0 || c == '\n')
            return width;
        if (c < 0x20) {
            text++;
            continue;
        }
        FontGlyph glyph;
        text = nextGlyph(text, &glyph);
        if (text == nullptr)
            return width;
        width = width + ((float)glyph.w / 16.0f - spacing);
    }
}

// ──────────────────────────────────────────────────────────────────────
// lineWidth in 1/16 pixel.
// FS2 0x001fe370

int FontData::lineWidthFixed(const u_short *text, int spacing) const
{
    int width = spacing;
    for (;;) {
        u_short c = *text;
        if (c == 0 || c == '\n')
            return width;
        if (c < 0x20) {
            text++;
            continue;
        }
        FontGlyph glyph;
        text = nextGlyph(text, &glyph);
        if (text == nullptr)
            return width;
        width += glyph.w - spacing;
    }
}

// ──────────────────────────────────────────────────────────────────────
// lineWidthFixed for byte text (the terminator ends it in nextGlyphMb).
// FS2 0x001fe410

int FontData::lineWidthFixedMb(const char *text, int spacing) const
{
    int width = spacing;
    for (;;) {
        signed char c = (signed char)*text;
        if (c == '\n')
            return width;
        if (c >= 1 && c < ' ') {
            text++;
            continue;
        }
        FontGlyph glyph;
        text = nextGlyphMb(text, &glyph);
        if (text == nullptr)
            return width;
        width += glyph.w - spacing;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The glyph of a wide character (the fallback when missing).
// FS2 0x001fe4d0

const u_short *FontData::nextGlyph(const u_short *text, FontGlyph *glyph) const
{
    u_short code = *text;
    if (code == 0)
        return nullptr;
    *glyph = find(code)->glyph;
    return text + 1;
}

// ──────────────────────────────────────────────────────────────────────
// The glyph of a one- or two-byte character: a byte below 0x20 (signed,
// so also 0x80..0xff) leads a two-byte code b0 << 8 | b1, any other byte
// is b << 8.
// FS2 0x001fe5b0

const char *FontData::nextGlyphMb(const char *text, FontGlyph *glyph) const
{
    signed char lead = (signed char)text[0];
    if (lead == 0)
        return nullptr;
    const char *next;
    u_short code;
    if (lead < 0x20) {
        next = text + 2;
        code = (u_short)((u_char)text[0] << 8 | (u_char)text[1]);
    } else {
        next = text + 1;
        code = (u_short)((u_char)text[0] << 8);
    }
    *glyph = find(code)->glyph;
    return next;
}
