/* ==========================================================================
 *  game/mslsort.h
 *
 *  MSL's std::sort as the ROM's instances run it: a quicksort whose two
 *  sample points move by a counter kept between calls, and a selection sort
 *  under 21 elements. Neither is stable, so what the draw lists do with equal
 *  keys is this code's and the host's std::sort would answer differently.
 *  Every instance has its own counter; each instance here is its own template
 *  instance, so one comparator object must serve all the lists that shared
 *  an instance in the ROM. tools/reverse/sortcmp.py compares it with the ROM.
 * ======================================================================== */
#ifndef _GAME_MSLSORT_H
#define _GAME_MSLSORT_H

#include <utility>

// Leaves the median of the three in *c.
// FS2 0x00193480

template <class T, class Less>
void mslMedianToEnd(T *a, T *b, T *c, Less less)
{
    const bool aUpToC = !less(*c, *a);
    const bool cOverB = less(*b, *c);
    if (aUpToC && !cOverB)
        return;
    if (!aUpToC && cOverB) {
        std::swap(*a, *b);
        return;
    }
    if (less(*b, *a))
        std::swap(*a, *b);
    std::swap(aUpToC ? *b : *a, *c);
}

// FS2 0x001935b0

template <class T, class Less>
void mslSelectionSort(T *first, T *last, Less less)
{
    for (; first < last - 1; first++) {
        T *lowest = first;
        for (T *at = first + 1; at != last; at++) {
            if (less(*at, *lowest))
                lowest = at;
        }
        if (lowest != first)
            std::swap(*lowest, *first);
    }
}

// The last part is the case of a pivot nothing is below: everything equal to
// it is skipped, or a list of one key would never end.
// FS2 0x00192150
// callcheck: mslMedianToEnd_Effects, mslSelectionSort_Effects, mslSort_Effects (the ROM instance over the effects' pairs is the template here)

template <class T, class Less>
void mslSort(T *first, T *last, Less less)
{
    constexpr int kSelectionBelow = 21;
    constexpr int kShiftLow = -4;
    constexpr int kShiftHigh = 4;
    constexpr int kShiftSpan = 5;
    static int shift = kShiftLow;           // FS2 0x0031a8f0 in the effects' instance

    for (;;) {
        const int count = (int)(last - first);
        if (count < 2)
            return;
        if (count < kSelectionBelow) {
            mslSelectionSort(first, last, less);
            return;
        }

        const int nearSample = count / 4 + shift % kShiftSpan;
        shift = shift + 1 > kShiftHigh ? kShiftLow : shift + 1;
        const int farSample = count * 3 / 4 + shift % kShiftSpan;
        shift = shift + 1 > kShiftHigh ? kShiftLow : shift + 1;

        T *const pivot = last - 1;
        mslMedianToEnd(first + nearSample, first + farSample, pivot, less);

        T *up = first;
        T *down = pivot;
        while (less(*up, *pivot))
            up++;
        do {
            down--;
            if (up == down)
                break;
        } while (!less(*down, *pivot));
        if (up < down) {
            std::swap(*up, *down);
            up++;
            for (;;) {
                while (less(*up, *pivot))
                    up++;
                do {
                    down--;
                } while (!less(*down, *pivot));
                if (down <= up)
                    break;
                std::swap(*up, *down);
                up++;
            }
        }

        if (up != first) {
            if (up - first < last - up) {
                mslSort(first, up, less);
                first = up;
            }
            else {
                mslSort(up, last, less);
                last = up;
            }
            continue;
        }

        std::swap(*up, *pivot);
        T *above = up + 1;
        T *top = pivot;
        if (!less(*first, *pivot)) {
            while (above != last && !less(*first, *above))
                above++;
            if (above < top)
                std::swap(*above, *top);
        }
        if (above < top) {
            for (;;) {
                while (!less(*first, *above))
                    above++;
                do {
                    top--;
                } while (less(*first, *top));
                if (top <= above)
                    break;
                std::swap(*above, *top);
                above++;
            }
        }
        first = above;
    }
}

#endif /* _GAME_MSLSORT_H */
