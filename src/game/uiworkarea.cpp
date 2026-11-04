/* ==========================================================================
 *  game/uiworkarea.cpp
 *
 *  Four one-liners between the file before and game/uilayout.cpp: UiTrack's
 *  two empty base slots (no other file holds them, uitrack.cpp starts at
 *  0x249550) and the UI work area's setter and getter. Placed by position.
 * ======================================================================== */
#include "game/uiworkarea.h"

#include "game/uisequence.h"

static void *sUiWorkArea;   /* sbss 0x31ae40 (gp-0x7a30) */

// ──────────────────────────────────────────────────────────────────────
// A track that draws nothing: the transform and colour kinds keep this
// slot and override apply.
// FS2 0x00243350

void UiTrack::draw(UiDrawer *drawer)
{
    (void)drawer;
}

// ──────────────────────────────────────────────────────────────────────
// A track with no first vertex; the shapes override it.
// FS2 0x00243360

const float *UiTrack::getAnchor()
{
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00243370

void setUiWorkArea(void *area)
{
    sUiWorkArea = area;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00243380

void *getUiWorkArea()
{
    return sUiWorkArea;
}
