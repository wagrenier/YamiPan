/* ==========================================================================
 *  game/stagenode.h
 *
 *  Stand-ins for the scene-node family of the run 0x17fce0..
 *  (unassigned@0017fce0, not cut yet; the file name is the port's): bones
 *  and StageSound's positional sources are nodes. What game/stagesound.cpp
 *  builds and calls, nothing more; it moves to the run's own header when
 *  that TU is written.
 *
 *  A node hangs under a parent (+0x4) in the parent's child list (+0x8,
 *  links of {item cell, next, prev}); the base dtor unlinks it and orphans
 *  its children. Vtables: 0x314120 / 0x314150 (roots, unread) ->
 *  0x315ae0 (StageNodeBase) -> 0x315a80 (StageNode, a transform at +0x40)
 *  -> e.g. 0x315a20 (StageSoundSource).
 * ======================================================================== */
#ifndef _GAME_STAGENODE_H
#define _GAME_STAGENODE_H

#include "sdk/libvu0.h"     /* sceVu0FVECTOR / FMATRIX */
#include "sdk/scetypes.h"

class StageNodeBase;

// A link of a node's child list. EE 0xc.
// layout: host -- EE: +0 the item cell (4 bytes holding the node), +4
// next, +8 prev.
struct StageNodeLink
{
    StageNodeBase **item;       // FS2 +0x0
    StageNodeLink  *next;       // FS2 +0x4
    StageNodeLink  *prev;       // FS2 +0x8
};

// A node's child list. EE 8.
// layout: host -- EE {head, count}.
struct StageNodeList
{
    StageNodeLink *head = nullptr;  // FS2 +0x0
    int            count = 0;       // FS2 +0x4

    // Appends a link holding a copy of *item.
    void pushBack(StageNodeBase **item);                                 // FS2 0x001842c0
    // Unlinks and frees link.
    void erase(StageNodeLink *link);                                     // FS2 0x00184370
};

// The node base (vtable 0x315ae0).
// layout: host -- EE: vptr +0, parent +0x4, children +0x8, two heap blocks
// +0x10 / +0x14 (freed by the dtor, heap 0x329970).
class StageNodeBase
{
public:
    StageNodeBase();                                                     // FS2 0x001d8540
    virtual ~StageNodeBase();                                            // FS2 0x001865c0: deleting

    // Leaves the parent's child list, if in it. Inline in the ROM (the
    // dtor, StageSound::addSource).
    void detach()
    {
        if (m_parent == nullptr)
            return;
        for (StageNodeLink *link = m_parent->m_children.head; link != nullptr; link = link->next) {
            if (*link->item == this) {
                m_parent->m_children.erase(link);
                m_parent = nullptr;
                break;
            }
        }
    }
    // Hangs the node under parent (null: only detached). Inline in the ROM
    // (StageSound::addSource).
    void setParent(StageNodeBase *parent)
    {
        detach();
        if (parent != nullptr) {
            StageNodeBase *self = this;
            m_parent = parent;
            parent->m_children.pushBack(&self);
            m_parent = parent;
        }
    }

    StageNodeBase *m_parent = nullptr;  // FS2 +0x04: none: the node rebuilds its own matrix (StageSound::updateSlots)
    StageNodeList  m_children;          // FS2 +0x08
    void          *_10 = nullptr;       // FS2 +0x10: a heap block (unread)
    void          *_14 = nullptr;       // FS2 +0x14: a heap block (unread)
};

// A node's placement (StageNode +0x40).
// layout: host -- EE 0xc0: flags +0 (dirty) / +1 / +2, position +0x10,
// rotation +0x20, two matrices +0x40 / +0x80 (the node's +0x80 / +0xc0).
struct NodeTransform
{
    NodeTransform();                                                     // FS2 0x001d7f20
    // Copies x, y, z (w stays).
    void setPosition(const float *pos);                                  // FS2 0x001d7ee0

    u_char        dirty;        // FS2 +0x00: 1 after the ctor
    u_char        _01;          // FS2 +0x01
    u_char        _02;          // FS2 +0x02
    sceVu0FVECTOR position;     // FS2 +0x10: (0, 0, 0, 1)
    sceVu0FVECTOR rotation;     // FS2 +0x20: (0, 0, 0, 1)
    sceVu0FMATRIX local;        // FS2 +0x40 (0x136400)
    sceVu0FMATRIX world;        // FS2 +0x80 (0x136400): the node's +0xc0
};

// A scene node (vtable 0x315a80): bones, StageSound's positional sources.
// The virtuals StageSound calls are plain stubs until their TU.
// layout: host -- EE: StageNodeBase, the transform at +0x40, its world
// matrix at +0xc0 (0x186a20 / 0x186a30 answer it).
class StageNode : public StageNodeBase
{
public:
    // Slot 3 / 4 (vtable +0x14 / +0x18): the node's matrix, rows 0..2 the
    // axes, row 3 the position (both this +0xc0 in 0x315a80).
    const sceVu0FVECTOR *getMatrix();                                    // FS2 vtable +0x14
    const sceVu0FVECTOR *getWorldMatrix();                               // FS2 vtable +0x18
    // Slot 6 (vtable +0x20): rebuilds the matrix (0x1d8420 in 0x315a20).
    void updateMatrix();                                                 // FS2 vtable +0x20

    NodeTransform m_xform;      // FS2 +0x40
};

#endif /* _GAME_STAGENODE_H */
