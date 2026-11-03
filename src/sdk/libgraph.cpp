// sdk/libgraph.cpp -- the GS library on the host. There is no GS: the Set* calls still fill the structures
// the game reads back (its draw env is where its presets take FRAME and ZBUF from), and everything that would
// have reached the GS ends here. The vblank is the one thing that really happens.
#include "libgraph.h"

#include "eeregs.h"
#include "sce_gs.h"
#include "yamipan/ee/interrupts.h"
#include "yamipan/ee/kernel.h"

using yamipan::ee::InterruptController;
using yamipan::ee::Kernel;
using yamipan::ee::VideoStandard;

namespace
{

constexpr short kGsVersion = 3;
constexpr int kWindowCentre = 2048;
constexpr int kFractionBits = 4;
constexpr int kPageWidth = 64;
constexpr u_int kOpaqueQ = 0x3f800000; // 1.0f

sceGsGParam s_parameters;

InterruptController &interrupts()
{
    return Kernel::instance().interrupts();
}

} // namespace

void sceGsResetGraph(short, short inter, short omode, short ffmode)
{
    s_parameters.sceGsInterMode = inter;
    s_parameters.sceGsOutMode = omode;
    s_parameters.sceGsFFMode = ffmode;
    s_parameters.sceGsVersion = kGsVersion;
    interrupts().setVideoStandard(omode == SCE_GS_PAL ? VideoStandard::Pal : VideoStandard::Ntsc);
}

void sceGsResetPath() {}

sceGsGParam *sceGsGetGParam()
{
    return &s_parameters;
}

int sceGsSyncPath(int, u_short)
{
    return 0; // nothing is ever in flight
}

int sceGsSyncV(int)
{
    Kernel::instance().waitVBlank();
    return static_cast<int>(interrupts().fieldCount() & 1);
}

int VSync()
{
    Kernel::instance().waitVBlank();
    return 0;
}

sceGsVCallbackFunc sceGsSyncVCallback(sceGsVCallbackFunc func)
{
    return interrupts().setVBlankCallback(func);
}

void sceGsPutDispEnv(sceGsDispEnv *) {}

int sceGsPutDrawEnv(sceGifTag *)
{
    return 0;
}

int sceGsExecLoadImage(sceGsLoadImage *, u_long128 *)
{
    return 0;
}

void sceGsSetDefDispEnv(sceGsDispEnv *disp, short psm, short w, short, short dx, short dy)
{
    *disp = {};
    disp->dispfb.FBW = static_cast<u_long>(w / kPageWidth);
    disp->dispfb.PSM = static_cast<u_long>(psm);
    disp->dispfb.DBX = static_cast<u_long>(dx);
    disp->dispfb.DBY = static_cast<u_long>(dy);
}

int sceGsSetDefDrawEnv(sceGsDrawEnv1 *draw, short psm, short w, short h, short ztest, short zpsm)
{
    *draw = {};
    draw->frame1addr = SCE_GS_FRAME_1;
    draw->frame1.FBW = static_cast<u_long>(w / kPageWidth);
    draw->frame1.PSM = static_cast<u_long>(psm);

    draw->zbuf1addr = SCE_GS_ZBUF_1;
    draw->zbuf1.PSM = static_cast<u_long>(zpsm & 0xf);
    draw->zbuf1.ZMSK = ztest == SCE_GS_ZNOUSE ? 1 : 0;

    draw->xyoffset1addr = SCE_GS_XYOFFSET_1;
    draw->xyoffset1.OFX = static_cast<u_long>((kWindowCentre - w / 2) << kFractionBits);
    draw->xyoffset1.OFY = static_cast<u_long>((kWindowCentre - h / 2) << kFractionBits);

    draw->scissor1addr = SCE_GS_SCISSOR_1;
    draw->scissor1.SCAX1 = static_cast<u_long>(w - 1);
    draw->scissor1.SCAY1 = static_cast<u_long>(h - 1);

    draw->prmodecontaddr = SCE_GS_PRMODECONT;
    draw->prmodecont.AC = 1;
    draw->colclampaddr = SCE_GS_COLCLAMP;
    draw->colclamp.CLAMP = 1;
    draw->dtheaddr = SCE_GS_DTHE;
    draw->dthe.DTHE = (psm & 2) != 0 ? 1 : 0;

    draw->test1addr = SCE_GS_TEST_1;
    draw->test1.ZTE = 1;
    draw->test1.ZTST = ztest == SCE_GS_ZNOUSE ? SCE_GS_ZALWAYS : static_cast<u_long>(ztest);
    return 8;
}

int sceGsSetDefClear(sceGsClear *clear, short ztest, short x, short y, short w, short h, u_char r, u_char g, u_char b,
                     u_char a, u_int z)
{
    *clear = {};
    clear->test1addr = SCE_GS_TEST_1;
    clear->test1.ZTE = 1;
    clear->test1.ZTST = SCE_GS_ZALWAYS;

    clear->primaddr = SCE_GS_PRIM;
    clear->prim.PRIM = SCE_GS_PRIM_SPRITE;

    clear->rgbaqaddr = SCE_GS_RGBAQ;
    clear->rgbaq.R = r;
    clear->rgbaq.G = g;
    clear->rgbaq.B = b;
    clear->rgbaq.A = a;
    *reinterpret_cast<u_int *>(reinterpret_cast<u_char *>(&clear->rgbaq) + 4) = kOpaqueQ;

    clear->xyz2aaddr = SCE_GS_XYZ2;
    clear->xyz2a.X = static_cast<u_long>(x) << kFractionBits;
    clear->xyz2a.Y = static_cast<u_long>(y) << kFractionBits;
    clear->xyz2a.Z = z;

    clear->xyz2baddr = SCE_GS_XYZ2;
    clear->xyz2b.X = static_cast<u_long>(x + w) << kFractionBits;
    clear->xyz2b.Y = static_cast<u_long>(y + h) << kFractionBits;
    clear->xyz2b.Z = z;

    clear->testbaddr = SCE_GS_TEST_1;
    clear->testb.ZTE = 1;
    clear->testb.ZTST = ztest == SCE_GS_ZNOUSE ? SCE_GS_ZALWAYS : static_cast<u_long>(ztest);
    return 6;
}

int sceGsSetDefTexEnv(sceGsTexEnv *tex, short flush, short tbp0, short tbw, short psm, short w, short h, short tfx,
                      short cbp, short cpsm, short cld, short filter)
{
    *tex = {};
    tex->texflushaddr = SCE_GS_TEXFLUSH;
    (void)flush;

    tex->tex1addr = SCE_GS_TEX1_1;
    tex->tex1.MMAG = static_cast<u_long>(filter);
    tex->tex1.MMIN = static_cast<u_long>(filter);

    tex->tex0addr = SCE_GS_TEX0_1;
    tex->tex0.TBP0 = static_cast<u_long>(tbp0);
    tex->tex0.TBW = static_cast<u_long>(tbw);
    tex->tex0.PSM = static_cast<u_long>(psm);
    tex->tex0.TW = static_cast<u_long>(w);
    tex->tex0.TH = static_cast<u_long>(h);
    tex->tex0.TCC = 1;
    tex->tex0.TFX = static_cast<u_long>(tfx);
    tex->tex0.CBP = static_cast<u_long>(cbp);
    tex->tex0.CPSM = static_cast<u_long>(cpsm);
    tex->tex0.CLD = static_cast<u_long>(cld);

    tex->clampaddr = SCE_GS_CLAMP_1;
    return 4;
}
