/* ==========================================================================
 *  game/fontdata.h
 *
 *  A font's glyph table (game/fontdata.cpp, not written yet; the helpers
 *  at 0x1fe020..0x1fe5b0): the glyphs sorted by their wide character, each
 *  with its rect in the font texture. The text tracks load it as an S2D
 *  image (S2dTextRec::glyphImage, UiFont::m_glyphs). Minimal: what
 *  UiDrawer::drawText reads. The names are the port's (no source string).
 * ======================================================================== */
#ifndef _GAME_FONTDATA_H
#define _GAME_FONTDATA_H

#include <stddef.h>

#include "sdk/scetypes.h"

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
    u_short   code;             // FS2 +0x00: the wide character (sorted)
    short     _02[3];
    FontGlyph glyph;            // FS2 +0x08
};
static_assert(sizeof(FontGlyphEntry) == 0x10, "FontGlyphEntry: EE 0x10");

// The table as loaded (the entry after the last is what a missing
// character draws).
class FontData
{
public:
    // 1 + the newlines (0 for a null text).
    int countLines(const u_short *text) const;                           // FS2 0x001fe150
    // The width in pixels up to the newline or the end: spacing + each
    // glyph's (w / 16 - spacing).
    float lineWidth(float spacing, const u_short *text) const;           // FS2 0x001fe2b0
    // The glyph of the character at text (control characters are the
    // caller's); answers the next character, null at the terminator.
    const u_short *nextGlyph(const u_short *text, FontGlyph *glyph) const; // FS2 0x001fe4d0

    int            _00, _04;
    int            m_count;     // FS2 +0x08: entries before the fallback
    short          _0c;
    short          m_lineHeight; // FS2 +0x0e: 1/16 pixel
    FontGlyphEntry m_entries[1]; // FS2 +0x10
};
static_assert(offsetof(FontData, m_lineHeight) == 0xe, "FontData::m_lineHeight");
static_assert(offsetof(FontData, m_entries) == 0x10, "FontData::m_entries");

#endif /* _GAME_FONTDATA_H */
