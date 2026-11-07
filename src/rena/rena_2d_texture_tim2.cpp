// rena/rena_2d_texture_tim2.cpp -- rena's TIM2 texture; the file name is the
// game's own (its error message). rena_2d_texture_tim2.h has the class.

#include "rena/rena_2d_texture_tim2.h"

#include "game/gfxtarget.h"
#include "game/heap.h"
#include "rx/rxsys.h"
#include "sdk/libgraph.h"

// The GS's numbers per Tim2ImageType, tables 0x30fd90..0x30fdb8.
static const char kTim2GsPsm[8] = {
    SCE_GS_PSMCT32, SCE_GS_PSMCT16, SCE_GS_PSMCT24, SCE_GS_PSMCT32,
    SCE_GS_PSMT4, SCE_GS_PSMT8, SCE_GS_PSMCT32, SCE_GS_PSMCT32,
};
static const short kTim2PageWidth[8] = {0, 64, 64, 64, 128, 128, 0, 0};
static const short kTim2PageHeight[8] = {0, 64, 32, 32, 128, 64, 0, 0};
static const short kTim2BitsPerPixel[8] = {0, 16, 24, 32, 4, 8, 0, 0};

constexpr int kTbwUnit = 64;                    // TEX0.TBW counts 64 pixels
constexpr int kBitsPerQword = 128;
constexpr u_int kClutTypeKind = 0x7;            // bit 7 of the file's ClutType: linear order

// Smallest n with (1 << n) >= size: TEX0.TW / TH. The ROM counts the leading
// bits of size * 2 - 1 (plzcw).
static int gsSizeLog2(int size)
{
    int log2 = 0;
    for (int value = size * 2 - 1; value > 1; value >>= 1)
        log2++;
    return log2;
}

// ──────────────────────────────────────────────────────────────────────
// putTex0's TEX0 value without the packet: the template with TBP0 at GS
// block `block` and CBP right after the image (GsRegionAlloc::bind,
// renaSetShadowTexture, renaSetSpotlight). The ROM writes the template
// to *out, then patches the low halfword's 14 TBP0 bits and the 14 CBP
// bits at 37 in place.
// FS2 0x002e1b10

void Tim2Texture::getTex0(sceGsTex0 *out, int block)
{
    *out = m_tex0;
    out->TBP0 = block;
    out->CBP = block + m_imageBlocks;
}

// ──────────────────────────────────────────────────────────────────────
// The TEX0 template at GS block `block`, its CLUT right after the image.
// FS2 0x002e1b70

void Tim2Texture::putTex0(u_long **cursor, int block, u_int reg)
{
    sceGsTex0 tex0 = m_tex0;
    tex0.TBP0 = block;
    tex0.CBP = block + m_imageBlocks;

    u_long *p = *cursor;
    p[0] = kTexPktDmaCnt2;
    p[1] = kTexPktVifFlushDirect2;
    p[2] = kTexPktGifTagAd1;
    p[3] = kGifRegAd;
    p[4] = *(u_long *)&tex0;
    p[5] = reg;
    *cursor = p + 6;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002e1c30

int Tim2Texture::getVramBlocks()
{
    return (m_imageBlocks + m_clutBlocks + kGsBlocksPerPage - 1) & ~(kGsBlocksPerPage - 1);
}

// ──────────────────────────────────────────────────────────────────────
// The ROM writes one image transfer per picture into the packet at *cursor:
// the pixels to `block` (split into runs under 0x8000 qwords, on whole
// lines); a 4-bit image's m_clutCount CLUT rows of 8 x 2 to block +
// m_imageBlocks, 8 blocks apart, or an 8-bit image's 16 x 16 there; the
// m_altClut, if any, 4 blocks after; then TEXFLUSH.
// PC: the host decodes the picture at its source, so nothing is sent. The
// callers already written leave the call out (game/uidrawer.cpp).
// FS2 0x002e1c50

void Tim2Texture::upload(u_long **cursor, int block)
{
}

// ──────────────────────────────────────────────────────────────────────
// One mip level of the picture: the GS's view of its pixels and CLUT.
// FS2 0x002e22d0
// callcheck: memset (the field resets at its head; the host's class has pointers)

void Tim2Texture::setupMip(void *image, TIM2_PICTUREHEADER *ph, int mipmap)
{
    m_image = image;
    void *clut = Tim2GetClut(ph);
    void *pixels = Tim2GetImage(ph, mipmap);

    // memset(this + 0x18, 0, 0x24), then the template and the CLUT kind.
    m_psm = 0;
    m_clutPsm = 0;
    m_width = 0;
    m_height = 0;
    m_imageBlocks = 0;
    m_imageQwords = 0;
    m_pixels = nullptr;
    m_clutCount = 0;
    m_clutBlocks = 0;
    m_clutQwords = 0;
    m_clut = nullptr;
    m_tex0 = sceGsTex0();
    m_clutKind = Tim2Clut_None;

    if (pixels != nullptr) {
        int width, height;
        int size = Tim2GetMipMapPictureSize(ph, mipmap, &width, &height);
        int type = ph->imageType;
        int pageWidth = kTim2PageWidth[type];
        int pageHeight = kTim2PageHeight[type];
        int alignedWidth = ~(pageWidth - 1) & (width + pageWidth - 1);
        int alignedHeight = ~(pageHeight - 1) & (height + pageHeight - 1);

        m_tex0.PSM = kTim2GsPsm[type];
        m_tex0.TBW = alignedWidth / kTbwUnit;
        m_tex0.TW = gsSizeLog2(width);
        m_tex0.TH = gsSizeLog2(height);
        m_tex0.TCC = 1;
        m_lineQwords = width * kTim2BitsPerPixel[type] / kBitsPerQword;
        m_psm = kTim2GsPsm[type];
        m_width = width;
        m_height = height;
        m_imageBlocks = (alignedWidth / pageWidth) * (alignedHeight / pageHeight) * kGsBlocksPerPage;
        m_imageQwords = size / 16;
        m_pixels = pixels;
    }

    if (clut != nullptr) {
        // The ROM indexes its tables with the whole byte; a linear-order CLUT
        // (bit 7) would read past them, so the host keeps the type bits.
        int clutType = ph->clutType & kClutTypeKind;
        m_tex0.CPSM = kTim2GsPsm[clutType];
        m_tex0.CSA = 0;
        m_tex0.CSM = 0;
        m_tex0.CLD = 1;
        m_clutCount = 0;

        if (ph->imageType == Tim2Image_Index4) {
            m_clutQwords = kTim2BitsPerPixel[clutType] * 16 / 8 / 16;
            m_clutCount = (int)(ph->clutSize >> 4) / m_clutQwords;
            m_clutKind = Tim2Clut_Index4;
            m_clutBlocks = 1;
        }
        else {
            m_clutQwords = kTim2BitsPerPixel[clutType] * 256 / 8 / 16;
            m_clutKind = Tim2Clut_Index8;
            m_clutBlocks = 4;
        }
        m_clutPsm = kTim2GsPsm[clutType];
        m_clut = clut;
    }
}

// ──────────────────────────────────────────────────────────────────────
// One Tim2Texture per mip level of the first picture: this one for the top
// level, a new one (holding only itself in m_mips) for each of the others.
// FS2 0x002e26c0
// callcheck: operator_new, operator_new[] (new / new[])

int Tim2Texture::load(const void *image)
{
    if (*(const char *)image != 'T') {
        rxSysPrintf("rena_2d_texture_tim2.cpp: tim2 file id error.\n");
        return 0;
    }
    TIM2_PICTUREHEADER *ph = Tim2GetPictureHeader((void *)image, 0);
    m_mipCount = ph->mipMapTextures;
    m_mips = new Tim2Texture *[m_mipCount];
    for (int i = 0; i < ph->mipMapTextures; i++) {
        Tim2Texture *mip;
        if (i == 0) {
            mip = this;
        }
        else {
            mip = new Tim2Texture();
            mip->m_mipCount = 1;
            mip->m_mips = new Tim2Texture *[1];
            mip->m_mips[0] = mip;
        }
        m_mips[i] = mip;
        m_mips[i]->setupMip((void *)image, ph, i);
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Gives the region back, deletes the other levels and the second CLUT. The
// file itself stays the caller's.
// FS2 0x002e2810
// callcheck: operator_delete, operator_delete[] (delete[]; the deleting variant, compiler generated)

Tim2Texture::~Tim2Texture()
{
    if (m_region != nullptr) {
        m_region->release();
        m_region = nullptr;
    }
    if (m_image != nullptr) {
        for (int i = 1; i < m_mipCount; i++) {
            if (m_mips[i] != nullptr)
                delete m_mips[i];
        }
        delete[] m_mips;
        m_image = nullptr;
        if (m_altClut != nullptr)
            gGlobalHeap.free(m_altClut);
        m_altClut = nullptr;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002e2920

Tim2Texture::Tim2Texture(const void *image)
    : m_region(nullptr), m_mips(nullptr), m_mipCount(0), m_altClut(nullptr), m_image(nullptr)
{
    load(image);
}
