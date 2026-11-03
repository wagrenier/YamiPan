// game/wstring.cpp -- the game's 16-bit strings (game/wstring.h).

#include "game/wstring.h"

#include <stdint.h>
#include <string.h>

#include "sdk/scetypes.h"

// A 16-bit string must start on an even address; the helpers refuse any
// other pointer.
static inline bool isWideAligned(const void *p)
{
    return ((uintptr_t)p & 1) == 0;
}

// ──────────────────────────────────────────────────────────────────────
// Compares a with b over a's length only. b's length is counted as well
// and never used, and the b == 0 test cannot fire inside a's length (a
// non-zero unit of a against a 0 of b already answered 1): both kept.
// FS2 0x002d4150

int wstrcmp(const u_short *a, const u_short *b)
{
    if (a == NULL || b == NULL || !isWideAligned(a) || !isWideAligned(b)) {
        return 0;
    }
    int len = wstrlen(a);
    wstrlen(b);
    for (int i = 0; i < len; i++, a++, b++) {
        if (*b < *a) {
            return 1;
        }
        if (*a < *b) {
            return -1;
        }
        if (*b == 0) {
            return -1;
        }
    }
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Tries needle at each position of hay from the start.
// FS2 0x002d42a0

u_short *wstrstr(const u_short *hay, const u_short *needle)
{
    if (hay == NULL || needle == NULL || !isWideAligned(hay) || !isWideAligned(needle)) {
        return NULL;
    }
    int hayLen = wstrlen(hay);
    int needleLen = wstrlen(needle);
    if (hayLen < needleLen) {
        return NULL;
    }
    for (int start = 0; start <= hayLen - needleLen; start++) {
        bool match = true;
        for (int i = 0; i < needleLen; i++) {
            if (needle[i] != hay[start + i]) {
                match = false;
                break;
            }
        }
        if (match) {
            return (u_short *)hay + start;
        }
    }
    return NULL;
}

// ──────────────────────────────────────────────────────────────────────
// Each byte of src becomes one unit of dst, the 0 included. Widening in
// place would overwrite bytes not read yet, so dst == src goes through a
// new[] copy of src (terminated twice: strcpy, then by hand).
// FS2 0x002d4440
// callcheck: operator_new[], operator_delete[] (new char[], delete[])

u_short *wstrFromAscii(u_short *dst, const char *src)
{
    if (dst == NULL) {
        return NULL;
    }
    if (src == NULL) {
        return dst;
    }
    if (!isWideAligned(dst)) {
        return NULL;
    }
    if ((const void *)dst == (const void *)src) {
        char *copy = new char[strlen(src) + 1];
        strcpy(copy, src);
        copy[strlen(src)] = '\0';
        u_short *end = wstrFromAscii(dst, copy);
        delete[] copy;
        return end;
    }
    while ((*dst = (u_char)*src) != 0) {
        dst++;
        src++;
    }
    return dst;
}

// ──────────────────────────────────────────────────────────────────────
// The 16-bit strcpy; dst == src goes through a new[] copy like
// wstrFromAscii.
// FS2 0x002d4510
// callcheck: operator_new[], operator_delete[] (new u_short[], delete[])

u_short *wstrcpy(u_short *dst, const u_short *src)
{
    if (dst == NULL) {
        return NULL;
    }
    if (src == NULL) {
        return dst;
    }
    if (!isWideAligned(dst) || !isWideAligned(src)) {
        return NULL;
    }
    if (dst == src) {
        u_short *copy = new u_short[wstrlen(src) + 1];
        wstrcpy(copy, src);
        u_short *end = wstrcpy(dst, copy);
        delete[] copy;
        return end;
    }
    while ((*dst = *src) != 0) {
        dst++;
        src++;
    }
    return dst;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002d4650

int wstrlen(const u_short *s)
{
    if (s == NULL || !isWideAligned(s)) {
        return 0;
    }
    int len = 0;
    while (*s != 0) {
        s++;
        len++;
    }
    return len;
}
