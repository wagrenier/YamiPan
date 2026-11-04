/* ==========================================================================
 *  game/stagenode.h
 *
 *  The scene-node family (the object base is in game/stageobject.cpp; the
 *  transform's code in the run at 0x1d7c50; the file name is the port's):
 *  bones and StageSound's positional sources are nodes. What is built and
 *  called so far, nothing more.
 *
 *  A node hangs under a parent (+0x4) in the parent's child list (+0x8,
 *  links of {item cell, next, prev}); the base dtor unlinks it and orphans
 *  its children. Vtables: 0x314120 / 0x314150 (roots, unread) ->
 *  0x315ae0 (StageNodeBase) -> 0x315a80 (StageNode, a transform at +0x40)
 *  -> e.g. 0x315a20 (StageSoundSource).
 * ======================================================================== */
#ifndef _GAME_STAGENODE_H
#define _GAME_STAGENODE_H

#include "game/stagelist.h"
#include "sdk/libvu0.h"     /* sceVu0FVECTOR / FMATRIX */
#include "sdk/scetypes.h"

class StageNodeBase;

// A node's child list. EE 8: {head, count}.
typedef StageList<StageNodeBase *> StageNodeList;

// The node base (vtable 0x315ae0).
// layout: host -- EE: vptr +0, parent +0x4, children +0x8, two heap blocks
// +0x10 / +0x14 (freed by the dtor, heap 0x329970).
class StageNodeBase
{
public:
    StageNodeBase();                                                     // FS2 0x001d8540
    // Slots 0, 1, 3, 4, 5 are empty in the ROM's table: the classes' own.
    // Slot 0: the transform's dirty flag.
    virtual void setDirty(bool dirty) = 0;
    // Slot 1: the transform's matrices, under the parent's.
    virtual void rebuildMatrix() = 0;
    virtual ~StageNodeBase();                                            // FS2 0x001865c0: slot 2, deleting
    // Slot 3 (vtable +0x14), slot 4: the node's matrix, rows 0..2 the axes,
    // row 3 the position.
    virtual const sceVu0FVECTOR *getMatrix() = 0;
    virtual const sceVu0FVECTOR *getWorldMatrix() = 0;
    // Slot 5: setDirty on the node and everything under it.
    virtual void setDirtyTree(bool dirty) = 0;
    // Slot 6 (vtable +0x20): rebuilds the matrices ("calcmatrix"), the
    // inverse too when the node has one.
    virtual void updateMatrix();                                         // FS2 0x001d8420

    // Leaves the parent's child list, if in it. Inline in the ROM (the
    // dtor, StageSound::addSource).
    void detach()
    {
        if (m_parent == nullptr)
            return;
        for (StageNodeList::Node *link = m_parent->m_children.head; link != nullptr; link = link->next) {
            if (link->value == this) {
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
            m_parent = parent;
            parent->m_children.pushBack(this);
            m_parent = parent;
        }
    }

    StageNodeBase *m_parent = nullptr;  // FS2 +0x04: none: the node rebuilds its own matrix (StageSound::updateSlots)
    StageNodeList  m_children;          // FS2 +0x08
    float         *m_lastPosition = nullptr; // FS2 +0x10: a heap vector, only where a class allocates it: StageObject::propagate keeps the world position in it
    sceVu0FVECTOR *m_inverse = nullptr; // FS2 +0x14: the matrix's inverse, a heap block (StageObject::setModel); updateMatrix keeps it
};

// A node's placement (StageNode +0x40).
// layout: host -- EE 0xc0: flags +0 (dirty) / +1 / +2, position +0x10,
// rotation +0x20, two matrices +0x40 / +0x80 (the node's +0x80 / +0xc0).
struct NodeTransform
{
    NodeTransform();                                                     // FS2 0x001d7f20
    // Copies x, y, z (w stays).
    void setPosition(const float *pos);                                  // FS2 0x001d7ee0
    // The matrices from position and rotation, under parent's when dirty.
    void update(StageNodeBase *parent);                                  // FS2 0x001d7d50
    // The same from three floats, w = 1.
    void setPositionXyz(float x, float y, float z);                      // FS2 0x001d7f00
    void setRotation(float x, float y, float z);                         // FS2 0x001d7ec0

    u_char        dirty;        // FS2 +0x00: 1 after the ctor
    u_char        _01;          // FS2 +0x01
    u_char        _02;          // FS2 +0x02
    sceVu0FVECTOR position;     // FS2 +0x10: (0, 0, 0, 1)
    sceVu0FVECTOR rotation;     // FS2 +0x20: (0, 0, 0, 1)
    sceVu0FMATRIX local;        // FS2 +0x40 (0x136400)
    sceVu0FMATRIX world;        // FS2 +0x80 (0x136400): the node's +0xc0
};

// A scene node (vtable 0x315a80): bones, StageSound's positional sources.
// layout: host -- EE: StageNodeBase, the transform at +0x40, its world
// matrix at +0xc0.
class StageNode : public StageNodeBase
{
public:
    // FS2 0x00186a00
    void setDirty(bool dirty) override { m_xform.dirty = dirty; }
    // FS2 0x00186a10
    // callcheck: update
    void rebuildMatrix() override { m_xform.update(m_parent); }
    // FS2 0x00186a20
    const sceVu0FVECTOR *getMatrix() override { return m_xform.world; }
    // FS2 0x00186a30
    const sceVu0FVECTOR *getWorldMatrix() override { return m_xform.world; }
    // FS2 0x001d7ce0
    // callcheck: setDirty, setDirtyTree (the children's virtuals)
    void setDirtyTree(bool dirty) override
    {
        setDirty(dirty);
        for (StageNodeList::Node *link = m_children.head; link != nullptr; link = link->next)
            link->value->setDirtyTree(dirty);
    }

    NodeTransform m_xform;      // FS2 +0x40
};

#endif /* _GAME_STAGENODE_H */
