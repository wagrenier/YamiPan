// game/uidrawer.cpp -- the 2D affine matrix of the layouts and UiDrawer,
// the draw helper their tracks draw through (game/uidrawer.h).
//
// PC: the ROM's drawer writes GIF packets (A+D runs for state, REGLIST
// runs for PRIM and vertices) into the packet list it was begun on; here
// the same register values go to the host UI draw module
// (yamipan/rendering/yamipan_drawui.h), keyed by that packet. The DIRECT /
// GIF run flags are kept: the return values follow them.

#include "game/uidrawer.h"

#include <math.h>
#include <stdint.h>

#include "game/fontdata.h"
#include "game/gamehooks.h"
#include "game/gfxtarget.h"
#include "game/gsutil.h"
#include "game/uitrack.h"
#include "sdk/libgraph.h"
#include "sdk/libvifpk.h"
#include "sdk/scetypes.h"
#include "yamipan/rendering/yamipan_drawui.h"

// The screen matrices of setMode's two modes (0x2fac90 / 0x2facc0): the
// 4:3 and 16:9 displays. [0] widens x by the aspect, [1] narrows it back;
// updateMatrix draws through [1], and in pixel mode with a rotation turns
// in the widened space so the rotation stays round on the display.
// PC: the any-aspect renderer is to take these from its display aspect
// (open-items.md); the ROM's two are kept until then.
static const UiMatrix kUiScreenMatrix[2][2] = {
    { { 4.0f / 3.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f }, { 0.75f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f } },
    { { 16.0f / 9.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f }, { 0.5625f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f } },
};

// The vertices' window coordinates are clamped to the 12.4 range around
// the GS centre (0x8000 = 2048.0) and drawn at the far depth.
constexpr float kUiXyLimit = 32752.0f;
constexpr float kUiPixelLimit = 2047.0f;    // the same in pixels
constexpr int   kGsWindowCentre = 0x8000;
constexpr u_int kUiVertexZ = 0xff0fffff;

// The scissor's window origin: the 512x512 target's centre.
constexpr int kUiScissorOrigin = 0x100;

// drawText's PRIM: a sprite per glyph, textured, blended, in texels.
constexpr u_long kUiPrimText = UiPrim_Sprite | UiPrim_Gouraud | UiPrim_Textured | UiPrim_Blend | UiPrim_UV;

// The colour byte limits of UiVec4::toRgbaq (0x3f7f7f7f, 0x3fff7f7f): at
// or above them the byte is 0xff.
constexpr float kUiColorLimit = 0x1.fefefep-1f;
constexpr float kUiAlphaLimit = 0x1.fefefep+0f;

// ALPHA_1 per UiBlend (FIX 0x80).
static const u_long kUiBlendAlpha[4] = {
    SCE_GS_SET_ALPHA(0, 1, 0, 1, 0x80),     // UiBlend_Normal
    SCE_GS_SET_ALPHA(0, 2, 0, 1, 0x80),     // UiBlend_Add
    SCE_GS_SET_ALPHA(0, 2, 1, 1, 0x80),     // UiBlend_AddDestAlpha
    SCE_GS_SET_ALPHA(0, 1, 1, 1, 0x80),     // UiBlend_NormalDestAlpha
};

// copyTarget's two A+D sets (0x3076f0 / 0x307730): a local-to-local copy
// of a 512x512 PSMCT32 buffer between GS blocks 0x800 and 0x1800.
// layout: host -- the ROM's {u_int reg, pad, u_long value} qwords.
struct UiGsRegWrite
{
    int    reg;
    u_long value;
};
static const UiGsRegWrite kUiCopyToBackup[4] = {
    { SCE_GS_TRXPOS, SCE_GS_SET_TRXPOS(0, 0, 0, 0, 0) },
    { SCE_GS_TRXREG, SCE_GS_SET_TRXREG(512, 512) },
    { SCE_GS_BITBLTBUF, SCE_GS_SET_BITBLTBUF(0x800, 8, SCE_GS_PSMCT32, 0x1800, 8, SCE_GS_PSMCT32) },
    { SCE_GS_TRXDIR, SCE_GS_SET_TRXDIR(2) },
};
static const UiGsRegWrite kUiCopyFromBackup[4] = {
    { SCE_GS_TRXPOS, SCE_GS_SET_TRXPOS(0, 0, 0, 0, 0) },
    { SCE_GS_TRXREG, SCE_GS_SET_TRXREG(512, 512) },
    { SCE_GS_BITBLTBUF, SCE_GS_SET_BITBLTBUF(0x1800, 8, SCE_GS_PSMCT32, 0x800, 8, SCE_GS_PSMCT32) },
    { SCE_GS_TRXDIR, SCE_GS_SET_TRXDIR(2) },
};

// CLAMP_1 of setTexRepeat: both axes repeat, or both clamp.
constexpr u_long kUiClampRepeat = SCE_GS_SET_CLAMP(0, 0, 0, 0, 0, 0);
constexpr u_long kUiClampClamp = SCE_GS_SET_CLAMP(1, 1, 0, 0, 0, 0);

// ──────────────────────────────────────────────────────────────────────
// Helpers, inline in the ROM.

// A point through m as the ROM spells it out (with its trailing + 0).
static inline float pointX(const UiMatrix &m, float x, float y)
{
    return m.tx + m.a * x + m.b * y + 0.0f;
}
static inline float pointY(const UiMatrix &m, float x, float y)
{
    return m.ty + m.c * x + m.d * y + 0.0f;
}

// A colour component as an RGBAQ byte: scale 256 (128 for alpha), 0 at or
// below 0, 0xff from the limit up.
static inline u_char colorByte(float f, float limit, float scale)
{
    if (!(f > 0.0f))
        return 0;
    if (!(f < limit))
        return 0xff;
    return (u_char)(u_int)(f * scale);
}

// The XYZ2 word of a vertex from its 12.4 window offsets: moved to the GS
// centre; split for the host the way the GS reads it.
static void putXyzWord(YamiPanUiVertex *out, int x, int y)
{
    u_long word = (u_long)(int64_t)(x + kGsWindowCentre)
                | (u_long)(int64_t)(y + kGsWindowCentre) << 16
                | (u_long)kUiVertexZ << 32;
    out->x = (u_short)word;
    out->y = (u_short)(word >> 16);
    out->z = (u_int)(word >> 32);
}

// xy through the drawer's matrix in 12.4, clamped.
static void putXyz(YamiPanUiVertex *out, const UiMatrix *m, const float *xy)
{
    float p[2];
    uiTransformPoint(p, m, xy, 1);
    if (p[0] > kUiXyLimit)
        p[0] = kUiXyLimit;
    if (p[0] < -kUiXyLimit)
        p[0] = -kUiXyLimit;
    if (p[1] > kUiXyLimit)
        p[1] = kUiXyLimit;
    if (p[1] < -kUiXyLimit)
        p[1] = -kUiXyLimit;
    putXyzWord(out, (int)p[0], (int)p[1]);
}

// A window pixel (already through the matrix: the text and the wide
// strips), clamped in pixels, then 12.4.
static inline float clampPixel(float f)
{
    if (f > kUiPixelLimit)
        f = kUiPixelLimit;
    if (f < -kUiPixelLimit)
        f = -kUiPixelLimit;
    return f;
}
static void putXyzPixels(YamiPanUiVertex *out, float x, float y)
{
    x = clampPixel(x);
    y = clampPixel(y);
    putXyzWord(out, (int)(x * 16.0f), (int)(y * 16.0f));
}

// The UV word from texel coordinates, in 12.4.
static void putUvTexels(YamiPanUiVertex *out, float u, float v)
{
    u_long word = (u_long)(int64_t)(int)(u * 16.0f)
                | (u_long)(int64_t)(int)(v * 16.0f) << 16;
    out->u = (u_short)word;
    out->v = (u_short)(word >> 16);
}

// uv 0..1 times the bound texture's size.
static void putUv(YamiPanUiVertex *out, const float *uv, int width, int height)
{
    putUvTexels(out, uv[0] * (float)width, uv[1] * (float)height);
}

// uv 0..1 within a sheet sprite's rect.
static void putSheetUv(YamiPanUiVertex *out, const TimSheetSprite *sprite, const float *uv)
{
    putUvTexels(out, (float)sprite->w * uv[0] + (float)sprite->u + 0.0f,
                (float)sprite->h * uv[1] + (float)sprite->v + 0.0f);
}

// SCISSOR_1 of a rect in window pixels: rounded, the far edges kept in
// the 512x512 target (x0 y0 from -256, x1 y1 up to 255), its origin
// moved to the target's centre.
static u_long scissorWord(float x0f, float y0f, float x1f, float y1f)
{
    int x0 = (int)(x0f + 0.5f);
    int y0 = (int)(y0f + 0.5f);
    int x1 = (int)(x1f + 0.5f);
    int y1 = (int)(y1f + 0.5f);
    if (x0 < -kUiScissorOrigin)
        x0 = -kUiScissorOrigin;
    if (y0 < -kUiScissorOrigin)
        y0 = -kUiScissorOrigin;
    if (x1 > kUiScissorOrigin - 1)
        x1 = kUiScissorOrigin - 1;
    if (y1 > kUiScissorOrigin - 1)
        y1 = kUiScissorOrigin - 1;
    return (u_long)(int64_t)(y1 + kUiScissorOrigin) << 48
         | (u_long)(int64_t)(y0 + kUiScissorOrigin) << 32
         | (u_long)(int64_t)(x0 + kUiScissorOrigin)
         | (u_long)(int64_t)(x1 + kUiScissorOrigin) << 16;
}

static void putRgbaq(YamiPanUiVertex *out, u_int rgbaq)
{
    out->rgba[0] = (u_char)rgbaq;
    out->rgba[1] = (u_char)(rgbaq >> 8);
    out->rgba[2] = (u_char)(rgbaq >> 16);
    out->rgba[3] = (u_char)(rgbaq >> 24);
}

// The add pass draws untextured and blended.
static inline u_long addPassPrim(u_long prim)
{
    return (prim & ~(u_long)(UiPrim_Textured | UiPrim_Blend)) | UiPrim_Blend;
}

void UiDrawer::closeGifRun()
{
    if (m_gifOpen) {
        // PC: the ROM closes the GIF tag (sceVif1PkCloseGifTag); the host
        // ends a run at the next call.
        m_gifOpen = 0;
        m_gifFormat = -1;
    }
}

void UiDrawer::resetState()
{
    m_packet = nullptr;
    m_texList = nullptr;
    for (int i = 0; i < kUiDrawerStackDepth; i++)
        m_stack[i].identity();
    m_top = 0;
    m_color.w = 1.0f;
    m_color.z = 1.0f;
    m_color.y = 1.0f;
    m_color.x = 1.0f;
    m_addColor.x = 0.0f;
    m_addColor.y = 0.0f;
    m_addColor.z = 0.0f;
    m_addColor.w = 0.0f;
    m_texBlock = 0;
    m_texWidth = 0;
    m_texHeight = 0;
}

// ──────────────────────────────────────────────────────────────────────
// UiMatrix
// FS2 0x0024dfa0
// callcheck: __extendsfdf2, FUN_00100230, FUN_00100250 (the angle compares as doubles)

void UiMatrix::interpolate(const UiMatrix &from, const UiMatrix &to, float t)
{
    float fromX = pointX(from, 0.0f, 0.0f);
    float fromY = pointY(from, 0.0f, 0.0f);
    float fromAxisXx = pointX(from, 1.0f, 0.0f) - fromX;
    float fromAxisXy = pointY(from, 1.0f, 0.0f) - fromY;
    float fromAxisYx = pointX(from, 0.0f, 1.0f) - fromX;
    float fromAxisYy = pointY(from, 0.0f, 1.0f) - fromY;
    float toX = pointX(to, 0.0f, 0.0f);
    float toY = pointY(to, 0.0f, 0.0f);
    float toAxisXx = pointX(to, 1.0f, 0.0f) - toX;
    float toAxisXy = pointY(to, 1.0f, 0.0f) - toY;
    float toAxisYx = pointX(to, 0.0f, 1.0f) - toX;
    float toAxisYy = pointY(to, 0.0f, 1.0f) - toY;

    float fromScaleX = sqrtf(fromAxisXx * fromAxisXx + fromAxisXy * fromAxisXy);
    float fromScaleY = sqrtf(fromAxisYx * fromAxisYx + fromAxisYy * fromAxisYy);
    float scaleX = t * (sqrtf(toAxisXx * toAxisXx + toAxisXy * toAxisXy) - fromScaleX) + fromScaleX + 0.0f;
    float fromAngle, toAngle;
    if (fromAxisXx * fromAxisYy - fromAxisXy * fromAxisYx < 0.0f) {
        // from is mirrored: x flipped, the angles of the flipped axes.
        scaleX = scaleX * -1.0f;
        fromAngle = atan2f(fromAxisXy, -fromAxisXx);
        toAngle = atan2f(toAxisXy, -toAxisXx);
    } else {
        fromAngle = atan2f(fromAxisXy, fromAxisXx);
        toAngle = atan2f(toAxisXy, toAxisXx);
    }
    // The shorter way round, and the result back into [-pi, pi].
    if ((double)(toAngle - fromAngle) > 3.141592653589793)
        toAngle -= 6.2831855f;
    else if ((double)(toAngle - fromAngle) < -3.141592653589793)
        toAngle += 6.2831855f;
    float angle = t * (toAngle - fromAngle) + fromAngle + 0.0f;
    if ((double)angle > 3.141592653589793)
        angle -= 6.2831855f;
    if ((double)angle < -3.141592653589793)
        angle += 6.2831855f;

    a = scaleX;
    b = 0.0f;
    tx = 0.0f;
    c = 0.0f;
    d = t * (sqrtf(toAxisYx * toAxisYx + toAxisYy * toAxisYy) - fromScaleY) + fromScaleY + 0.0f;
    ty = 0.0f;
    rotate(angle);
    tx = tx + (toX - fromX) * t + fromX;
    ty = ty + (toY - fromY) * t + fromY;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024e360
// callcheck: atan2f (tail call)

float UiMatrix::angle() const
{
    float ox = pointX(*this, 0.0f, 0.0f);
    float oy = pointY(*this, 0.0f, 0.0f);
    return atan2f(pointY(*this, 1.0f, 0.0f) - oy, pointX(*this, 1.0f, 0.0f) - ox);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024e3d0
// callcheck: identity (inline)

void UiMatrix::setScaleRotTrans(const UiMatrix &m, const float *scale)
{
    float ox = pointX(m, 0.0f, 0.0f);
    float oy = pointY(m, 0.0f, 0.0f);
    float ang = atan2f(pointY(m, 1.0f, 0.0f) - oy, pointX(m, 1.0f, 0.0f) - ox);
    UiMatrix s = { scale[0], 0.0f, 0.0f, 0.0f, scale[1], 0.0f };
    identity();
    concat(s);
    rotate(ang);
    tx += ox;
    ty += oy;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024e4f0

void uiTransformPoint(float *out, const UiMatrix *m, const float *in, int toFixed)
{
    out[0] = m->tx + in[0] * m->a + in[1] * m->b;
    out[1] = m->ty + in[0] * m->c + in[1] * m->d;
    if (toFixed) {
        out[0] = out[0] * 16.0f;
        out[1] = out[1] * 16.0f;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024e580

void UiMatrix::translate(const float *xy)
{
    tx = tx + xy[0];
    ty = ty + xy[1];
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024e5b0

void UiMatrix::setScale(float sx, float sy)
{
    a = sx;
    b = 0.0f;
    tx = 0.0f;
    c = 0.0f;
    d = sy;
    ty = 0.0f;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024e5d0

void UiMatrix::rotate(float angle)
{
    float a0 = a, b0 = b, tx0 = tx;
    float c0 = c, d0 = d, ty0 = ty;
    float cs = cosf(angle);
    float ns = -sinf(angle);
    float sn = sinf(angle);
    float cs2 = cosf(angle);
    a = cs * a0 + ns * c0;
    b = cs * b0 + ns * d0;
    tx = cs * tx0 + ns * ty0 + 0.0f;
    c = sn * a0 + cs2 * c0;
    d = sn * b0 + cs2 * d0;
    ty = sn * tx0 + cs2 * ty0 + 0.0f;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024e6d0

void UiMatrix::copy(const UiMatrix &m)
{
    a = m.a;
    b = m.b;
    tx = m.tx;
    c = m.c;
    d = m.d;
    ty = m.ty;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024e710

void UiMatrix::concat(const UiMatrix &m)
{
    float a0 = a, b0 = b, tx0 = tx;
    a = m.a * a0 + m.b * c;
    b = m.a * b0 + m.b * d;
    tx = m.tx + m.a * tx0 + m.b * ty;
    c = m.c * a0 + m.d * c;
    d = m.c * b0 + m.d * d;
    ty = m.ty + m.c * tx0 + m.d * ty;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024e7c0

void UiMatrix::multiply(const UiMatrix &l, const UiMatrix &r)
{
    a = l.a * r.a + l.b * r.c;
    b = l.a * r.b + l.b * r.d;
    tx = l.tx + l.a * r.tx + l.b * r.ty;
    c = l.c * r.a + l.d * r.c;
    d = l.c * r.b + l.d * r.d;
    ty = l.ty + l.c * r.tx + l.d * r.ty;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024e880

void UiMatrix::identity()
{
    a = 1.0f;
    b = 0.0f;
    tx = 0.0f;
    c = 0.0f;
    d = 1.0f;
    ty = 0.0f;
}

// ──────────────────────────────────────────────────────────────────────
// UiDrawer: the matrix
// FS2 0x0024e990

int UiDrawer::setMode(int mode)
{
    int old = m_mode;
    if (old != mode) {
        m_mode = mode;
        updateMatrix();
    }
    return old;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024e9d0

u_char UiDrawer::setPixelMode(u_char on)
{
    u_char old = m_pixelMode;
    if (on != old) {
        m_pixelMode = on;
        updateMatrix();
    }
    return old;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024ea10

void UiDrawer::setTranslate(const float *xy)
{
    m_translate[0] = xy[0];
    m_translate[1] = xy[1];
    updateMatrix();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024ea30

void UiDrawer::updateMatrix()
{
    const UiMatrix *screen = m_mode == 0 ? kUiScreenMatrix[0] : kUiScreenMatrix[1];
    const UiMatrix &top = m_stack[m_top];
    if (m_pixelMode == 0) {
        m_matrix.copy(top);
        m_matrix.concat(screen[1]);
        m_matrix.translate(m_translate);
    } else if (top.b == 0.0f && top.c == 0.0f) {
        m_matrix.copy(top);
        m_matrix.translate(m_translate);
    } else {
        UiMatrix m;
        m_matrix.copy(screen[0]);
        m.copy(top);
        m_matrix.concat(m);
        m_matrix.a = m_matrix.a * screen[1].a;
        m_matrix.b = m_matrix.b * screen[1].a;
        m_matrix.c = m_matrix.c * screen[1].d;
        m_matrix.d = m_matrix.d * screen[1].d;
        m_matrix.translate(m_translate);
    }
}

// ──────────────────────────────────────────────────────────────────────
// UiDrawer: text
// FS2 0x0024eba0
// callcheck: UiVec4, sceVif1PkOpenGifTag, sceVif1PkAddGsAD, sceVif1PkAddGsData, sceVif1PkCloseGifTag (the implicit copy; PC: YamiPan_DrawUiReg / Prim / Vertex)

int UiDrawer::drawText(UiFont *font, int /*message*/, const UiTextBox *box, float alignX, float alignY)
{
    const u_short *text = font->m_text;
    if ((uintptr_t)text & 1)
        return 0;
    int drawn = 0;
    if (text == nullptr)
        return 0;
    if (alignX < 0.0f)
        alignX = 0.0f;
    if (alignX > 1.0f)
        alignX = 1.0f;
    if (alignY < 0.0f)
        alignY = 0.0f;
    if (alignY > 1.0f)
        alignY = 1.0f;

    setTexture(font->m_texture);
    if (m_packet != nullptr) {
        setTexture(nullptr);
        YamiPan_DrawUiReg(m_packet, SCE_GS_TEX1_1, 0);
    }
    u_char rgbaq[4];
    if (isAddPass()) {
        m_addColor.toRgbaq(rgbaq);
    } else if (isFixedColor()) {
        m_color.toRgbaq(rgbaq);
    } else {
        UiVec4 color = box->rgba;
        color.multiply(m_color);
        color.toRgbaq(rgbaq);
    }
    u_long prim = kUiPrimText;
    if (m_packet != nullptr) {
        setTexture(nullptr);
        if (m_addPass)
            prim = addPassPrim(prim);
        YamiPan_DrawUiPrim(m_packet, prim, rgbaq);
    }

    // The box through the matrix (the glyphs are not: they keep their
    // texel size), the text block aligned in it.
    float topLeft[2], bottomRight[2];
    uiTransformPoint(topLeft, &m_matrix, box->topLeft, 0);
    uiTransformPoint(bottomRight, &m_matrix, box->bottomRight, 0);
    const FontData *glyphs = (const FontData *)font->m_glyphs;
    float spacingX = font->m_spacing[0];
    float spacingY = font->m_spacing[1];
    float boxHeight = bottomRight[1] - topLeft[1];
    int lines = glyphs->countLines(text);
    float y = alignY * (boxHeight - ((float)lines * ((float)glyphs->m_lineHeight / 16.0f)
                                     - spacingY * (float)(lines - 1)));
    if (y < 0.0f)
        y = 0.0f;
    y = y + topLeft[1];
    if (openVertices(UiVertex_Uv) < 0)
        return -1;

    // Line by line, each aligned on its own width; a glyph is a sprite
    // from its rect's corner to the opposite one.
    while (text != nullptr) {
        float x = alignX * ((bottomRight[0] - topLeft[0]) - glyphs->lineWidth(spacingX, text));
        if (x < 0.0f)
            x = 0.0f;
        x = x + topLeft[0];
        for (;;) {
            u_short c = *text;
            if (c == 0) {
                text = nullptr;
                break;
            }
            if (c == '\n') {
                text++;
                y = y + ((float)glyphs->m_lineHeight / 16.0f - spacingY);
                break;
            }
            if (c < 0x20) {
                text++;
                continue;
            }
            FontGlyph glyph;
            text = glyphs->nextGlyph(text, &glyph);
            if (text == nullptr)
                break;
            float u = (float)glyph.u / 16.0f;
            float v = (float)glyph.v / 16.0f;
            float w = (float)glyph.w / 16.0f;
            float h = (float)glyph.h / 16.0f;
            float x0 = clampPixel(x);
            float y0 = clampPixel(y);
            YamiPanUiVertex corner = {};
            putUvTexels(&corner, u, v);
            putXyzPixels(&corner, x0, y0);
            YamiPan_DrawUiVertex(m_packet, &corner);
            corner = {};
            putUvTexels(&corner, u + w, v + h);
            putXyzPixels(&corner, x0 + w, y0 + h);
            YamiPan_DrawUiVertex(m_packet, &corner);
            drawn++;
            x = x + (w - spacingX);
        }
    }
    return drawn;
}

// ──────────────────────────────────────────────────────────────────────
// UiVec4
// FS2 0x0024f2c0

void UiVec4::multiply(const UiVec4 &v)
{
    x = x * v.x;
    y = y * v.y;
    z = z * v.z;
    w = w * v.w;
}

// FS2 0x0024f310 UiVec4::UiVec4 (implicit copy constructor)

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024f340

u_char UiDrawer::isFixedColor() const
{
    return m_fixedColor;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024f350

void UiVec4::toRgbaq(u_char *out) const
{
    out[0] = colorByte(x, kUiColorLimit, 256.0f);
    out[1] = colorByte(y, kUiColorLimit, 256.0f);
    out[2] = colorByte(z, kUiColorLimit, 256.0f);
    out[3] = colorByte(w, kUiAlphaLimit, 128.0f);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024f570

u_char UiDrawer::isAddPass() const
{
    return m_addPass;
}

// ──────────────────────────────────────────────────────────────────────
// UiDrawer: textures
// FS2 0x0024f580
// callcheck: operator_new, GsTexture, place, upload, putTex0, getSize, sceVif1PkCloseGifTag, sceVif1PkCloseDirectCode, sceVif1PkCloseDmaTag, sceVif1PkCnt, sceVif1PkAddCode, sceVif1PkOpenDirectCode (PC: the GS side through the host; the inline closeDirect / closeGifRun)

int UiDrawer::setImage(const void *image)
{
    if (m_packet == nullptr)
        return -1;
    GsTexture *tex = nullptr;
    if (image == nullptr) {
        if (m_directOpen) {
            closeGifRun();
            return 0;
        }
    } else {
        tex = new GsTexture(image);
        closeDirect();
        // PC: TexAreaList::place / GsTexture::upload / putTex0(TEX0_1):
        // no GS memory; the host decodes the image now (its owner dies
        // below).
        YamiPan_DrawUiTexture(m_packet, tex);
        tex->getSize(&m_texWidth, &m_texHeight);
    }
    // PC: the ROM opens a CNT tag, a NOP and a DIRECT code here.
    if (tex != nullptr)
        delete tex;
    m_directOpen = 1;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024f7a0
// callcheck: place, upload, putTex0, getSize, sceVif1PkCloseGifTag, sceVif1PkCloseDirectCode, sceVif1PkCloseDmaTag, sceVif1PkCnt, sceVif1PkAddCode, sceVif1PkOpenDirectCode (PC: the GS side through the host; the inline closeDirect / closeGifRun)

int UiDrawer::setTexture(GsTexture *tex)
{
    if (m_packet == nullptr)
        return -1;
    if (tex == nullptr) {
        if (m_directOpen) {
            closeGifRun();
            return 0;
        }
    } else {
        closeDirect();
        // PC: TexAreaList::place(m_texList, &m_texBlock, tex), the upload
        // when newly placed and putTex0(TEX0_1): no GS memory; the host
        // binds the texture by its owner.
        YamiPan_DrawUiTexture(m_packet, tex);
        tex->getSize(&m_texWidth, &m_texHeight);
    }
    // PC: the ROM opens a CNT tag, a NOP and a DIRECT code here.
    m_directOpen = 1;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024f960
// callcheck: sceVif1PkCloseGifTag, sceVif1PkCloseDirectCode, sceVif1PkCloseDmaTag (PC: no packet)

int UiDrawer::closeDirect()
{
    if (!m_directOpen)
        return 0;
    closeGifRun();
    m_directOpen = 0;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// UiDrawer: begin / end
// FS2 0x0024f9e0
// callcheck: identity, sceVif1PkCloseGifTag, sceVif1PkCloseDirectCode, sceVif1PkCloseDmaTag (the inline closeDirect / resetState)

void UiDrawer::end()
{
    setDrawPreset(UiPreset_DrawEnv);
    closeDirect();
    resetState();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024fad0

void UiDrawer::begin(sceVif1Packet *packet, TexAreaList *texList)
{
    m_packet = packet;
    m_texList = texList;
    setDrawPreset(UiPreset_Normal);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024fae0
// callcheck: operator_delete (the deleting destructor)

UiDrawer::~UiDrawer()
{
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024fb30
// callcheck: identity, updateMatrix (the inline resetState and setMode)

UiDrawer::UiDrawer()
{
    resetState();
    m_preset = -1;
    m_blend = -1;
    m_gifFormat = -1;
    m_directOpen = 0;
    m_gifOpen = 0;
    m_addPass = 0;
    m_savedPreset = -1;
    m_savedBlend = -1;
    m_fixedColor = 0;
    m_translate[0] = 0.0f;
    m_translate[1] = 0.0f;
    m_mode = 2;
    m_pixelMode = 0;
    setMode(0);
}

// ──────────────────────────────────────────────────────────────────────
// UiDrawer: state
// FS2 0x0024fc20

u_char UiDrawer::setAddPass(u_char on)
{
    u_char old = m_addPass;
    if (on != old) {
        if (on == 0) {
            setDrawPreset(m_savedPreset);
            setBlend(m_savedBlend);
            m_addPass = 0;
        } else {
            m_savedPreset = m_preset;
            m_savedBlend = m_blend;
            setBlend(UiBlend_AddDestAlpha);
            m_addPass = on;
        }
    }
    return old;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024fcb0
// callcheck: sceVif1PkOpenGifTag, sceVif1PkAddGsAD, sceVif1PkCloseGifTag (PC: YamiPan_DrawUiReg)

int UiDrawer::setGsReg(int reg, u_long value)
{
    if (m_packet == nullptr)
        return -1;
    setTexture(nullptr);
    YamiPan_DrawUiReg(m_packet, reg, value);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024fd40
// callcheck: UiVec4, sceVif1PkOpenGifTag, sceVif1PkAddGsData, sceVif1PkCloseGifTag (the implicit copy; PC: YamiPan_DrawUiPrim)

int UiDrawer::setPrimColor(u_long prim, const UiVec4 *rgba)
{
    if (m_packet == nullptr)
        return -1;
    setTexture(nullptr);
    if (m_addPass)
        prim = addPassPrim(prim);
    u_char rgbaq[4];
    if (isAddPass()) {
        m_addColor.toRgbaq(rgbaq);
    } else if (isFixedColor()) {
        m_color.toRgbaq(rgbaq);
    } else {
        UiVec4 color = *rgba;
        color.multiply(m_color);
        color.toRgbaq(rgbaq);
    }
    YamiPan_DrawUiPrim(m_packet, prim, rgbaq);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024fe60
// callcheck: sceVif1PkOpenGifTag, sceVif1PkAddGsData, sceVif1PkCloseGifTag (PC: YamiPan_DrawUiPrim)

int UiDrawer::setPrim(u_long prim)
{
    if (m_packet == nullptr)
        return -1;
    setTexture(nullptr);
    if (m_addPass)
        prim = addPassPrim(prim);
    YamiPan_DrawUiPrim(m_packet, prim, nullptr);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024fef0
// callcheck: sceVif1PkOpenGifTag, sceVif1PkAddGsAD, sceVif1PkCloseGifTag (PC: YamiPan_DrawUiReg)

int UiDrawer::setTexRepeat(u_char on)
{
    if (m_packet == nullptr)
        return -1;
    setTexture(nullptr);
    YamiPan_DrawUiReg(m_packet, SCE_GS_CLAMP_1, on == 0 ? kUiClampClamp : kUiClampRepeat);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024ffa0
// callcheck: sceVif1PkOpenGifTag, sceVif1PkAddGsAD, sceVif1PkCloseGifTag (PC: YamiPan_DrawUiReg)

int UiDrawer::setBlend(int mode)
{
    if (m_packet == nullptr)
        return -1;
    if (m_blend == mode)
        return 0;
    setTexture(nullptr);
    m_blend = mode;
    if ((u_int)m_blend > UiBlend_NormalDestAlpha)
        return -2;
    YamiPan_DrawUiReg(m_packet, SCE_GS_ALPHA_1, kUiBlendAlpha[m_blend]);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00250130
// callcheck: gfxUiPresetNormal, gfxUiPresetAlphaOnly1, gfxUiPresetAlphaOnly2, gfxUiPresetRgbOnly3, gfxUiPresetRgbOnly4, gfxUiPresetRgbOnly5, gfxUiPresetDrawEnv

int UiDrawer::setDrawPreset(int preset)
{
    if (m_packet == nullptr)
        return -1;
    if (m_preset == preset)
        return 0;
    setTexture(nullptr);
    m_preset = preset;
    switch (m_preset) {
    case UiPreset_Normal:
        gfxUiPresetNormal(m_packet);
        break;
    case UiPreset_AlphaOnly1:
        gfxUiPresetAlphaOnly1(m_packet);
        break;
    case UiPreset_AlphaOnly2:
        gfxUiPresetAlphaOnly2(m_packet);
        break;
    case UiPreset_RgbOnly3:
        gfxUiPresetRgbOnly3(m_packet);
        break;
    case UiPreset_RgbOnly4:
        gfxUiPresetRgbOnly4(m_packet);
        break;
    case UiPreset_RgbOnly5:
        gfxUiPresetRgbOnly5(m_packet);
        break;
    case UiPreset_DrawEnv:
        gfxUiPresetDrawEnv(m_packet);
        break;
    default:
        return -2;
    }
    m_blend = UiBlend_Normal;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0024e8a0
// callcheck: sceVif1PkOpenGifTag, sceVif1PkAddGsAD, sceVif1PkCloseGifTag (PC: YamiPan_DrawUiReg; the host copies on TRXDIR)

int UiDrawer::copyTarget(u_char toBackup)
{
    if (m_packet == nullptr)
        return -1;
    setTexture(nullptr);
    const UiGsRegWrite *regs = toBackup == 0 ? kUiCopyFromBackup : kUiCopyToBackup;
    for (int i = 0; i < 4; i++)
        YamiPan_DrawUiReg(m_packet, regs[i].reg, regs[i].value);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002503c0
// callcheck: sceVif1PkOpenGifTag, sceVif1PkAddGsAD, sceVif1PkCloseGifTag (PC: YamiPan_DrawUiReg)

int UiDrawer::setScissorPixels(const float *rect)
{
    if (m_packet == nullptr)
        return -1;
    setTexture(nullptr);
    YamiPan_DrawUiReg(m_packet, SCE_GS_SCISSOR_1, scissorWord(rect[0], rect[1], rect[2], rect[3]));
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00250520
// callcheck: sceVif1PkOpenGifTag, sceVif1PkAddGsAD, sceVif1PkCloseGifTag (PC: YamiPan_DrawUiReg)

int UiDrawer::setScissor(const float *rect)
{
    if (m_packet == nullptr)
        return -1;
    setTexture(nullptr);
    float p0[2], p1[2];
    uiTransformPoint(p0, &m_matrix, rect, 0);
    uiTransformPoint(p1, &m_matrix, rect + 2, 0);
    YamiPan_DrawUiReg(m_packet, SCE_GS_SCISSOR_1, scissorWord(p0[0], p0[1], p1[0], p1[1]));
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002506c0
// callcheck: sceVif1PkOpenGifTag, sceVif1PkAddGsAD, sceVif1PkCloseGifTag (PC: YamiPan_DrawUiReg)

int UiDrawer::resetScissor()
{
    if (m_packet == nullptr)
        return -1;
    setTexture(nullptr);
    GfxDrawEnv *env = gfxGetCurrentDrawEnv();
    const sceGsDrawEnv1 *regs = env != nullptr ? &env->env : nullptr;
    YamiPan_DrawUiReg(m_packet, SCE_GS_SCISSOR_1, *(const u_long *)&regs->scissor1);
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// UiDrawer: vertices
// FS2 0x00250240
// callcheck: sceVif1PkCloseGifTag, sceVif1PkOpenGifTag (the inline closeGifRun; PC: YamiPan_DrawUiBegin for the tags 0x307660..0x3076d0)

int UiDrawer::openVertices(int format)
{
    if (m_packet == nullptr)
        return -1;
    if (!m_gifOpen) {
        setTexture(nullptr);
    } else {
        if (format == m_gifFormat)
            return 0;
        closeGifRun();
    }
    if ((u_int)format >= UiVertex_FormatCount)
        return -1;
    YamiPan_DrawUiBegin(m_packet, format);
    m_gifFormat = format;
    m_gifOpen = 1;
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00250750
// callcheck: uiTransformPoint, sceVif1PkAddGsData (in putXyz; PC: YamiPan_DrawUiVertex)

int UiDrawer::drawSheetFlat(const UiTexVertex *verts, int count, const TimSheetSprite *sprite, int grouped)
{
    if (openVertices(grouped == 0 ? UiVertex_Uv : UiVertex_Grouped | UiVertex_Uv) < 0)
        return -1;
    for (int i = 0; i < count; i++) {
        YamiPanUiVertex v = {};
        putSheetUv(&v, sprite, verts[i].uv);
        putXyz(&v, &m_matrix, verts[i].xy);
        YamiPan_DrawUiVertex(m_packet, &v);
    }
    return count;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002509b0
// callcheck: uiTransformPoint, uiPkAddGsData, sceVif1PkAddGsData (in putXyz; PC: YamiPan_DrawUiVertex)

int UiDrawer::drawSheetGouraud(const UiTexColorVertex *verts, int count, const TimSheetSprite *sprite, int grouped)
{
    if (openVertices(grouped == 0 ? UiVertex_Rgbaq | UiVertex_Uv
                                  : UiVertex_Grouped | UiVertex_Rgbaq | UiVertex_Uv) < 0)
        return -1;
    for (int i = 0; i < count; i++) {
        YamiPanUiVertex v = {};
        putRgbaq(&v, vertexColor(&verts[i].rgba));
        putSheetUv(&v, sprite, verts[i].uv);
        putXyz(&v, &m_matrix, verts[i].xy);
        YamiPan_DrawUiVertex(m_packet, &v);
    }
    return count;
}

// ──────────────────────────────────────────────────────────────────────
// An out-of-line jump to sceVif1PkAddGsData (the gouraud draws' RGBAQ
// word); the host takes the colour with the vertex, nothing to write.
// FS2 0x00250c30 uiPkAddGsData (library thunk)

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00250c40

u_int UiDrawer::vertexColor(const UiVec4 *rgba) const
{
    UiVec4 color;
    if (m_addPass) {
        color = m_addColor;
    } else if (m_fixedColor) {
        color = m_color;
    } else {
        color.x = rgba->x * m_color.x;
        color.y = rgba->y * m_color.y;
        color.z = rgba->z * m_color.z;
        color.w = rgba->w * m_color.w;
    }
    u_char rgbaq[4];
    color.toRgbaq(rgbaq);
    return rgbaq[0] | rgbaq[1] << 8 | rgbaq[2] << 16 | (u_int)rgbaq[3] << 24;
}

// ──────────────────────────────────────────────────────────────────────
// The wide strips' half-width as a scale of each pair's cross vector:
// width / 2 over the first pair's length (both through the matrix).
static float stripScale(const UiMatrix *m, float width, const float *p, const float *q)
{
    float a[2], b[2];
    uiTransformPoint(a, m, p, 0);
    uiTransformPoint(b, m, q, 0);
    float dx = a[0] - b[0];
    float dy = a[1] - b[1];
    return (width / 2.0f) / sqrtf(dx * dx + dy * dy);
}

// FS2 0x00251350
// callcheck: sceVif1PkAddGsData (in putXyz; PC: YamiPan_DrawUiVertex)

int UiDrawer::drawTexStripFlat(float width, const UiVertex *verts, int count, const UiVec4 *uvRect)
{
    if (openVertices(UiVertex_Uv) < 0)
        return -1;
    const float uv0[2] = { uvRect->x, uvRect->y };
    const float uv1[2] = { uvRect->z, uvRect->w };
    if (width == 0.0f) {
        for (int i = 0; i < count; i++) {
            YamiPanUiVertex v = {};
            putUv(&v, uv0, m_texWidth, m_texHeight);
            putXyz(&v, &m_matrix, verts[i].xy);
            YamiPan_DrawUiVertex(m_packet, &v);
        }
    } else {
        float scale = stripScale(&m_matrix, width, verts[0].xy, verts[1].xy);
        for (int i = 0; i < count; i += 2, verts += 2) {
            float p[2], q[2];
            uiTransformPoint(p, &m_matrix, verts[0].xy, 0);
            uiTransformPoint(q, &m_matrix, verts[1].xy, 0);
            float offX = (q[0] - p[0]) * scale;
            float offY = (q[1] - p[1]) * scale;
            YamiPanUiVertex v = {};
            putUv(&v, uv0, m_texWidth, m_texHeight);
            putXyzPixels(&v, p[0] + offX, p[1] + offY);
            YamiPan_DrawUiVertex(m_packet, &v);
            v = {};
            putUv(&v, uv1, m_texWidth, m_texHeight);
            putXyzPixels(&v, p[0] - offX, p[1] - offY);
            YamiPan_DrawUiVertex(m_packet, &v);
        }
    }
    return count;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002519a0
// callcheck: uiPkAddGsData, sceVif1PkAddGsData (in putXyz; PC: YamiPan_DrawUiVertex)

int UiDrawer::drawTexStripGouraud(float width, const UiColorVertex *verts, int count, const UiVec4 *uvRect)
{
    if (openVertices(UiVertex_Rgbaq | UiVertex_Uv) < 0)
        return -1;
    const float uv0[2] = { uvRect->x, uvRect->y };
    const float uv1[2] = { uvRect->z, uvRect->w };
    if (width == 0.0f) {
        for (int i = 0; i < count; i++) {
            YamiPanUiVertex v = {};
            putRgbaq(&v, vertexColor(&verts[i].rgba));
            putUv(&v, uv0, m_texWidth, m_texHeight);
            putXyz(&v, &m_matrix, verts[i].xy);
            YamiPan_DrawUiVertex(m_packet, &v);
        }
    } else {
        float scale = stripScale(&m_matrix, width, verts[0].xy, verts[1].xy);
        for (int i = 0; i < count; i += 2, verts += 2) {
            float p[2], q[2];
            uiTransformPoint(p, &m_matrix, verts[0].xy, 0);
            uiTransformPoint(q, &m_matrix, verts[1].xy, 0);
            float offX = (q[0] - p[0]) * scale;
            float offY = (q[1] - p[1]) * scale;
            YamiPanUiVertex v = {};
            putRgbaq(&v, vertexColor(&verts[0].rgba));
            putUv(&v, uv0, m_texWidth, m_texHeight);
            putXyzPixels(&v, p[0] + offX, p[1] + offY);
            YamiPan_DrawUiVertex(m_packet, &v);
            v = {};
            putRgbaq(&v, vertexColor(&verts[1].rgba));
            putUv(&v, uv1, m_texWidth, m_texHeight);
            putXyzPixels(&v, p[0] - offX, p[1] - offY);
            YamiPan_DrawUiVertex(m_packet, &v);
        }
    }
    return count;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00252040
// callcheck: uiTransformPoint, sceVif1PkAddGsData (in putXyz; PC: YamiPan_DrawUiVertex)

int UiDrawer::drawTexFlat(const UiTexVertex *verts, int count, int grouped)
{
    if (openVertices(grouped == 0 ? UiVertex_Uv : UiVertex_Grouped | UiVertex_Uv) < 0)
        return -1;
    for (int i = 0; i < count; i++) {
        YamiPanUiVertex v = {};
        putUv(&v, verts[i].uv, m_texWidth, m_texHeight);
        putXyz(&v, &m_matrix, verts[i].xy);
        YamiPan_DrawUiVertex(m_packet, &v);
    }
    return count;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00252260
// callcheck: uiTransformPoint, uiPkAddGsData, sceVif1PkAddGsData (in putXyz; PC: YamiPan_DrawUiVertex)

int UiDrawer::drawTexGouraud(const UiTexColorVertex *verts, int count, int grouped)
{
    if (openVertices(grouped == 0 ? UiVertex_Rgbaq | UiVertex_Uv
                                  : UiVertex_Grouped | UiVertex_Rgbaq | UiVertex_Uv) < 0)
        return -1;
    for (int i = 0; i < count; i++) {
        YamiPanUiVertex v = {};
        putRgbaq(&v, vertexColor(&verts[i].rgba));
        putUv(&v, verts[i].uv, m_texWidth, m_texHeight);
        putXyz(&v, &m_matrix, verts[i].xy);
        YamiPan_DrawUiVertex(m_packet, &v);
    }
    return count;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002524a0
// callcheck: uiTransformPoint, uiPkAddGsData, sceVif1PkAddGsData (in putXyz; PC: YamiPan_DrawUiVertex)

int UiDrawer::drawGouraud(const UiColorVertex *verts, int count, int grouped)
{
    if (openVertices(grouped == 0 ? UiVertex_Rgbaq : UiVertex_Grouped | UiVertex_Rgbaq) < 0)
        return -1;
    for (int i = 0; i < count; i++) {
        YamiPanUiVertex v = {};
        putRgbaq(&v, vertexColor(&verts[i].rgba));
        putXyz(&v, &m_matrix, verts[i].xy);
        YamiPan_DrawUiVertex(m_packet, &v);
    }
    return count;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00252680
// callcheck: uiTransformPoint, sceVif1PkAddGsData (in putXyz; PC: YamiPan_DrawUiVertex)

int UiDrawer::drawFlat(const UiVertex *verts, int count, int grouped)
{
    if (openVertices(grouped == 0 ? UiVertex_Xy : UiVertex_Grouped | UiVertex_Xy) < 0)
        return -1;
    for (int i = 0; i < count; i++) {
        YamiPanUiVertex v = {};
        putXyz(&v, &m_matrix, verts[i].xy);
        YamiPan_DrawUiVertex(m_packet, &v);
    }
    return count;
}
