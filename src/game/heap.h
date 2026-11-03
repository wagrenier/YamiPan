/* ==========================================================================
 *  game/heap.h
 *
 *  The global heap 0x329970 and the game RNG around 0x1037c0..0x104c60
 *  (unassigned: MSL or game code is open, open-items.md). Minimal: only
 *  what game/gamesystem.cpp touches.
 * ======================================================================== */
#ifndef _GAME_HEAP_H
#define _GAME_HEAP_H

// Usage marks GameSystem::flipFrame reports; mark 2 is reset every frame.
// The meaning of marks 0 and 1 is a guess until their setters are read.
enum HeapMark
{
    HeapMark_Base  = 0,         // guess
    HeapMark_Stage = 1,         // guess
    HeapMark_Frame = 2,         // GameSystem::flipFrame sets it each frame
};

// layout: host -- contents not read yet (a semaphore and a block, by
// shutdownGlobalHeap).
class Heap
{
public:
    void setMark(int mark);                                              // FS2 0x001037c0
    int  markedUsage(int mark);                                          // FS2 0x00103870
};

extern Heap gGlobalHeap;        /* bss 0x329970 */

void initGlobalHeap(int arg);                                            // FS2 0x00104600
void shutdownGlobalHeap();                                               // FS2 0x001045a0
void rndInit(int seed);                                                  // FS2 0x00104860

#endif /* _GAME_HEAP_H */
