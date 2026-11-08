// rena/tim2.cpp -- Sony's sample TIM2 library as the game links it, all
// nine functions of 0x2e97a8..0x2e9d90 (the sample's Tim2IsClut2 and
// Tim2GetMipMapHeader are not in the ROM).

#include "rena/tim2.h"

#include "rx/rxsys.h"

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
// A CLUT entry as 0xAABBGGRR. 256-colour CLUTs without the linear bit
// (0x80), and 16-colour ones with TIM2_CLUT_BLOCKED, are kept in the GS's
// CSM1 block order: entries 8..15 and 16..23 of every 32 trade places.
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
