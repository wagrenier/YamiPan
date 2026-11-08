/* ==========================================================================
 *  rena/tim2.h
 *
 *  Sony's sample TIM2 library as the game links it (0x2e97a8.., no source
 *  string: read from Tim2GetPictureHeader's own message; the file and the
 *  struct names are the sample's), all of it: 0x2e97a8..0x2eaeb0. rena's
 *  Tim2Texture calls the readers; nothing calls the writers, the GS
 *  transfers or the snapshot. TIM2 files are read in place, as the disc
 *  has them.
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

// What Tim2CheckFileHeaer answers: the file's kind, or 0 when broken.
enum Tim2FileKind
{
    Tim2File_Broken = 0,
    Tim2File_Tim2   = 1,        // "TIM2"
    Tim2File_Clt2   = 2,        // "CLT2": a CLUT-only file
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

// ClutType's high bits above the pixel kind.
#define TIM2_CLUT_TYPE_MASK     0x3f    // the Tim2ImageType of the entries
#define TIM2_CLUT_BLOCKED       0x40    // a 16-colour CLUT kept in the GS's CSM1 block order
#define TIM2_CLUT_LINEAR        0x80    // entries in linear order (the GS's CSM2)

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

// The user space's optional header ("eXt\0"): user data, then a comment.
struct TIM2_EXHEADER
{
    char     exHeaderId[4];     // FS2 +0x00: "eXt\0"
    u_int    userSpaceSize;     // FS2 +0x04: bytes, this header included
    u_int    userDataSize;      // FS2 +0x08: bytes after this header
    u_int    reserved;          // FS2 +0x0c
};
static_assert(sizeof(TIM2_EXHEADER) == 0x10, "TIM2_EXHEADER");

// The file's kind (Tim2FileKind); prints and answers 0 when broken.
int   Tim2CheckFileHeaer(void *tim2);                                    // FS2 0x002e97a8
TIM2_PICTUREHEADER *Tim2GetPictureHeader(void *tim2, int imgno);         // FS2 0x002e9868
// The level's size in *width / *height (each may be null); answers its
// bytes, rounded up to 16.
int   Tim2GetMipMapPictureSize(TIM2_PICTUREHEADER *ph, int mipmap,
                               int *width, int *height);                 // FS2 0x002e98f0
// The space between the headers and the pixels, null when there is none;
// *size (may be null) its bytes, or the eXt header's count.
void *Tim2GetUserSpace(TIM2_PICTUREHEADER *ph, int *size);               // FS2 0x002e99a8
// The user data after an eXt header (or the whole user space without one).
void *Tim2GetUserData(TIM2_PICTUREHEADER *ph, int *size);                // FS2 0x002e9a28
// The comment after the user data, null without an eXt header or a comment.
char *Tim2GetComment(TIM2_PICTUREHEADER *ph);                            // FS2 0x002e9a90
void *Tim2GetImage(TIM2_PICTUREHEADER *ph, int mipmap);                  // FS2 0x002e9af0
void *Tim2GetClut(TIM2_PICTUREHEADER *ph);                               // FS2 0x002e9b60
// Entry `no` of CLUT `clut` as 0xAABBGGRR (alpha 0x80 = opaque); 0 for a
// picture without an indexed image or past the CLUT.
u_int Tim2GetClutColor(TIM2_PICTUREHEADER *ph, int clut, int no);        // FS2 0x002e9b88
// Stores entry `no` of CLUT `clut` from 0xAABBGGRR; answers the old colour
// as Tim2GetClutColor would (0 and nothing stored where it answers 0).
u_int Tim2SetClutColor(TIM2_PICTUREHEADER *ph, int clut, int no,
                       u_int newcolor);                                  // FS2 0x002e9d90
// The raw texel at (x, y) of a mip level: a colour as the file stores it,
// or a CLUT index; 0 outside the level.
u_int Tim2GetTexel(TIM2_PICTUREHEADER *ph, int mipmap, int x, int y);    // FS2 0x002ea010
// Stores a raw texel and answers the old one.
u_int Tim2SetTexel(TIM2_PICTUREHEADER *ph, int mipmap, int x, int y,
                   u_int newtexel);                                      // FS2 0x002ea188
// The colour at (u, v) of level 0's coordinates in level `mipmap`, as
// 0xAABBGGRR, through CLUT `clut` for an indexed image.
u_int Tim2GetTextureColor(TIM2_PICTUREHEADER *ph, int mipmap, int clut,
                          int u, int v);                                 // FS2 0x002ea380

// The GS transfers. The ROM's cut takes the addresses from the picture's own
// GsTex0 / GsMiptbp (no tbp / cbp arguments); nothing in the ROM calls them.
// PC: no GS upload is made (textures are decoded from the file: Tim2Texture).
void  Tim2LoadPicture(TIM2_PICTUREHEADER *ph);                           // FS2 0x002ea490
void  Tim2LoadImage(TIM2_PICTUREHEADER *ph);                             // FS2 0x002ea4b8
void  Tim2LoadClut(TIM2_PICTUREHEADER *ph);                              // FS2 0x002ea610

struct sceGsDispEnv;
// Writes the displayed frame (d0; d1 the other field's, or null) to `fname`
// as a TIM2 file. 1 on success, 0 on an unknown pixel format or no file.
// PC: there is no GS frame to read back: the rows are written black.
int   Tim2TakeSnapshot(sceGsDispEnv *d0, sceGsDispEnv *d1, char *fname); // FS2 0x002ea8d8

#endif /* _RENA_TIM2_H */
