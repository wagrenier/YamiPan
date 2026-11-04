/* ==========================================================================
 *  game/nodetransform.cpp
 *
 *  The code of the scene nodes (game/stagenode.h), 0x1d7c50..0x1d85b0: the
 *  node's placement (NodeTransform), the slots of the two node classes
 *  that keep bare matrices, and the base's walks over its children. The
 *  stage's "calcmatrix" is StageNodeBase::updateMatrix from each root.
 *  The file name is the port's (no source string).
 *
 *  Mapped, no code of their own:
 *    FS2 0x001d81c0, 0x001d81d0, 0x001d8250  a link's next, a list's head, a
 *        link's value: the out-of-line StageNodeList walks of the recursive
 *        functions; the host reads the fields.
 *    FS2 0x001d8410  a four-word copy (propagate); memcpy.
 *    FS2 0x001d85a0  StageNodeList's ctor (the StageList template).
 *    FS2 0x00188890, 0x001889d0, 0x002687c0  the classes' deleting dtors, the
 *        implicit ones over ~StageNodeBase.
 * ======================================================================== */
#include "game/heap.h"
#include "game/stagenode.h"
#include "game/vu0math.h"

// ──────────────────────────────────────────────────────────────────────
// The world matrix under the parent's, or the local one for a root. The
// matrix comes from outside (StageModel::bindBones), nothing here turns it.
// FS2 0x001d7c80
// callcheck: getWorldMatrix (the parent's virtual)

void StageBoneNode::rebuildMatrix()
{
    if (m_parent == nullptr)
        copyMatrix(m_world, m_local);
    else
        multiplyMatrix(m_world, m_parent->getWorldMatrix(), m_local);
}

// ──────────────────────────────────────────────────────────────────────
// Rebuilds both matrices when dirty; the flag stays set, the caller clears
// it (slot 0 / slot 5). With positionOnly the parent only places the
// position, and a node with no parent loses it: the local matrix's row 3 is
// still (0, 0, 0, 1) there, and the ROM copies that.
// FS2 0x001d7d50
// callcheck: getMatrix (the parent's virtual)

void NodeTransform::update(StageNodeBase *parent)
{
    if (dirty == 0)
        return;

    if (rotationXyz == 1)
        rotationMatrixXyz(local, rotation);
    else
        rotationMatrixZxy(local, rotation);

    if (positionOnly == 1) {
        if (parent == nullptr) {
            copyMatrix(world, local);
        } else {
            sceVu0FVECTOR placed;
            applyMatrix(placed, parent->getMatrix(), position);
            translateMatrix(world, local, placed);
        }
    } else {
        local[3][0] = position[0];
        local[3][1] = position[1];
        local[3][2] = position[2];
        local[3][3] = 1.0f;
        if (parent == nullptr)
            copyMatrix(world, local);
        else
            multiplyMatrix(world, parent->getMatrix(), local);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001d7ea0

void NodeTransform::setRotationVec(const float *rot)
{
    rotation[0] = rot[0];
    rotation[1] = rot[1];
    rotation[2] = rot[2];
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001d7ec0

void NodeTransform::setRotation(float x, float y, float z)
{
    rotation[0] = x;
    rotation[1] = y;
    rotation[2] = z;
    rotation[3] = 1.0f;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001d7ee0

void NodeTransform::setPosition(const float *pos)
{
    position[0] = pos[0];
    position[1] = pos[1];
    position[2] = pos[2];
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001d7f00

void NodeTransform::setPositionXyz(float x, float y, float z)
{
    position[0] = x;
    position[1] = y;
    position[2] = z;
    position[3] = 1.0f;
}

// ──────────────────────────────────────────────────────────────────────
// Dirty from the start, so the first calcmatrix builds the matrices.
// FS2 0x001d7f20

NodeTransform::NodeTransform()
{
    positionOnly = 0;
    rotationXyz = 0;
    position[0] = position[1] = position[2] = 0.0f;
    position[3] = 1.0f;
    rotation[0] = rotation[1] = rotation[2] = 0.0f;
    rotation[3] = 1.0f;
    identityMatrix(local);
    identityMatrix(world);
    dirty = 1;
}

// ──────────────────────────────────────────────────────────────────────
// The ROM unrolls the recursion three levels (with setInverse inline) and
// reaches the rest through the out-of-line list walks.
// FS2 0x001d7fc0
// callcheck: free, alloc, identityMatrix, setInverse, setInverseTree, begin, linkNext, linkValue (the unrolled levels are the recursion's, the list walks fields)

void StageNodeBase::setInverseTree(bool on)
{
    setInverse(on);
    for (StageNodeList::Node *link = m_children.head; link != nullptr; link = link->next)
        link->value->setInverseTree(on);
}

// ──────────────────────────────────────────────────────────────────────
// Asked twice to turn on, the ROM takes a second block and loses the first.
// FS2 0x001d81e0

void StageNodeBase::setInverse(bool on)
{
    if (!on) {
        if (m_inverse != nullptr)
            gGlobalHeap.free(m_inverse);
        m_inverse = nullptr;
    } else {
        m_inverse = (sceVu0FVECTOR *)gGlobalHeap.alloc(sizeof(sceVu0FMATRIX), sizeof(sceVu0FMATRIX));
        identityMatrix(m_inverse);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Unrolled three levels in the ROM, like setInverseTree.
// FS2 0x001d8260
// callcheck: getWorldMatrix, propagate, begin, linkNext, linkValue, copyPosition (the children's; the list walks are fields, the copy is memcpy)

void StageNodeBase::propagate()
{
    if (m_lastPosition != nullptr)
        memcpy(m_lastPosition, getWorldMatrix()[3], sizeof(sceVu0FVECTOR));
    for (StageNodeList::Node *link = m_children.head; link != nullptr; link = link->next)
        link->value->propagate();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001d8420
// callcheck: rebuildMatrix, getWorldMatrix (virtual slots)

void StageNodeBase::updateMatrix()
{
    rebuildMatrix();
    if (m_inverse != nullptr)
        invertRigidMatrix(m_inverse, getWorldMatrix());
    updateChildMatrices();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001d8480
// callcheck: rebuildMatrix, getWorldMatrix (virtual slots)

void StageNodeBase::updateChildMatrices()
{
    for (StageNodeList::Node *link = m_children.head; link != nullptr; link = link->next) {
        StageNodeBase *child = link->value;
        child->rebuildMatrix();
        if (child->m_inverse != nullptr)
            invertRigidMatrix(child->m_inverse, child->getWorldMatrix());
    }
    for (StageNodeList::Node *link = m_children.head; link != nullptr; link = link->next)
        link->value->updateChildMatrices();
}

// ──────────────────────────────────────────────────────────────────────
// Its fields' initialisers: no parent, no children, no blocks.
// FS2 0x001d8540
// callcheck: StageNodeList (m_children's ctor)

StageNodeBase::StageNodeBase()
{
}
