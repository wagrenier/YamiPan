// rena/tim2.cpp -- Sony's sample TIM2 library, what rena's Tim2Texture calls
// (the rest of 0x2e97a8.. is not written yet).

#include "rena/tim2.h"

#include "rx/rxsys.h"

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
