/* ==========================================================================
 *  rena/rena_draw.h
 *
 *  rena's frame calls, as game/drawsys.cpp makes them (the run
 *  unassigned@002e2970, not cut into files yet; the names are the port's).
 *  In the ROM they write the VU1 program, its constants and the model
 *  packets; the host keeps the calls in the game's order and the run's TU
 *  turns them into native draws (no VU1 and no GS memory on the host).
 * ======================================================================== */
#ifndef _RENA_RENA_DRAW_H
#define _RENA_RENA_DRAW_H

#include "sdk/libvifpk.h"
#include "sdk/libvu0.h"

class DrawSysLights;            // game/drawsys.h
struct RenaSpotLight;           // game/scenerender.h
struct ModelSurface;            // game/gamehooks.h
class Tim2Texture;              // game/gsutil.h
struct DrawSysRecord;           // game/drawsys.h

void renaBeginFrame(sceVif1Packet *packet);                              // FS2 0x002e86b0
void renaSetSpotlight(sceVif1Packet *packet, Tim2Texture *texture, int block); // FS2 0x002e87f0
// The scene's spot for this frame's passes, null with no scene: VU addresses 201..211 and 171.
void renaSetSpotRecord(const RenaSpotLight *spot);                       // FS2 0x002d5710 (inline in drawSysRender)
void renaSetShadowTexture(sceVif1Packet *packet, Tim2Texture *texture, int block); // FS2 0x002e82a0
// The record's ambient, directional colours and directions.
void renaSetLights(sceVif1Packet *packet, DrawSysLights *lights);        // FS2 0x002e8080
// A shadow's texture projection and clip matrices (index 0..6).
void renaSetShadowMatrices(sceVif1Packet *packet, int index, sceVu0FMATRIX projection,
                           sceVu0FMATRIX clip);                          // FS2 0x002e8300
// How many shadows the next pass's models receive.
void renaSetShadowCount(sceVif1Packet *packet, int count);               // FS2 0x002e8570
// The record's models whose draw group is in mask.
void renaDrawModels(sceVif1Packet *packet, DrawSysRecord *record, int mask); // FS2 0x002e5450
void renaEndPass(sceVif1Packet *packet);                                 // FS2 0x002e85e0
// PC: a pass of the stage's effects, in the list's place: vertices in view space, over the depth so far. The ROM's
// GS state (TEST_1 no depth write, FOGCOL) as the host's. renaEndPass ends it.
void renaBeginEffects(sceVif1Packet *packet, const sceVu0FVECTOR *screen);
// One step of a texture animation (ModelInstance::texAnims).
void renaStepTexAnim(ModelSurface *surface, int ticks);                             // FS2 0x002e3bf0

#endif /* _RENA_RENA_DRAW_H */
