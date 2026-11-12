/* ==========================================================================
 *  game/scene.h
 *
 *  The scene controller (game/scene.cpp, 0x231bb0..0x231c90): thread slot
 *  1. It loads the resident packs, runs the startup sequence, starts the
 *  overlay thread, then forever runs the current scene of the table
 *  0x2fabd0 in thread slot 2 and waits for it to end. Retail has one
 *  scene, "Menu (Pack File)" (menuSceneThread), and nothing writes the
 *  index, so the loop restarts the menu flow each time it returns.
 * ======================================================================== */
#ifndef _GAME_SCENE_H
#define _GAME_SCENE_H

#include <stddef.h>

#include "game/thread.h"
#include "sdk/scetypes.h"

// One scene: what gameSceneThread hands threadSlotCreate (EE 0x18 bytes).
// layout: host -- pointers; the EE offsets are in the comments.
struct SceneDef
{
    const char       *name;         // FS2 +0x00: read nowhere ("Menu (Pack File)")
    ThreadFunc        func;         // FS2 +0x04
    ThreadCleanupFunc cleanup;      // FS2 +0x08
    void             *arg;          // FS2 +0x0c
    int               stackSize;    // FS2 +0x10: 0 = the slot's default stack
    u_int            *stack;        // FS2 +0x14: Heap::alloc'ed when stackSize is set
};

// Thread slot 1 (gameMainThread starts it): never returns.
void gameSceneThread(void *arg);                                         // FS2 0x00231bb0

#endif /* _GAME_SCENE_H */
