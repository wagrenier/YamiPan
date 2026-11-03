/* ==========================================================================
 *  game/vcmtable.h
 *
 *  The VCM table of a model (game/vcmtable.cpp, 0x296d60..0x299d90): parts
 *  whose tag (the part's material name, or the nearest part up its group
 *  chain named "VCM__...") holds "__"-separated tokens are shown, faded or
 *  hidden by the objects named after those tokens (the controllers, bound
 *  by name with attach). Each distinct tag is one state with a mask of its
 *  tokens; update turns the controllers' weights into the state's fade /
 *  off bits and alpha and copies them into every part record linked to it.
 *  Tags on the disc look like "vcm__ENV_SL42c1" (one token each). The file
 *  and class names are the port's (from the VCM__ string; no source path).
 *  Its MSL string / set / map instances follow its code in the ROM: the
 *  host <string> / <set> / <map> provide them (list in vcmtable.cpp).
 * ======================================================================== */
#ifndef _GAME_VCMTABLE_H
#define _GAME_VCMTABLE_H

#include <stddef.h>

#include "sdk/scetypes.h"

class RenaModel;                // game/gamehooks.h (rena's model)
class VcmController;            // game/gamehooks.h
struct ModelPartRec;            // game/gamehooks.h

// The tag a part without a VCM__ part above it gets, and the prefix looked
// for on the parts' own names (case ignored).
#define VCM_TAG_PREFIX  "VCM__"   // FS2 0x00309c90
// What separates a tag's tokens; the first token follows the first one.
#define VCM_TOKEN_SEP   "__"      // FS2 0x00309c98

// update's alpha for a token's weight: weight * 128.
constexpr float kVcmAlphaScale = 128.0f;

// One distinct tag: the controllers it depends on (bits in the table's
// token order), and what update made of them. The bits below are compact:
// bit k is the k-th set bit of mask, not the token index.
struct VcmState
{
    u_int  mask;                // FS2 +0x0: 1 << token index per token
    u_char fadeBits;            // FS2 +0x4: tokens weighing under 1
    u_char offBits;             // FS2 +0x5: tokens weighing 0
    u_char alpha;               // FS2 +0x6: the last fading token's weight * 128
};
static_assert(sizeof(VcmState) == 0x8, "VcmState");
static_assert(offsetof(VcmState, fadeBits) == 0x4, "VcmState::fadeBits");
static_assert(offsetof(VcmState, alpha) == 0x6, "VcmState::alpha");

// A tagged part: its record and its state.
// layout: host -- EE 8: rec +0, state +4.
struct VcmPartLink
{
    ModelPartRec *rec;          // FS2 +0x0
    int           state;        // FS2 +0x4: index into m_states
};

// layout: host -- EE 0x1c (operator new(0x1c) in the model holder 0x188b10).
class VcmTable
{
public:
    explicit VcmTable(RenaModel *model);                                 // FS2 0x002972d0
    ~VcmTable();                                                         // FS2 0x00297210

    // The weights of the controllers now: every state, then every part.
    void update();                                                       // FS2 0x00296d60
    // Binds obj to the token equal to its name (nothing when none is).
    void attach(VcmController *obj);                                     // FS2 0x00297150

    int            m_tokenCount;    // FS2 +0x00
    const char   **m_tokens;        // FS2 +0x04: sorted by strcmp, owned
    VcmController **m_controllers;  // FS2 +0x08: per token, null until attached
    int            m_stateCount;    // FS2 +0x0c
    VcmState      *m_states;        // FS2 +0x10: in tag order (strcmp)
    int            m_linkCount;     // FS2 +0x14
    VcmPartLink   *m_links;         // FS2 +0x18: in part order
};

#endif /* _GAME_VCMTABLE_H */
