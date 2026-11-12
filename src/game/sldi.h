/* ==========================================================================
 *  game/sldi.h
 *
 *  "SLDI", the packing of the stage packs (game/sldi.cpp, the one function
 *  0x2d0a10; PackJob::load is its only caller). A 16-byte header, then
 *  tagged records that either copy bytes from the file or repeat bytes
 *  already written:
 *
 *    000nnnnn nnnnnnnn                  literal, n (13 bits) bytes follow
 *    001xxxxx nnnnnnnn nnnnnnnn         literal, n (16 bits, low byte first)
 *    01nnnnnn                           literal, n (6 bits)
 *    100ddddd dddddddd nnnnnnnn         repeat n bytes from d back (13 bits)
 *    101xxxxx dddddddd dddddddd nnnnnnnn   the same, d 16 bits, low byte first
 *
 *  Tag 11 is an error. x bits are ignored.
 * ======================================================================== */
#ifndef _GAME_SLDI_H
#define _GAME_SLDI_H

#include <stddef.h>

#include "sdk/scetypes.h"

// EE 0x10, from the disc: the code reads only magic and unpackedSize.
struct SldiHeader
{
    char magic[4];              // FS2 +0x0: "SLDI"
    u_int version;              // FS2 +0x4: 1 in every file on the disc; never read
    u_int packedSize;           // FS2 +0x8: the whole file, this header too; never read
    int   unpackedSize;         // FS2 +0xc
};
static_assert(sizeof(SldiHeader) == 0x10, "SldiHeader size");
static_assert(offsetof(SldiHeader, version) == 0x4, "SldiHeader version");
static_assert(offsetof(SldiHeader, packedSize) == 0x8, "SldiHeader packedSize");
static_assert(offsetof(SldiHeader, unpackedSize) == 0xc, "SldiHeader unpackedSize");

// A record's first byte.
constexpr u_char kSldiTagMask     = 0xc0;
constexpr u_char kSldiTagLiteral  = 0x00;   // length in the following bytes
constexpr u_char kSldiTagShortRun = 0x40;   // literal, length in the tag byte
constexpr u_char kSldiTagRepeat   = 0x80;
constexpr u_char kSldiWide        = 0x20;   // the 16-bit form of Literal / Repeat
constexpr u_char kSldiHighMask    = 0x1f;   // the 13-bit forms' high bits
constexpr u_char kSldiShortMask   = 0x3f;   // ShortRun's length

// Unpacks src into dst and answers the unpacked size; -1 for another magic,
// a bad tag or a total that does not add up. There is no bound on dst.
int sldiDecompress(const void *src, void *dst);                          // FS2 0x002d0a10

#endif /* _GAME_SLDI_H */
