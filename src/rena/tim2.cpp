// rena/tim2.cpp -- Sony's sample TIM2 library as the game links it, all
// twenty functions of 0x2e97a8..0x2eaeb0 (the sample's Tim2IsClut2 and
// Tim2GetMipMapHeader are not in the ROM; its loaders are an older cut that
// takes the GS addresses from the picture's own registers).

#include "rena/tim2.h"

#include <string.h>

#include "rx/rxsys.h"
#include "sdk/eekernel.h"
#include "sdk/fileio.h"
#include "sdk/libgraph.h"

// The GS's TEX0 fields the loaders read and the snapshot writes.
#define TIM2_TEX0_TBP(t)     ((u_int)(t) & 0x3fff)
#define TIM2_TEX0_TBW(t)     ((u_int)((t) >> 14) & 0x3f)
#define TIM2_TEX0_PSM(t)     ((u_int)((t) >> 20) & 0x3f)
#define TIM2_TEX0_CBP(t)     ((u_int)((t) >> 37) & 0x3fff)
#define TIM2_TEX0_CPSM(t)    ((u_int)((t) >> 51) & 0xf)
#define TIM2_TEX0_CSM_CSA    (0x3fULL << 55)    // CSM and CSA, cleared by Tim2LoadClut

// One mip level's base in GsMiptbp1 / 2: three levels of 20 bits each.
#define TIM2_MIPTBP_BITS     20

// A GS register kept as two words in the header.
static u_long tim2Reg(const u_int *reg)
{
    return reg[0] | ((u_long)reg[1] << 32);
}

static void tim2SetReg(u_int *reg, u_long value)
{
    reg[0] = (u_int)value;
    reg[1] = (u_int)(value >> 32);
}

// The CSM1 swap: 256-colour CLUTs without TIM2_CLUT_LINEAR, and 16-colour
// ones with TIM2_CLUT_BLOCKED, are kept in the GS's block order, entries
// 8..15 and 16..23 of every 32 trading places. Inline in both of the ROM's
// CLUT accessors.
static int tim2ClutSwap(const TIM2_PICTUREHEADER *ph, int n)
{
    switch ((ph->clutType << 8) | ph->imageType) {
    case ((TIM2_CLUT_BLOCKED | Tim2Image_Rgb16) << 8) | Tim2Image_Index4:
    case ((TIM2_CLUT_BLOCKED | Tim2Image_Rgb24) << 8) | Tim2Image_Index4:
    case ((TIM2_CLUT_BLOCKED | Tim2Image_Rgb32) << 8) | Tim2Image_Index4:
    case (Tim2Image_Rgb16 << 8) | Tim2Image_Index8:
    case (Tim2Image_Rgb24 << 8) | Tim2Image_Index8:
    case (Tim2Image_Rgb32 << 8) | Tim2Image_Index8:
        if ((n & 0x1f) >= 8) {
            if ((n & 0x1f) < 0x10)
                n += 8;
            else if ((n & 0x1f) < 0x18)
                n -= 8;
        }
        break;
    }
    return n;
}

static void Tim2LoadTexture(int psm, u_int tbp, int tbw, int w, int h, void *image);
static int  Tim2CalcBufWidth(int psm, int w);
static int  Tim2GetLog2(int n);

// The header bytes before a picture's user space, by its mip level count:
// the picture header, then the mip header (0x20 for 2..4 levels, 0x30 for
// 5..7). FS2 0x00310360, "00PPP```".
static const char s_tim2HeaderSizes[8] = {
    0x30, 0x30, 0x50, 0x50, 0x50, 0x60, 0x60, 0x60,
};

// ──────────────────────────────────────────────────────────────────────
// "TIM2" or "CLT2", version 3, or 4 with either alignment.
// FS2 0x002e97a8

int Tim2CheckFileHeaer(void *tim2)
{
    TIM2_FILEHEADER *file = (TIM2_FILEHEADER *)tim2;
    int kind;
    if (file->fileId[0] == 'T' && file->fileId[1] == 'I'
        && file->fileId[2] == 'M' && file->fileId[3] == '2') {
        kind = Tim2File_Tim2;
    } else if (file->fileId[0] == 'C' && file->fileId[1] == 'L'
               && file->fileId[2] == 'T' && file->fileId[3] == '2') {
        kind = Tim2File_Clt2;
    } else {
        const u_char *id = (const u_char *)file->fileId;
        rxSysPrintf("Tim2CheckFileHeader: TIM2 is broken %02X,%02X,%02X,%02X\n",
                    id[0], id[1], id[2], id[3]);
        return Tim2File_Broken;
    }
    if (!(file->formatVersion == 3
          || (file->formatVersion == 4 && file->formatId <= Tim2Format_Align128))) {
        rxSysPrintf("Tim2CheckFileHeader: TIM2 is broken\n");
        return Tim2File_Broken;
    }
    return kind;
}

// ──────────────────────────────────────────────────────────────────────
// The header of picture `imgno`; each picture's totalSize leads to the next.
// FS2 0x002e9868

TIM2_PICTUREHEADER *Tim2GetPictureHeader(void *tim2, int imgno)
{
    TIM2_FILEHEADER *file = (TIM2_FILEHEADER *)tim2;
    if (imgno >= file->pictures) {
        rxSysPrintf("Tim2GetPictureHeader: Illegal image no.(%d)\n", imgno);
        return nullptr;
    }
    TIM2_PICTUREHEADER *ph = file->formatId == Tim2Format_Align16
        ? (TIM2_PICTUREHEADER *)((char *)tim2 + 0x10)
        : (TIM2_PICTUREHEADER *)((char *)tim2 + 0x80);
    for (int i = 0; i < imgno; i++)
        ph = (TIM2_PICTUREHEADER *)((char *)ph + ph->totalSize);
    return ph;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002e98f0

int Tim2GetMipMapPictureSize(TIM2_PICTUREHEADER *ph, int mipmap, int *width, int *height)
{
    int w = ph->imageWidth >> mipmap;
    int h = ph->imageHeight >> mipmap;
    if (width != nullptr)
        *width = w;
    if (height != nullptr)
        *height = h;
    int size = w * h;
    switch (ph->imageType) {
    case Tim2Image_Rgb16:  size *= 2; break;
    case Tim2Image_Rgb24:  size *= 3; break;
    case Tim2Image_Rgb32:  size *= 4; break;
    case Tim2Image_Index4: size /= 2; break;
    }
    return (size + 0xf) & ~0xf;
}

// ──────────────────────────────────────────────────────────────────────
// What lies between the picture's headers and its pixels. Its size is the
// eXt header's own count when there is one, else all of it.
// FS2 0x002e99a8

void *Tim2GetUserSpace(TIM2_PICTUREHEADER *ph, int *size)
{
    int headers = s_tim2HeaderSizes[ph->mipMapTextures];
    if (ph->headerSize == headers) {
        if (size != nullptr)
            *size = 0;
        return nullptr;
    }
    TIM2_EXHEADER *ex = (TIM2_EXHEADER *)((char *)ph + headers);
    if (size != nullptr) {
        if (ex->exHeaderId[0] == 'e' && ex->exHeaderId[1] == 'X'
            && ex->exHeaderId[2] == 't' && ex->exHeaderId[3] == '\0')
            *size = ex->userSpaceSize;
        else
            *size = ph->headerSize - headers;
    }
    return ex;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002e9a28

void *Tim2GetUserData(TIM2_PICTUREHEADER *ph, int *size)
{
    TIM2_EXHEADER *ex = (TIM2_EXHEADER *)Tim2GetUserSpace(ph, size);
    if (ex == nullptr)
        return nullptr;
    if (!(ex->exHeaderId[0] == 'e' && ex->exHeaderId[1] == 'X'
          && ex->exHeaderId[2] == 't' && ex->exHeaderId[3] == '\0'))
        return ex;
    if (size != nullptr)
        *size = ex->userDataSize;
    return ex + 1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002e9a90

char *Tim2GetComment(TIM2_PICTUREHEADER *ph)
{
    TIM2_EXHEADER *ex = (TIM2_EXHEADER *)Tim2GetUserSpace(ph, nullptr);
    if (ex == nullptr)
        return nullptr;
    if (!(ex->exHeaderId[0] == 'e' && ex->exHeaderId[1] == 'X'
          && ex->exHeaderId[2] == 't' && ex->exHeaderId[3] == '\0'))
        return nullptr;
    if (ex->userSpaceSize == sizeof(TIM2_EXHEADER) + ex->userDataSize)
        return nullptr;
    return (char *)(ex + 1) + ex->userDataSize;
}

// ──────────────────────────────────────────────────────────────────────
// The pixels of a mip level: after the header and the lower levels. Null
// past the last level.
// FS2 0x002e9af0

void *Tim2GetImage(TIM2_PICTUREHEADER *ph, int mipmap)
{
    if (mipmap >= ph->mipMapTextures)
        return nullptr;
    char *image = (char *)ph + ph->headerSize;
    if (ph->mipMapTextures == 1)
        return image;
    TIM2_MIPMAPHEADER *mip = (TIM2_MIPMAPHEADER *)(ph + 1);
    for (int i = 0; i < mipmap; i++)
        image += mip->mmImageSize[i];
    return image;
}

// ──────────────────────────────────────────────────────────────────────
// The CLUT, after every level's pixels; null when the picture has none.
// FS2 0x002e9b60

void *Tim2GetClut(TIM2_PICTUREHEADER *ph)
{
    if (ph->clutColors == 0)
        return nullptr;
    return (char *)ph + ph->headerSize + ph->imageSize;
}

// ──────────────────────────────────────────────────────────────────────
// A CLUT entry as 0xAABBGGRR, found through the CSM1 swap (tim2ClutSwap).
// FS2 0x002e9b88

u_int Tim2GetClutColor(TIM2_PICTUREHEADER *ph, int clut, int no)
{
    u_char *pal = (u_char *)Tim2GetClut(ph);
    if (pal == nullptr)
        return 0;

    int n;
    switch (ph->imageType) {
    case Tim2Image_Index4: n = clut * 16 + no;  break;
    case Tim2Image_Index8: n = clut * 256 + no; break;
    default:               return 0;
    }
    if (n > ph->clutColors)
        return 0;
    n = tim2ClutSwap(ph, n);

    u_int r, g, b, a;
    switch (ph->clutType & TIM2_CLUT_TYPE_MASK) {
    case Tim2Image_Rgb16: {
        u_int c = pal[n * 2] | (pal[n * 2 + 1] << 8);
        r = (c << 3) & 0xf8;
        g = (c >> 2) & 0xf8;
        b = (c >> 7) & 0xf8;
        a = (c >> 8) & 0x80;
        break;
    }
    case Tim2Image_Rgb24:
        r = pal[n * 3];
        g = pal[n * 3 + 1];
        b = pal[n * 3 + 2];
        a = 0x80;
        break;
    case Tim2Image_Rgb32:
        r = pal[n * 4];
        g = pal[n * 4 + 1];
        b = pal[n * 4 + 2];
        a = pal[n * 4 + 3];
        break;
    default:
        r = g = b = a = 0;
        break;
    }
    return (a << 24) | (b << 16) | (g << 8) | r;
}

// ──────────────────────────────────────────────────────────────────────
// Tim2GetClutColor's twin: the same entry, stored from 0xAABBGGRR (RGB16
// keeps the top five bits of each channel and alpha's bit 7 as its own top
// bit). Answers the old colour.
// FS2 0x002e9d90

u_int Tim2SetClutColor(TIM2_PICTUREHEADER *ph, int clut, int no, u_int newcolor)
{
    u_char *pal = (u_char *)Tim2GetClut(ph);
    if (pal == nullptr)
        return 0;

    int n;
    switch (ph->imageType) {
    case Tim2Image_Index4: n = clut * 16 + no;  break;
    case Tim2Image_Index8: n = clut * 256 + no; break;
    default:               return 0;
    }
    if (n > ph->clutColors)
        return 0;
    n = tim2ClutSwap(ph, n);

    u_int r, g, b, a;
    switch (ph->clutType & TIM2_CLUT_TYPE_MASK) {
    case Tim2Image_Rgb16: {
        u_char *p = pal + n * 2;
        u_int c = p[0] | (p[1] << 8);
        r = (c << 3) & 0xf8;
        g = (c >> 2) & 0xf8;
        b = (c >> 7) & 0xf8;
        a = (c >> 8) & 0x80;
        u_int v = ((newcolor >> 3) & 0x1f)          // r
                | ((newcolor >> 6) & 0x3e0)         // g
                | ((newcolor >> 9) & 0x7c00)        // b
                | ((newcolor >> 16) & 0x8000);      // a's bit 7
        p[0] = (u_char)v;
        p[1] = (u_char)(v >> 8);
        break;
    }
    case Tim2Image_Rgb24: {
        u_char *p = pal + n * 3;
        r = p[0];
        g = p[1];
        b = p[2];
        a = 0x80;
        p[0] = (u_char)newcolor;
        p[1] = (u_char)(newcolor >> 8);
        p[2] = (u_char)(newcolor >> 16);
        break;
    }
    case Tim2Image_Rgb32: {
        u_char *p = pal + n * 4;
        r = p[0];
        g = p[1];
        b = p[2];
        a = p[3];
        p[0] = (u_char)newcolor;
        p[1] = (u_char)(newcolor >> 8);
        p[2] = (u_char)(newcolor >> 16);
        p[3] = (u_char)(newcolor >> 24);
        break;
    }
    default:
        r = g = b = a = 0;
        break;
    }
    return (a << 24) | (b << 16) | (g << 8) | r;
}

// ──────────────────────────────────────────────────────────────────────
// The bound is the sample's: x == width and y == height still read (one
// past the row, or the next level's first bytes).
// FS2 0x002ea010

u_int Tim2GetTexel(TIM2_PICTUREHEADER *ph, int mipmap, int x, int y)
{
    u_char *image = (u_char *)Tim2GetImage(ph, mipmap);
    if (image == nullptr)
        return 0;
    int w, h;
    Tim2GetMipMapPictureSize(ph, mipmap, &w, &h);
    if (x > w || y > h)
        return 0;

    int t = y * w + x;
    switch (ph->imageType) {
    case Tim2Image_Rgb16: {
        u_char *p = image + t * 2;
        return p[0] | (p[1] << 8);
    }
    case Tim2Image_Rgb24: {
        u_char *p = image + t * 3;
        return (p[2] << 16) | (p[1] << 8) | p[0];
    }
    case Tim2Image_Rgb32: {
        u_char *p = image + t * 4;
        return ((u_int)p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0];
    }
    case Tim2Image_Index4:
        if (x & 1)
            return image[t / 2] >> 4;
        return image[t / 2] & 0xf;
    case Tim2Image_Index8:
        return image[t];
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Tim2GetTexel's twin. The 4-bit case is the sample's slip, kept: an even x
// stores (old << 4) | new, an odd x (new << 4) | old, so the old nibble
// moves and the new one is not masked.
// FS2 0x002ea188

u_int Tim2SetTexel(TIM2_PICTUREHEADER *ph, int mipmap, int x, int y, u_int newtexel)
{
    u_char *image = (u_char *)Tim2GetImage(ph, mipmap);
    if (image == nullptr)
        return 0;
    int w, h;
    Tim2GetMipMapPictureSize(ph, mipmap, &w, &h);
    if (x > w || y > h)
        return 0;

    int t = y * w + x;
    u_int old;
    switch (ph->imageType) {
    case Tim2Image_Rgb16: {
        u_char *p = image + t * 2;
        old = p[0] | (p[1] << 8);
        p[0] = (u_char)newtexel;
        p[1] = (u_char)(newtexel >> 8);
        break;
    }
    case Tim2Image_Rgb24: {
        u_char *p = image + t * 3;
        old = (p[2] << 16) | (p[1] << 8) | p[0];
        p[0] = (u_char)newtexel;
        p[1] = (u_char)(newtexel >> 8);
        p[2] = (u_char)(newtexel >> 16);
        break;
    }
    case Tim2Image_Rgb32: {
        u_char *p = image + t * 4;
        old = ((u_int)p[3] << 24) | (p[2] << 16) | (p[1] << 8) | p[0];
        p[0] = (u_char)newtexel;
        p[1] = (u_char)(newtexel >> 8);
        p[2] = (u_char)(newtexel >> 16);
        p[3] = (u_char)(newtexel >> 24);
        break;
    }
    case Tim2Image_Index4: {
        u_char *p = image + t / 2;
        if (x & 1) {
            old = *p >> 4;
            *p = (u_char)((newtexel << 4) | old);
        } else {
            old = *p & 0xf;
            *p = (u_char)((old << 4) | newtexel);
        }
        break;
    }
    case Tim2Image_Index8:
        old = image[t];
        image[t] = (u_char)newtexel;
        break;
    default:
        return 0;
    }
    return old;
}

// ──────────────────────────────────────────────────────────────────────
// (u, v) are level 0's coordinates. RGB16 widens to 8 bits a channel
// (alpha 0x80 or 0), RGB24 gets alpha 0x80, indices go through the CLUT.
// FS2 0x002ea380

u_int Tim2GetTextureColor(TIM2_PICTUREHEADER *ph, int mipmap, int clut, int u, int v)
{
    if (Tim2GetImage(ph, mipmap) == nullptr)
        return 0;
    u_int t = Tim2GetTexel(ph, mipmap, u >> mipmap, v >> mipmap);
    switch (ph->imageType) {
    case Tim2Image_Rgb16:
        return (((t >> 15) & 1) << 31)
             | (((t >> 10) & 0x1f) << 19)
             | (((t >> 5) & 0x1f) << 11)
             | ((t & 0x1f) << 3);
    case Tim2Image_Rgb24:
        return (t & 0xffffff) | 0x80000000;
    case Tim2Image_Rgb32:
        return t;
    case Tim2Image_Index4:
    case Tim2Image_Index8:
        return Tim2GetClutColor(ph, clut, t);
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002ea490

void Tim2LoadPicture(TIM2_PICTUREHEADER *ph)
{
    Tim2LoadImage(ph);
    Tim2LoadClut(ph);
}

// ──────────────────────────────────────────────────────────────────────
// Every mip level to its own base: level 0 at GsTex0's TBP0 / TBW, levels
// 1..3 from GsMiptbp1, 4..6 from GsMiptbp2.
// FS2 0x002ea4b8

void Tim2LoadImage(TIM2_PICTUREHEADER *ph)
{
    u_long tex0 = tim2Reg(ph->gsTex0);
    TIM2_MIPMAPHEADER *mip = nullptr;
    if (ph->mipMapTextures > 1)
        mip = (TIM2_MIPMAPHEADER *)(ph + 1);

    for (int i = 0; i < ph->mipMapTextures; i++) {
        u_int tbp, tbw;
        if (i == 0) {
            tbp = TIM2_TEX0_TBP(tex0);
            tbw = TIM2_TEX0_TBW(tex0);
        } else {
            u_long miptbp;
            int k;
            if (i < 4) {
                miptbp = tim2Reg(mip->gsMiptbp1);
                k = i - 1;
            } else {
                miptbp = tim2Reg(mip->gsMiptbp2);
                k = i - 4;
            }
            tbp = (u_int)(miptbp >> (k * TIM2_MIPTBP_BITS)) & 0x3fff;
            tbw = (u_int)(miptbp >> (k * TIM2_MIPTBP_BITS + 14)) & 0x3f;
        }
        void *image = Tim2GetImage(ph, i);
        int w, h;
        Tim2GetMipMapPictureSize(ph, i, &w, &h);
        Tim2LoadTexture(TIM2_TEX0_PSM(tex0), tbp, tbw, w, h, image);
    }
}

// ──────────────────────────────────────────────────────────────────────
// The CLUT to GsTex0's CBP in CSM1, so CSM and CSA are cleared in the
// header. CLUTs already in the GS's block order go as one 16-wide image;
// linear ones are reordered by sending each 16 colours as an 8 x 2 block.
// FS2 0x002ea610 // callcheck: sceGsSetDefLoadImage, FlushCache, sceGsExecLoadImage, sceGsSyncPath (PC: no GS upload)

void Tim2LoadClut(TIM2_PICTUREHEADER *ph)
{
    if (ph->clutType == 0)
        return;
    u_long tex0 = tim2Reg(ph->gsTex0) & ~TIM2_TEX0_CSM_CSA;
    tim2SetReg(ph->gsTex0, tex0);
    u_int cbp = TIM2_TEX0_CBP(tex0);
    u_int cpsm = TIM2_TEX0_CPSM(tex0);
    u_char *clut = (u_char *)ph + ph->headerSize + ph->imageSize;

    switch ((ph->clutType << 8) | ph->imageType) {
    case ((TIM2_CLUT_BLOCKED | Tim2Image_Rgb16) << 8) | Tim2Image_Index4:
    case ((TIM2_CLUT_BLOCKED | Tim2Image_Rgb24) << 8) | Tim2Image_Index4:
    case ((TIM2_CLUT_BLOCKED | Tim2Image_Rgb32) << 8) | Tim2Image_Index4:
    case (Tim2Image_Rgb16 << 8) | Tim2Image_Index8:
    case (Tim2Image_Rgb24 << 8) | Tim2Image_Index8:
    case (Tim2Image_Rgb32 << 8) | Tim2Image_Index8:
        Tim2LoadTexture(cpsm, cbp, 1, 16, ph->clutColors >> 4, clut);
        break;

    case (Tim2Image_Rgb16 << 8) | Tim2Image_Index4:
    case (Tim2Image_Rgb24 << 8) | Tim2Image_Index4:
    case (Tim2Image_Rgb32 << 8) | Tim2Image_Index4:
    case ((TIM2_CLUT_LINEAR | Tim2Image_Rgb16) << 8) | Tim2Image_Index4:
    case ((TIM2_CLUT_LINEAR | Tim2Image_Rgb24) << 8) | Tim2Image_Index4:
    case ((TIM2_CLUT_LINEAR | Tim2Image_Rgb32) << 8) | Tim2Image_Index4:
    case ((TIM2_CLUT_LINEAR | Tim2Image_Rgb16) << 8) | Tim2Image_Index8:
    case ((TIM2_CLUT_LINEAR | Tim2Image_Rgb24) << 8) | Tim2Image_Index8:
    case ((TIM2_CLUT_LINEAR | Tim2Image_Rgb32) << 8) | Tim2Image_Index8:
        for (int i = 0; i < (ph->clutColors >> 4); i++) {
            // The ROM sends these 16 colours as 8 x 2 to (cbp, width 1,
            // cpsm) at ((i & 1) * 8, (i >> 1) * 2). PC: no GS upload.
            switch (ph->clutType & TIM2_CLUT_TYPE_MASK) {
            case Tim2Image_Rgb16: clut += 2 * 16; break;
            case Tim2Image_Rgb24: clut += 3 * 16; break;
            default:              clut += 4 * 16; break;
            }
        }
        (void)cbp;
        (void)cpsm;
        break;

    default:
        rxSysPrintf("Illegal clut and texture combination. ($%02X,$%02X)\n",
                    ph->clutType, ph->imageType);
        break;
    }
}

// ──────────────────────────────────────────────────────────────────────
// A TIM2 v4 file of one picture, the display's size (DW / (MAGH + 1) by
// DH + 1) in its frame's format. With a second field's env, even rows come
// from d0 and odd ones from d1, each at DBY + row / 2.
// FS2 0x002ea8d8 // callcheck: sceGsSetDefStoreImage, FlushCache, sceGsExecStoreImage, sceGsSyncPath (PC: no GS frame to read back)

int Tim2TakeSnapshot(sceGsDispEnv *d0, sceGsDispEnv *d1, char *fname)
{
    int width = d0->display.DW / (d0->display.MAGH + 1);
    int height = d0->display.DH + 1;
    int psm = d0->dispfb.PSM;

    int bytes;
    int imageType;
    switch (psm) {
    case SCE_GS_PSMCT32:
        bytes = width * 4;
        imageType = Tim2Image_Rgb32;
        break;
    case SCE_GS_PSMCT24:
        bytes = width * 3;
        imageType = Tim2Image_Rgb24;
        break;
    case SCE_GS_PSMCT16:
    case SCE_GS_PSMCT16S:
        bytes = width * 2;
        imageType = Tim2Image_Rgb16;
        break;
    default:
        rxSysPrintf("Illegal pixel format.\n");
        return 0;
    }

    int fd = sceOpen(fname, SCE_CREAT | SCE_WRONLY);
    if (fd == -1) {
        rxSysPrintf("file create failure.\n");
        return 0;
    }

    TIM2_FILEHEADER file;
    file.fileId[0] = 'T';
    file.fileId[1] = 'I';
    file.fileId[2] = 'M';
    file.fileId[3] = '2';
    file.formatVersion = 4;
    file.formatId = Tim2Format_Align16;
    file.pictures = 1;
    for (int i = 0; i < 8; i++)
        file.reserved[i] = 0;
    sceWrite(fd, &file, sizeof(file));

    TIM2_PICTUREHEADER pict;
    pict.imageSize = bytes * height;
    pict.totalSize = pict.imageSize + sizeof(TIM2_PICTUREHEADER);
    pict.clutSize = 0;
    pict.headerSize = sizeof(TIM2_PICTUREHEADER);
    pict.clutColors = 0;
    pict.pictFormat = 0;
    pict.mipMapTextures = 1;
    pict.clutType = 0;
    pict.imageType = (u_char)imageType;
    pict.imageWidth = (u_short)width;
    pict.imageHeight = (u_short)height;
    tim2SetReg(pict.gsTex0,
               SCE_GS_SET_TEX0(0, Tim2CalcBufWidth(psm, width) & 0x3f, psm,
                               Tim2GetLog2(width) & 0xf, Tim2GetLog2(height) & 0xf,
                               0, 0, 0, 0, 0, 0, 0));
    pict.gsTex1[0] = 0;
    pict.gsTex1[1] = 0;
    pict.gsTexaFbaPabe = 0;
    pict.gsTexClut = 0;
    sceWrite(fd, &pict, sizeof(pict));

    // PC: the ROM reads each row back from the frame buffer (FBP * 32, FBW,
    // at (DBX, DBY + row) of d0, or with d1 at DBY + row / 2 of d0 on even
    // rows and of d1 on odd ones). There is no GS here: the rows go out black.
    u_long128 pixels[4096 / 16];
    memset(pixels, 0, sizeof(pixels));
    for (int i = 0; i < height; i++)
        sceWrite(fd, pixels, bytes);
    (void)d1;

    sceClose(fd);
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// A picture to GS memory at (tbp, tbw), in slices of at most 0x7ffc0
// bytes (a DMA transfer's limit). Unknown formats send nothing.
// FS2 0x002eac98 // callcheck: sceGsSetDefLoadImage, FlushCache, sceGsExecLoadImage, sceGsSyncPath (PC: no GS upload)

static void Tim2LoadTexture(int psm, u_int tbp, int tbw, int w, int h, void *image)
{
    int n;
    switch (psm) {
    case SCE_GS_PSMZ32:
    case SCE_GS_PSMCT32:
        n = w * 4;
        break;
    case SCE_GS_PSMZ24:
    case SCE_GS_PSMCT24:
        n = w * 3;
        break;
    case SCE_GS_PSMZ16:
    case SCE_GS_PSMZ16S:
    case SCE_GS_PSMCT16:
    case SCE_GS_PSMCT16S:
        n = w * 2;
        break;
    case SCE_GS_PSMT8H:
    case SCE_GS_PSMT8:
        n = w;
        break;
    case SCE_GS_PSMT4HL:
    case SCE_GS_PSMT4HH:
    case SCE_GS_PSMT4:
        n = w / 2;
        break;
    default:
        return;
    }

    int rows = 0x7ffc0 / n;
    for (int i = 0; i < h; i += rows) {
        u_char *p = (u_char *)image + n * i;
        if (i + rows > h)
            rows = h - i;
        // The ROM sends `rows` rows from p to (tbp, tbw, psm) at (0, i),
        // w wide. PC: no GS upload.
        (void)p;
    }
    (void)tbp;
    (void)tbw;
}

// ──────────────────────────────────────────────────────────────────────
// The buffer width in 64-pixel pages; 8- and 4-bit textures use an even
// number. 0 for a format the GS cannot texture from.
// FS2 0x002eae00

static int Tim2CalcBufWidth(int psm, int w)
{
    switch (psm) {
    case SCE_GS_PSMT8H:
    case SCE_GS_PSMT4HL:
    case SCE_GS_PSMT4HH:
    case SCE_GS_PSMCT32:
    case SCE_GS_PSMCT24:
    case SCE_GS_PSMZ32:
    case SCE_GS_PSMZ24:
    case SCE_GS_PSMCT16:
    case SCE_GS_PSMCT16S:
    case SCE_GS_PSMZ16:
    case SCE_GS_PSMZ16S:
        return (w + 63) / 64;
    case SCE_GS_PSMT8:
    case SCE_GS_PSMT4:
        w = (w + 63) / 64;
        if (w & 1)
            w++;
        return w;
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// log2 of n rounded up (TEX0's TW / TH).
// FS2 0x002eae70

static int Tim2GetLog2(int n)
{
    int i;
    for (i = 31; i > 0; i--) {
        if (n & (int)(1u << i))
            break;
    }
    if (n > (int)(1u << i))
        i++;
    return i;
}
