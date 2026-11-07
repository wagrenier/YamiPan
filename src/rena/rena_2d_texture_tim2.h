/* ==========================================================================
 *  rena/rena_2d_texture_tim2.h
 *
 *  rena's TIM2 texture (rena_2d_texture_tim2.cpp, 0x2e1b70..0x2e2970): one
 *  Tim2Texture per mip level of a TIM2 file's first picture, the top level
 *  owning the others. It reads the file in place and works out the GS's view
 *  of it: the TEX0 template, the blocks it needs, the pixels and CLUT. The
 *  file stays the loader's.
 *
 *  PC: nothing reaches GS memory. The host draw module decodes the picture
 *  at its source (m_image) to a GPU texture (TextureStore); the block
 *  numbers still matter to the game's own VRAM bookkeeping (TexAreaList).
 * ======================================================================== */
#ifndef _RENA_RENA_2D_TEXTURE_TIM2_H
#define _RENA_RENA_2D_TEXTURE_TIM2_H

#include "rena/tim2.h"
#include "sdk/sce_gs.h"
#include "sdk/scetypes.h"

class GsRegion;                 // rena/rena_2d_texture_vram.h

// A setTex0 packet (putTex0, GsTexture::putTex0): DMA CNT of 2 qwords; VIF
// FLUSH, DIRECT 2; a GIF tag of one A+D (NLOOP 1, EOP, NREG 1).
constexpr u_long kTexPktDmaCnt2 = 0x10000002;
constexpr u_long kTexPktVifFlushDirect2 = 0x5100000211000000ull;
constexpr u_long kTexPktGifTagAd1 = 0x1000000000008001ull;
constexpr u_long kGifRegAd = 0xe;

// How a texture's CLUT goes to the GS.
enum Tim2ClutKind
{
    Tim2Clut_None   = 0,
    Tim2Clut_Index4 = 1,        // m_clutCount rows of 16 entries, 8 blocks apart
    Tim2Clut_Index8 = 2,        // one 256-entry block of 16 x 16
};

// EE 0x48, vptr 0x31a7c0 at +0x44 after the data. layout: host
class Tim2Texture
{
public:
    // Loads a TIM2 image ('T' first, else rena's "tim2" message).
    Tim2Texture(const void *image);                                      // FS2 0x002e2920
    virtual ~Tim2Texture();                                              // FS2 0x002e2810: deleting (obj, flag)

    // Writes a 3-qword packet at *cursor (CNT / FLUSH DIRECT / A+D) with
    // the TEX0 template at GS block `block` (its CLUT after it) to reg.
    // PC: the host binds textures by their owner (yamipan_drawui.h).
    void putTex0(u_long **cursor, int block, u_int reg);                 // FS2 0x002e1b70
    // The image's and CLUT's GS blocks, rounded up to whole pages.
    int getVramBlocks();                                                 // FS2 0x002e1c30
    // PC: nothing to send, no GS memory (the body says what the ROM sends).
    void upload(u_long **cursor, int block);                             // FS2 0x002e1c50
    // The TEX0 template at GS block `block`, its CLUT after the image:
    // putTex0's value without the packet (GsRegionAlloc::bind). Written in
    // unassigned@002e1b10, the word before this file's range.
    void getTex0(sceGsTex0 *out, int block);                             // FS2 0x002e1b10
    // The float at +8 of the TIM2 file (its header's reserved bytes): the
    // distance scale rena_draw's mip choice divides by.
    float getLodScale();                                                 // FS2 0x002e16c0
    // Mip level `level`, 0 past the last or without a list.
    Tim2Texture *getMip(int level);                                      // FS2 0x002e16d0

    sceGsTex0     m_tex0;       // FS2 +0x00: the TEX0 template (no TBP0 / CBP)
    GsRegion     *m_region;     // FS2 +0x08: the VRAM region held, 0 if none
    Tim2Texture **m_mips;       // FS2 +0x0c: every level, [0] the top one
    short         m_mipCount;   // FS2 +0x10
    short         m_lineQwords; // FS2 +0x12: qwords per line of pixels
    short         m_clutKind;   // FS2 +0x14: Tim2ClutKind
    short         m_psm;        // FS2 +0x18
    short         m_clutPsm;    // FS2 +0x1a
    short         m_width;      // FS2 +0x1c
    short         m_height;     // FS2 +0x1e
    short         m_imageBlocks;// FS2 +0x20: the image's GS blocks; the CLUT follows them
    int           m_imageQwords;// FS2 +0x24
    void         *m_pixels;     // FS2 +0x28
    short         m_clutCount;  // FS2 +0x2e: the CLUT rows of a 4-bit image
    short         m_clutBlocks; // FS2 +0x30
    int           m_clutQwords; // FS2 +0x34: of one row
    void         *m_clut;       // FS2 +0x38
    void         *m_altClut;    // FS2 +0x3c: a second CLUT on the global heap, sent 4 blocks after the first (modelTexturePremultiplyPalette)
    void         *m_image;      // FS2 +0x40: the TIM2 file (TexAreaList's key)

private:
    // An extra mip level: load makes them, the top level owns them.
    Tim2Texture() : m_region(nullptr), m_altClut(nullptr), m_image(nullptr) {}

    int  load(const void *image);                                        // FS2 0x002e26c0
    void setupMip(void *image, TIM2_PICTUREHEADER *ph, int mipmap);      // FS2 0x002e22d0
};

#endif /* _RENA_RENA_2D_TEXTURE_TIM2_H */
