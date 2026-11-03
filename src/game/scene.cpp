// game/scene.cpp -- the scene controller (game/scene.h).

#include "game/scene.h"

#include "game/gamehooks.h"
#include "game/gfxtarget.h"
#include "game/heap.h"
#include "game/screen.h"
#include "game/stageresult.h"
#include "game/thread.h"
#include "sdk/scetypes.h"

constexpr u_int kSceneStackAlign = 0x10;

// The scenes (data 0x2fabd0): one in retail.
static SceneDef sScenes[] = {
    { "Menu (Pack File)", menuSceneThread, nullptr, (void *)1, 0, nullptr },
};

static int sSceneIndex;                         /* sbss 0x31ae3c (gp-0x7a34): read only */

// ──────────────────────────────────────────────────────────────────────
// Loads the resident packs and the game pack's tables, starts the
// overlay thread, then runs the current scene in slot 2 until it exits,
// over and over. After each scene the overlay's screen object goes and the
// next frame's draw env clears to black.
// FS2 0x00231bb0

void gameSceneThread(void *arg)
{
    (void)arg;
    loadInitPack();
    loadGamePack();
    overlayThreadStart();
    for (;;) {
        loadMenuPack();
        SceneDef *scene = &sScenes[sSceneIndex];
        // Allocated each time the scene starts and never freed (no retail
        // scene asks for a stack).
        if (scene->stackSize != 0)
            scene->stack = (u_int *)gGlobalHeap.alloc(scene->stackSize, kSceneStackAlign);
        threadSlotCreate(scene->func, ThreadSlot_02, scene->arg, scene->cleanup,
                         scene->stackSize, scene->stack);
        do
            threadYield(1);
        while (threadSlotState(ThreadSlot_02) != ThreadState_Free);
        overlayReleaseScreen();
        gfxSetDrawEnvClear(gfxGetCurrentDrawEnv(), 1, 0, 0, 0, 0);
    }
}
