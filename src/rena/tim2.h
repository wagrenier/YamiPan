/* ==========================================================================
 *  rena/tim2.h
 *
 *  Sony's sample TIM2 library as the game links it (0x2e97a8.., no source
 *  string: read from Tim2GetPictureHeader's own message; the file and the
 *  struct names are the sample's). Only what rena's Tim2Texture calls is
 *  written so far. TIM2 files are read in place, as the disc has them.
 * ======================================================================== */
#ifndef _RENA_TIM2_H
#define _RENA_TIM2_H

#include "sdk/scetypes.h"

// PictFormat 0 aligns the pictures to 16 bytes, 1 to 128.
enum Tim2FormatId
{
    Tim2Format_Align16  = 0,
    Tim2Format_Align128 = 1,
};

// The image's and the CLUT's pixel kinds (ImageType, ClutType's low bits).
enum Tim2ImageType
{
    Tim2Image_Undefined = 0,
    Tim2Image_Rgb16     = 1,
    Tim2Image_Rgb24     = 2,
    Tim2Image_Rgb32     = 3,
    Tim2Image_Index4    = 4,
    Tim2Image_Index8    = 5,
};

// The file's header, 16 bytes, padded to 0x80 with Tim2Format_Align128.
struct TIM2_FILEHEADER
{
    char     fileId[4];         // FS2 +0x00: "TIM2"
    u_char   formatVersion;     // FS2 +0x04
    u_char   formatId;          // FS2 +0x05: Tim2FormatId
    u_short  pictures;          // FS2 +0x06
    char     reserved[8];       // FS2 +0x08: FS2 keeps a render target's tag at +0xc (GsTargetTag)
};
static_assert(sizeof(TIM2_FILEHEADER) == 0x10, "TIM2_FILEHEADER");

// One picture's header, 0x30. Its mip header follows when it has mip
// levels; then the pixels of every level, then the CLUT.
struct TIM2_PICTUREHEADER
{
    u_int    totalSize;         // FS2 +0x00
    u_int    clutSize;          // FS2 +0x04: bytes
    u_int    imageSize;         // FS2 +0x08: bytes
    u_short  headerSize;        // FS2 +0x0c
    u_short  clutColors;        // FS2 +0x0e
    u_char   pictFormat;        // FS2 +0x10
    u_char   mipMapTextures;    // FS2 +0x11
    u_char   clutType;          // FS2 +0x12: Tim2ImageType; bit 7 linear order
    u_char   imageType;         // FS2 +0x13: Tim2ImageType
    u_short  imageWidth;        // FS2 +0x14
    u_short  imageHeight;       // FS2 +0x16
    u_int    gsTex0[2];         // FS2 +0x18
    u_int    gsTex1[2];         // FS2 +0x20
    u_int    gsTexaFbaPabe;     // FS2 +0x28
    u_int    gsTexClut;         // FS2 +0x2c
};
static_assert(sizeof(TIM2_PICTUREHEADER) == 0x30, "TIM2_PICTUREHEADER");

// The mip header, right after the picture's when it has mip levels.
struct TIM2_MIPMAPHEADER
{
    u_int    gsMiptbp1[2];      // FS2 +0x00 (0x30 in the picture)
    u_int    gsMiptbp2[2];      // FS2 +0x08
    u_int    mmImageSize[8];    // FS2 +0x10 (0x40 in the picture): each level's bytes
};
static_assert(sizeof(TIM2_MIPMAPHEADER) == 0x30, "TIM2_MIPMAPHEADER");

TIM2_PICTUREHEADER *Tim2GetPictureHeader(void *tim2, int imgno);         // FS2 0x002e9868
// The level's size in *width / *height (each may be null); answers its
// bytes, rounded up to 16.
int   Tim2GetMipMapPictureSize(TIM2_PICTUREHEADER *ph, int mipmap,
                               int *width, int *height);                 // FS2 0x002e98f0
void *Tim2GetImage(TIM2_PICTUREHEADER *ph, int mipmap);                  // FS2 0x002e9af0
void *Tim2GetClut(TIM2_PICTUREHEADER *ph);                               // FS2 0x002e9b60

#endif /* _RENA_TIM2_H */
