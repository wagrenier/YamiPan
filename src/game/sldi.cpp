// game/sldi.cpp -- "SLDI" unpacking (game/sldi.h). The file name is the
// port's: no source string, the function sits alone between the scene
// model code and 0x2d0d10.

#include "game/sldi.h"

#include <string.h>

#include "sdk/scetypes.h"

// ──────────────────────────────────────────────────────────────────────
// Walks the records until the header's size is reached. A repeat copies
// forward one byte at a time, so a distance shorter than the count
// repeats a pattern (the ROM does it eight bytes a turn, the same order).
// PackJob::load unpacks in place, the file at the end of the block and
// dst its start: every pack on the disc comes out the same that way
// (114 files, tried at the stage packs' 0x1800000 bytes).
// FS2 0x002d0a10

int sldiDecompress(const void *src, void *dst)
{
    const SldiHeader *header = (const SldiHeader *)src;
    if (strncmp(header->magic, "SLDI", sizeof(header->magic)) != 0)
        return -1;

    const int size = header->unpackedSize;
    const u_char *in = (const u_char *)(header + 1);
    u_char *out = (u_char *)dst;
    int done = 0;

    while (done < size) {
        u_char tag = in[0];
        int count;

        switch (tag & kSldiTagMask) {
        case kSldiTagRepeat: {
            int distance;
            if ((tag & kSldiWide) != 0) {
                distance = in[1] | in[2] << 8;
                count = in[3];
                in += 4;
            } else {
                distance = (tag & kSldiHighMask) << 8 | in[1];
                count = in[2];
                in += 3;
            }
            for (int i = 0; i < count; i++)
                out[done + i] = out[done - distance + i];
            break;
        }
        case kSldiTagShortRun:
            count = tag & kSldiShortMask;
            memmove(out + done, in + 1, count);
            in += count + 1;
            break;
        case kSldiTagLiteral:
            if ((tag & kSldiWide) != 0) {
                count = in[1] | in[2] << 8;
                memmove(out + done, in + 3, count);
                in += count + 3;
            } else {
                count = (tag & kSldiHighMask) << 8 | in[1];
                memmove(out + done, in + 2, count);
                in += count + 2;
            }
            break;
        default:
            return -1;
        }
        done += count;
    }
    return done == size ? size : -1;
}
