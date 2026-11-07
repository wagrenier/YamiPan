/* ==========================================================================
 *  game/menuprogset.h
 *
 *  The gun sight over the frame, 0x2b9ef0..0x2bb510: the init pack's six
 *  images and what PlayerMotion::setScope chooses to show of them. The file
 *  name is the port's; so is the place of quatBetween, the function that
 *  follows the class in the ROM.
 * ======================================================================== */
#ifndef GAME_MENUPROGSET_H
#define GAME_MENUPROGSET_H

#include "sdk/libvu0.h"

class Tim2Texture;

// MenuProgSet::state: the sight drawn.
enum MenuProgSight
{
    MenuProgSight_None  = 0,
    MenuProgSight_Scope = 1,    // a rifle's: a soft dark edge and the spot
    MenuProgSight_R64C  = 2,    // R_64C's: a round hole, ticks and a cross
};

// layout: host -- EE 0x1c: the state, then six textures.
struct MenuProgSet
{
    MenuProgSet();                                                       // FS2 0x002bb4e0: seven words zero
    ~MenuProgSet();                                                      // FS2 0x002bb3e0: deleting
    // Six textures over the init pack's images.
    void load(void *image0, void *image1, void *image2, void *image3, void *image4,
              void *image5);                                             // FS2 0x002bb2b0
    // The sight over the frame, at PlayerMotion::scopeZoom.
    void draw(float zoom);                                               // FS2 0x002bb260

    int state = MenuProgSight_None; // FS2 +0x00: PlayerMotion::setScope

private:
    void drawR64C(float zoom);                                           // FS2 0x002b9ef0
    void drawScope();                                                    // FS2 0x002ba8d0

    Tim2Texture *m_scopeMask  = nullptr; // FS2 +0x04: image 0, a quarter of the rifle's edge
    Tim2Texture *m_spot       = nullptr; // FS2 +0x08: image 1, the spot in the centre
    Tim2Texture *m_r64cMask   = nullptr; // FS2 +0x0c: image 2, a quarter of the round hole
    Tim2Texture *m_tick       = nullptr; // FS2 +0x10: image 3
    Tim2Texture *m_lineAcross = nullptr; // FS2 +0x14: image 4
    Tim2Texture *m_lineDown   = nullptr; // FS2 +0x18: image 5
};

// The shortest turn taking `from` to `to` as a quaternion; `unit`: both are
// of length 1 already. Vectors opposite give the half turn about their cross.
void quatBetween(sceVu0FVECTOR out, const sceVu0FVECTOR from, const sceVu0FVECTOR to,
                 int unit);                                              // FS2 0x002bb510

#endif /* GAME_MENUPROGSET_H */
