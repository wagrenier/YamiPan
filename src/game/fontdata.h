/* ==========================================================================
 *  game/fontdata.h
 *
 *  Fonts (game/fontdata.cpp, 0x1fcb40..0x1fe6b0): the glyph table of a
 *  "knj" font file (FontData: the glyphs sorted by their character code,
 *  each with its rect in the font texture) and the printers that draw
 *  printf text with it as one GS sprite per glyph (TextPrinter /
 *  FontPrinter; the wide-text printer on vtable 0x319ff0, 0x2d3700.., is
 *  FontPrinter's subclass). The S2D text tracks use FontData directly
 *  (UiFont::m_glyphs, UiDrawer::drawText). The names are the port's (no
 *  source string).
 * ======================================================================== */
#ifndef _GAME_FONTDATA_H
#define _GAME_FONTDATA_H

#include <stddef.h>

#include "sdk/libvifpk.h"
#include "sdk/scetypes.h"

class Tim2Texture;

// A glyph's rect in the font texture, in 1/16 texel (the GS's UV units):
// w / 16 is also its advance in pixels, h / 16 its height.
struct FontGlyph
{
    short u, v;                 // FS2 +0x0
    short w, h;                 // FS2 +0x4
};
static_assert(sizeof(FontGlyph) == 0x8, "FontGlyph: EE 0x8");

struct FontGlyphEntry
{
    u_short   code;             // FS2 +0x00: the character code (sorted)
    short     _02[3];
    FontGlyph glyph;            // FS2 +0x08
};
static_assert(sizeof(FontGlyphEntry) == 0x10, "FontGlyphEntry: EE 0x10");

// The magic a font file starts with (0x305b38, compared with its NUL).
constexpr char kFontMagic[4] = { 'k', 'n', 'j', '\0' };

// The table as loaded (the entry after the last is what a missing
// character draws). Wide text (u_short) holds the codes as they are; byte
// text holds a byte < 0x20 (signed: also every byte >= 0x80) and the next
// as the code b0 << 8 | b1, any other byte b as b << 8.
class FontData
{
public:
    // 1 + the newlines (0 for a null text).
    int countLines(const u_short *text) const;                           // FS2 0x001fe150
    // The same for byte text; spaces and control bytes are not looked up.
    int countLinesMb(const char *text) const;                            // FS2 0x001fe1e0
    // The width in pixels up to the newline or the end: spacing + each
    // glyph's (w / 16 - spacing).
    float lineWidth(float spacing, const u_short *text) const;           // FS2 0x001fe2b0
    // lineWidth in 1/16 pixel, spacing in 1/16 too.
    int lineWidthFixed(const u_short *text, int spacing) const;          // FS2 0x001fe370
    // lineWidthFixed for byte text.
    int lineWidthFixedMb(const char *text, int spacing) const;           // FS2 0x001fe410
    // The glyph of the character at text (control characters are the
    // caller's); answers the next character, null at the terminator.
    const u_short *nextGlyph(const u_short *text, FontGlyph *glyph) const; // FS2 0x001fe4d0
    // nextGlyph for byte text (one- or two-byte codes).
    const char *nextGlyphMb(const char *text, FontGlyph *glyph) const;   // FS2 0x001fe5b0

    char           m_magic[4];  // FS2 +0x00: kFontMagic
    int            _04;
    int            m_count;     // FS2 +0x08: entries before the fallback
    short          m_cellWidth; // FS2 +0x0c: 1/16 pixel
    short          m_lineHeight; // FS2 +0x0e: 1/16 pixel
    FontGlyphEntry m_entries[1]; // FS2 +0x10

private:
    const FontGlyphEntry *find(u_short code) const;
};
static_assert(offsetof(FontData, m_count) == 0x8, "FontData::m_count");
static_assert(offsetof(FontData, m_cellWidth) == 0xc, "FontData::m_cellWidth");
static_assert(offsetof(FontData, m_lineHeight) == 0xe, "FontData::m_lineHeight");
static_assert(offsetof(FontData, m_entries) == 0x10, "FontData::m_entries");

// ──────────────────────────────────────────────────────────────────────
// The printers. The base (vtable 0x315fb0, only the dtor) holds what a
// sprite needs: colour, the packet drawn into, the origin and the centring
// flags; the vptr follows them (+0x10). layout: host -- EE offsets in the
// comments (CodeWarrior puts the vptr after the data).

enum TextPrinterFlag
{
    TextPrinterFlag_CenterX = 1 << 0,   // each line centred on x
    TextPrinterFlag_CenterY = 1 << 1,   // the first line centred on y
};

// A printer's colour until set: grey at full GS alpha (0x80 is 1.0).
constexpr u_int kTextPrinterRgba = 0x80808080;

// The base (EE 0x14). layout: host -- EE offsets in the comments.
class TextPrinter
{
public:
    // Inline in the ROM (FontPrinter's ctor stores the base vtable first).
    TextPrinter(short x, short y)
        : m_rgba(kTextPrinterRgba), m_packet(nullptr), m_originX(x), m_originY(y), m_flags(0)
    {
    }
    virtual ~TextPrinter();                                              // FS2 0x001fcb40: deleting (obj, flag)

    u_int          m_rgba;      // FS2 +0x00: RGBAQ's colour bytes (0x80808080)
    sceVif1Packet *m_packet;    // FS2 +0x04: the packet the sprites go to (an open REGLIST run)
    short          m_originX;   // FS2 +0x08: pixels, GS window
    short          m_originY;   // FS2 +0x0a
    u_int          m_flags;     // FS2 +0x0c: TextPrinterFlag
    // vptr                        FS2 +0x10
};

// The font printer (EE 0x40, vtable 0x315ff0): a FontData and its texture,
// byte text. Positions are GS window coordinates in 1/16 pixel.
// layout: host -- EE offsets in the comments.
class FontPrinter : public TextPrinter
{
public:
    FontPrinter(short x, short y);                                       // FS2 0x001fe0c0
    virtual ~FontPrinter();                                              // FS2 0x001fe020: slot 0, deleting (obj, flag)

    // Slot 1: the glyph at text (null at the terminator).
    virtual const char *nextGlyph(const char *text, FontGlyph *glyph);  // FS2 0x001fdff0
    // Slot 2: ALPHA_1 / TEXA / TEX1_1 (bilinear: filtered) / TEST_1, the
    // texture's TEX0_1 and a TEXFLUSH.
    virtual void setDrawState(sceVif1Packet *packet, u_char bilinear);   // FS2 0x001fdcf0
    // Slot 3: the texture into GS memory at m_block, then TEXFLUSH.
    virtual void uploadTexture(sceVif1Packet *packet);                   // FS2 0x001fd2c0
    // Slot 4: printf at (x, y) from the origin; answers the sprites drawn.
    virtual int print(int x, int y, const char *fmt, ...);               // FS2 0x001fd6f0
    // Slot 5: print with the glyphs, advance and line height scaled.
    virtual int printScaled(float scale, int x, int y, const char *fmt, ...); // FS2 0x001fd370
    // Slot 6: the first `count` lines' widths in pixels; answers the lines.
    virtual int measureLines(int *widths, int count, const char *fmt, ...); // FS2 0x001fda30
    // Slot 7: the width of text up to its newline, 1/16 pixel.
    virtual int textWidth(const char *text);                             // FS2 0x001fdce0
    // Slots 8 / 9: the font's cell width and line height in pixels.
    virtual short cellWidth();                                           // FS2 0x001fd060
    virtual short lineHeight();                                          // FS2 0x001fd080
    // Slot 10: FontData::countLinesMb.
    virtual int countLines(const char *text);                            // FS2 0x001fd0a0

    // Takes the font when its magic is "knj"; 0 (and no font) otherwise.
    u_char setFont(const FontData *font);                                // FS2 0x001fd260
    // An unowned texture / a TIM2 image to own a texture of; block < 0 is
    // the texture area's (gfxGetVramBlock).
    void setTexture(Tim2Texture *texture, int block);                    // FS2 0x001fd0d0
    void setImage(const void *image, int block);                         // FS2 0x001fd160
    void releaseTexture();                                               // FS2 0x001fd210

    const FontData *m_font;     // FS2 +0x14
    const void     *m_image;    // FS2 +0x18: set when m_texture is owned
    Tim2Texture    *m_texture;  // FS2 +0x1c
    int             m_block;    // FS2 +0x20: the texture's GS block
    int             m_glyphCount; // FS2 +0x24: the font's m_count
    int             m_startX;   // FS2 +0x28: the last print's start, 1/16 pixel
    int             m_startY;   // FS2 +0x2c
    int             m_endX;     // FS2 +0x30: where the last print ended
    int             m_endY;     // FS2 +0x34
    int             m_spacing;  // FS2 +0x38: 1/16 pixel off each advance (0x20)
    int             m_lineSpacing; // FS2 +0x3c: 1/16 pixel off each line step

private:
    // The body print and printScaled each have inline in the ROM (print's
    // is this at scale 1, where every product is exact); not a ROM function.
    int drawText(float scale, int x, int y, const char *text);
};

// The vtable 0x315fc0 after the base's: no RTTI, and no code stores it, so
// this class is never constructed. Its two prints use the printer's fields
// and fetch glyphs through slot 1 of the object's own vtable -- a
// FontPrinter's nextGlyph, but on this vtable printScaled. Kept for the
// record. layout: host
class SpritePrinter
{
public:
    virtual int print(int x, int y, const char *fmt, ...);               // FS2 0x001fce40: slot 0
    virtual int printScaled(float scale, int x, int y, const char *fmt, ...); // FS2 0x001fcbf0: slot 1
    virtual int query2();                                                // FS2 0x001fcb90
    virtual int query3();                                                // FS2 0x001fcbb0
    virtual int query4();                                                // FS2 0x001fcbc0
    virtual int query5();                                                // FS2 0x001fcbd0
    virtual int query6();                                                // FS2 0x001fcbe0

    u_int          m_rgba;      // FS2 +0x00
    sceVif1Packet *m_packet;    // FS2 +0x04

private:
    // The ROM's call through vtable slot 1 (+0xc); not a ROM function.
    const char *nextGlyph(const char *text, FontGlyph *glyph);
};

#endif /* _GAME_FONTDATA_H */
