// game/gsutil.cpp -- the 2D layouts' draw-state presets and GsTexture
// (game/gsutil.h).
//
// PC: the ROM's presets write A+D runs into the packet; here the same
// register values go to the host UI draw module
// (yamipan/rendering/yamipan_drawui.h), keyed by that packet, as
// game/uidrawer.cpp does for the drawer's own state.

#include "game/gsutil.h"

#include "game/gfxtarget.h"
#include "sdk/libgraph.h"
#include "sdk/libvifpk.h"
#include "sdk/scetypes.h"
#include "yamipan/rendering/yamipan_drawui.h"

// One A+D register write of a preset table (0x3071a0..0x3072f0).
// layout: host -- the ROM's {u_int reg, pad, u_long value} qwords.
struct GsRegWrite
{
    int    reg;
    u_long value;
};

// TEST_1: depth test on and always passing (ZTE 1, ZTST ALWAYS), with
// the presets' alpha tests.
constexpr u_long kTestNone = SCE_GS_SET_TEST(0, 0, 0, 0, 0, 0, 1, 1);
constexpr u_long kTestAlwaysPass = SCE_GS_SET_TEST(1, 1, 0, 0, 0, 0, 1, 1);
constexpr u_long kTestAlphaGequal80 = SCE_GS_SET_TEST(1, 5, 0x80, 0, 0, 0, 1, 1);
// ATST NEVER with AFAIL RGB_ONLY: every pixel writes colour only, and
// only where the frame's alpha bit is 0 (DATE, DATM 0) or 1 (DATM 1).
constexpr u_long kTestRgbOnlyDestAlpha0 = SCE_GS_SET_TEST(1, 0, 0, 3, 1, 0, 1, 1);
constexpr u_long kTestRgbOnlyDestAlpha1 = SCE_GS_SET_TEST(1, 0, 0, 3, 1, 1, 1, 1);

// ALPHA_1: (Cs - Cd) * As + Cd, and (Cs - Cd) * Ad + Cd.
constexpr u_long kAlphaNormal = SCE_GS_SET_ALPHA(0, 1, 0, 1, 0);
constexpr u_long kAlphaDestAlpha = SCE_GS_SET_ALPHA(0, 1, 1, 1, 0x80);
// TEX1_1: LCM 1, bilinear magnify and minify.
constexpr u_long kTex1Linear = SCE_GS_SET_TEX1(1, 0, 1, 1, 0, 0, 0);

static const GsRegWrite kPresetNormal[6] = {           // 0x3071a0
    { SCE_GS_TEST_1, kTestNone },
    { SCE_GS_ALPHA_1, kAlphaNormal },
    { SCE_GS_CLAMP_1, SCE_GS_SET_CLAMP(0, 0, 0, 0, 0, 0) },
    { SCE_GS_TEX1_1, kTex1Linear },
    { SCE_GS_COLCLAMP, 1 },
    { SCE_GS_CLAMP_1, SCE_GS_SET_CLAMP(1, 1, 0, 0, 0, 0) },
};
static const GsRegWrite kPresetAlphaOnly1[3] = {       // 0x307200
    { SCE_GS_TEST_1, kTestAlwaysPass },
    { SCE_GS_ALPHA_1, kAlphaNormal },
    { SCE_GS_TEX1_1, kTex1Linear },
};
static const GsRegWrite kPresetAlphaOnly2[3] = {       // 0x307230
    { SCE_GS_TEST_1, kTestAlphaGequal80 },
    { SCE_GS_ALPHA_1, kAlphaNormal },
    { SCE_GS_TEX1_1, kTex1Linear },
};
static const GsRegWrite kPresetRgbOnly3[3] = {         // 0x307260
    { SCE_GS_TEST_1, kTestRgbOnlyDestAlpha0 },
    { SCE_GS_ALPHA_1, kAlphaNormal },
    { SCE_GS_TEX1_1, kTex1Linear },
};
static const GsRegWrite kPresetRgbOnly4[3] = {         // 0x307290
    { SCE_GS_TEST_1, kTestRgbOnlyDestAlpha1 },
    { SCE_GS_ALPHA_1, kAlphaNormal },
    { SCE_GS_TEX1_1, kTex1Linear },
};
static const GsRegWrite kPresetRgbOnly5[3] = {         // 0x3072c0
    { SCE_GS_TEST_1, kTestNone },
    { SCE_GS_ALPHA_1, kAlphaDestAlpha },
    { SCE_GS_TEX1_1, kTex1Linear },
};

// FRAME_1's FBMSK (bits 32..63: a set bit is not written).
constexpr u_long kFrameMaskBits = 0xffffffff00000000ull;
constexpr u_long kFrameWriteAll = 0;
constexpr u_long kFrameAlphaOnly = 0x00ffffffull << 32;
constexpr u_long kFrameRgbOnly = 0xff000000ull << 32;
// ZBUF_1's ZMSK.
constexpr u_long kZbufNoWrite = 1ull << 32;

// GsTexture's render targets: 512x512, TBW 8, TCC on, TFX modulate.
constexpr int    kGsTargetSize = 512;
constexpr u_long kTex0TargetZbuf = SCE_GS_SET_TEX0(0x1800, 8, SCE_GS_PSMCT32, 9, 9, 1, 0, 0, 0, 0, 0, 0);
constexpr u_long kTex0TargetDraw = SCE_GS_SET_TEX0(0x800, 8, SCE_GS_PSMCT32, 9, 9, 1, 0, 0, 0, 0, 0, 0);
constexpr u_long kTex0TargetView = SCE_GS_SET_TEX0(0, 8, SCE_GS_PSMCT16, 9, 9, 1, 0, 0, 0, 0, 0, 0);

// ──────────────────────────────────────────────────────────────────────
// Helpers, inline in the ROM.

// The current draw env's registers (the ROM's base adjustment keeps null).
static const sceGsDrawEnv1 *currentEnv()
{
    GfxDrawEnv *env = gfxGetCurrentDrawEnv();
    return env != nullptr ? &env->env : nullptr;
}

// A preset's fixed run, then FRAME_1 with frameMask and ZBUF_1 without
// depth writes, each in its own A+D tag.
static void putPreset(sceVif1Packet *packet, const GsRegWrite *regs, int count, u_long frameMask)
{
    for (int i = 0; i < count; i++)
        YamiPan_DrawUiReg(packet, regs[i].reg, regs[i].value);
    const sceGsDrawEnv1 *env = currentEnv();
    u_long frame = (*(const u_long *)&env->frame1 & ~kFrameMaskBits) | frameMask;
    u_long zbuf = *(const u_long *)&env->zbuf1 | kZbufNoWrite;
    YamiPan_DrawUiReg(packet, SCE_GS_FRAME_1, frame);
    YamiPan_DrawUiReg(packet, SCE_GS_ZBUF_1, zbuf);
}

// ──────────────────────────────────────────────────────────────────────
// The presets
// FS2 0x00230980
// callcheck: sceVif1PkOpenGifTag, sceVif1PkAddGsAD, sceVif1PkCloseGifTag, gfxGetCurrentDrawEnv (PC: YamiPan_DrawUiReg; the env through currentEnv)

void gfxUiPresetRgbOnly5(sceVif1Packet *packet)
{
    putPreset(packet, kPresetRgbOnly5, 3, kFrameRgbOnly);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00230ab0
// callcheck: sceVif1PkOpenGifTag, sceVif1PkAddGsAD, sceVif1PkCloseGifTag, gfxGetCurrentDrawEnv (PC: YamiPan_DrawUiReg; the env through currentEnv)

void gfxUiPresetRgbOnly4(sceVif1Packet *packet)
{
    putPreset(packet, kPresetRgbOnly4, 3, kFrameRgbOnly);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00230be0
// callcheck: sceVif1PkOpenGifTag, sceVif1PkAddGsAD, sceVif1PkCloseGifTag, gfxGetCurrentDrawEnv (PC: YamiPan_DrawUiReg; the env through currentEnv)

void gfxUiPresetRgbOnly3(sceVif1Packet *packet)
{
    putPreset(packet, kPresetRgbOnly3, 3, kFrameRgbOnly);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00230d10
// callcheck: sceVif1PkOpenGifTag, sceVif1PkAddGsAD, sceVif1PkCloseGifTag, gfxGetCurrentDrawEnv (PC: YamiPan_DrawUiReg; the env through currentEnv)

void gfxUiPresetAlphaOnly2(sceVif1Packet *packet)
{
    putPreset(packet, kPresetAlphaOnly2, 3, kFrameAlphaOnly);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00230e40
// callcheck: sceVif1PkOpenGifTag, sceVif1PkAddGsAD, sceVif1PkCloseGifTag, gfxGetCurrentDrawEnv (PC: YamiPan_DrawUiReg; the env through currentEnv)

void gfxUiPresetAlphaOnly1(sceVif1Packet *packet)
{
    putPreset(packet, kPresetAlphaOnly1, 3, kFrameAlphaOnly);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00230f70
// callcheck: sceVif1PkOpenGifTag, sceVif1PkAddGsAD, sceVif1PkCloseGifTag, gfxGetCurrentDrawEnv (PC: YamiPan_DrawUiReg; the env through currentEnv)

void gfxUiPresetDrawEnv(sceVif1Packet *packet)
{
    for (int i = 0; i < 6; i++)
        YamiPan_DrawUiReg(packet, kPresetNormal[i].reg, kPresetNormal[i].value);
    const sceGsDrawEnv1 *env = currentEnv();
    YamiPan_DrawUiReg(packet, SCE_GS_FRAME_1, *(const u_long *)&env->frame1);
    YamiPan_DrawUiReg(packet, SCE_GS_ZBUF_1, *(const u_long *)&env->zbuf1);
    // The ROM asks for the env again here.
    env = currentEnv();
    YamiPan_DrawUiReg(packet, SCE_GS_SCISSOR_1, *(const u_long *)&env->scissor1);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00231090
// callcheck: sceVif1PkOpenGifTag, sceVif1PkAddGsAD, sceVif1PkCloseGifTag, gfxGetCurrentDrawEnv (PC: YamiPan_DrawUiReg; the env through currentEnv)

void gfxUiPresetNormal(sceVif1Packet *packet)
{
    putPreset(packet, kPresetNormal, 6, kFrameWriteAll);
}

// ──────────────────────────────────────────────────────────────────────
// GsTexture
// FS2 0x002311c0

void GsTexture::getSize(int *width, int *height)
{
    if (!m_isTarget) {
        *width = m_width;
        *height = m_height;
    } else {
        *width = m_targetWidth;
        *height = m_targetHeight;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00231200
// callcheck: putTex0 (the base's, Tim2Texture::putTex0)

void GsTexture::putTex0(u_long **cursor, int block, u_int reg)
{
    if (!m_isTarget) {
        Tim2Texture::putTex0(cursor, block, reg);
        return;
    }
    u_long *p = *cursor;
    p[0] = kTexPktDmaCnt2;
    p[1] = kTexPktVifFlushDirect2;
    p[2] = kTexPktGifTagAd1;
    p[3] = kGifRegAd;
    p[4] = m_targetTex0;
    p[5] = reg;
    *cursor = p + 6;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00231290

int GsTexture::readTarget(const void *image)
{
    int found = 1;
    m_isTarget = 1;
    switch (((const int *)image)[3]) {
    case GsTargetTag_Zbuf:
        m_isTarget = 1;
        m_targetTex0 = kTex0TargetZbuf;
        m_targetWidth = kGsTargetSize;
        m_targetHeight = kGsTargetSize;
        break;
    case GsTargetTag_Draw:
        m_isTarget = 1;
        m_targetTex0 = kTex0TargetDraw;
        m_targetWidth = kGsTargetSize;
        m_targetHeight = kGsTargetSize;
        break;
    case GsTargetTag_View:
        m_isTarget = 1;
        m_targetTex0 = kTex0TargetView;
        m_targetWidth = kGsTargetSize;
        m_targetHeight = kGsTargetSize;
        break;
    default:
        m_isTarget = 0;
        found = 0;
        break;
    }
    return found;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00231360
// callcheck: Tim2Texture, readTarget (the base constructor)

GsTexture::GsTexture(const void *image)
    : Tim2Texture(image)
{
    m_isTarget = 0;
    readTarget(image);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002313b0
// callcheck: ~Tim2Texture, operator_delete (the base destructor and the deleting variant, compiler generated)

GsTexture::~GsTexture()
{
}
