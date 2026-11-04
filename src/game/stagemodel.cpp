/* ==========================================================================
 *  game/stagemodel.cpp
 *
 *  StageModel (0x187230..0x188d60; declared in game/gamehooks.h, the file
 *  name is the port's): what an object of the stage holds of its model: the
 *  rena instance (+0), its VCM table, the pose that plays motions, and two
 *  node arrays that tie the instance's bones into the scene tree.
 *
 *  Every bone of the instance has a matrix pointer (ModelBone::matrix). A
 *  bone the pose owns points at the pose's bone (setBaseMotion), reached in
 *  the tree through a StageMatrixNode under its name in boneMap. Any other
 *  bone gets a StageBoneNode (bindBones): a bind matrix under its parent's
 *  node, so a rigid part follows the object or the bone it hangs from.
 *
 *  The MSL map instances after 0x188d60 (0x188d60..0x189ce0) are StringMap:
 *  game/stagelist.h. The pose is game/modelpose.h, mostly still stubs.
 * ======================================================================== */
#include <new>

#include "game/drawsys.h"
#include "game/gamehooks.h"
#include "game/heap.h"
#include "game/mathutil.h"
#include "game/modelpose.h"
#include "game/vcmtable.h"
#include "game/vu0math.h"

static const float kPi2 = 1.5707964f;           /* 0x3fc90fdb */
// applyMotion repeats while the pose stands farther than this from where
// the object is, squared, on x and y.
constexpr float kMotionSettled = 25.0f;
constexpr int   kAlignNodes = 0x40;
constexpr int   kNameInline = kPoseBoneName - 1;

// A character's neck: the first three letters of the model. The first name
// has its own limits.
static const char *const kNeckCharacters[] = { "kyo", "tka", "yrt", "jei", "hrs" }; /* 0x2f7cf0 */
static const sceVu0FVECTOR kNeckLimitsFirst = { 23.0f, 11.0f, 10.0f, 1.0f };
static const sceVu0FVECTOR kNeckLimitsOther = { 23.0f, 14.0f, 11.0f, 1.0f };
// The models that keep their neck bone (the first six letters).
static const char *const kNeckBoneModels[] = { "ryh_ky", "ekm_ky" };            /* 0x2f7d08 */
static const char kNeckBone[] = "bn_d_neck";    /* 0x3039f0 */
// These models are never marked distant (four letters).
static const char kNeverDistantModel[] = "kkb_"; /* 0x303a00 */

// newlib's strncasecmp 0x1285d0 (ASCII); not in every host libc.
static int nameCompareNoCase(const char *a, const char *b, size_t n)
{
    for (; n != 0; n--, a++, b++) {
        int ca = (*a >= 'A' && *a <= 'Z') ? *a + 0x20 : *a;
        int cb = (*b >= 'A' && *b <= 'Z') ? *b + 0x20 : *b;
        if (ca != cb || ca == 0)
            return ca - cb;
    }
    return 0;
}

// The motion's events, by what they are called in its file. A HIT_.._BEGIN
// event names its limb.
// layout: host -- the port's tables.
struct MotionEventName
{
    const char *name;
    u_int       flag;
};
static const MotionEventName kHitLimbs[] = {
    { "_RH_", MotionEvent_HitRightHand }, /* 0x303968 */
    { "_LH_", MotionEvent_HitLeftHand },  /* 0x303970 */
    { "_RL_", MotionEvent_HitRightLeg },  /* 0x303978 */
    { "_LL_", MotionEvent_HitLeftLeg },   /* 0x303980 */
    { "_HD_", MotionEvent_HitHead },      /* 0x303988 */
    { "_PV_", MotionEvent_HitPelvis },    /* 0x303990 */
};
static const MotionEventName kPlainEvents[] = {
    { "HIT_END",   MotionEvent_HitEnd },   /* 0x303998 */
    { "HIT_SWING", MotionEvent_HitSwing }, /* 0x3039a0 */
    { "BEGIN",     MotionEvent_Begin },    /* 0x303960 */
    { "END",       MotionEvent_End },      /* 0x3039b0 */
    { "STEP_L",    MotionEvent_StepLeft }, /* 0x3039b8 */
    { "STEP_R",    MotionEvent_StepRight }, /* 0x3039c0 */
    { "CLICK",     MotionEvent_Click },    /* 0x3039c8 */
    { "CANCEL",    MotionEvent_Cancel },   /* 0x3039d0 */
    { "CANCEL_W",  MotionEvent_CancelW },  /* 0x3039d8 */
    { "SOUND",     MotionEvent_Sound },    /* 0x3039e8 */
};
static u_int motionEventFlag(const char *event)
{
    if (strstr(event, "HIT_") != nullptr && strstr(event, "BEGIN") != nullptr) {
        for (const MotionEventName &limb : kHitLimbs)
            if (strstr(event, limb.name) != nullptr)
                return limb.flag;
        return MotionEvent_HitRightHand;
    }
    for (const MotionEventName &plain : kPlainEvents)
        if (strcmp(event, plain.name) == 0)
            return plain.flag;
    return 0;
}

// ──────────────────────────────────────────────────────────────────────
// Inline in the ROM (bindBones).

bool StageModel::poseOwnsBone(int bone) const
{
    return pose != nullptr && pose->getBoneMatrix(bone) != nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00187230
// callcheck: update

void StageModel::updateVcm()
{
    if (data != nullptr)
        vcm->update();
}

// ──────────────────────────────────────────────────────────────────────
// One step of the motion: the pose starts from position and rotation, plays
// frames, and what it moved goes back into both (and into the steps). Then
// the surface animation and the frame's events.
// FS2 0x00187260
// callcheck: subAngles, setPosition, rotate, clearPlacement, subAngles, addAngles, matrixToRotZYX, subAngles, addAngles, getPosition2, getSurfaceFrames, setFrames, popEvent, strstr, strstr, strstr, strstr, strstr, strstr, strstr, strstr, strcmp, strcmp, strcmp, strcmp, strcmp, strcmp, strcmp, strcmp, strcmp, strcmp, popEvent (motionEventFlag: the ROM's chains as tables)

void StageModel::advance(float frames, sceVu0FVECTOR position, sceVu0FVECTOR rotation, int arg)
{
    if (pose == nullptr)
        return;

    if (pose->parentMatrix == nullptr) {
        sceVu0FVECTOR turn;
        subAngles(turn, rotation, pose->rotation);
        pose->setPosition(position);
        pose->rotate(turn);
    } else {
        pose->clearPlacement();
    }
    pose->update(frames, m_loop != 0);
    m_motionFrames += frames;

    if (pose->parentMatrix == nullptr) {
        subAngles(m_rotationStep, pose->rotation, rotation);
        addAngles(rotation, rotation, m_rotationStep);
    } else {
        sceVu0FVECTOR world;
        matrixToRotZYX(world, pose->parentMatrix);
        subAngles(m_rotationStep, world, rotation);
        addAngles(rotation, rotation, m_rotationStep);
    }

    if (pose->parentMatrix == nullptr) {
        // PC: the ROM's w is what the stack held; the position keeps its own.
        sceVu0FVECTOR at;
        at[3] = position[3];
        pose->getPosition2(at);
        for (int i = 0; i < 4; i++) {
            m_positionStep[i] = at[i] - position[i];
            position[i] += m_positionStep[i];
        }
    } else {
        memset(m_positionStep, 0, sizeof(m_positionStep));
        memcpy(position, pose->rootBone->matrix[3], sizeof(sceVu0FVECTOR));
    }

    if (data != nullptr && m_animateSurface != 0 && m_kind1Surface != nullptr) {
        // PC: zero where the ROM reads the stack when there is no animation.
        float surfaceFrames[3] = { 0.0f, 0.0f, 0.0f };
        pose->getSurfaceFrames(surfaceFrames);
        m_kind1Surface->setFrames(surfaceFrames[2], (int)surfaceFrames[0], (int)surfaceFrames[1]);
    }

    m_motionEvents = 0;
    char event[kPoseEventName];
    while (pose->popEvent(event))
        m_motionEvents |= motionEventFlag(event);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00187720
// callcheck: play, playLayered

void StageModel::playMotion(ModelMotion *motion, bool arg0, bool layered, u_int flags, int arg)
{
    u_int play = (arg0 ? (MotionPlay_Arg0 | MotionPlay_Base) : MotionPlay_Base) | flags;
    if (!layered)
        pose->play(motion, play, arg);
    else
        pose->playLayered(motion, play, arg);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00187770
// callcheck: FUN_00189050, find, play, playLayered (StringMap::find)

void StageModel::playNamed(int index, const char *name, bool arg0, bool layered, u_int flags, int arg)
{
    MotionMap::Node *node = motions[index].find(name);
    ModelMotion *motion = node != nullptr ? node->value : nullptr;
    u_int play = (arg0 ? (MotionPlay_Arg0 | MotionPlay_Base) : MotionPlay_Base) | flags;
    if (!layered)
        pose->play(motion, play, arg);
    else
        pose->playLayered(motion, play, arg);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00187820
// callcheck: playFile, playFileLayered

void StageModel::playMotionFile(void *file, bool arg0, bool layered, u_int flags, int arg)
{
    u_int play = (arg0 ? (MotionPlay_Arg0 | MotionPlay_Base) : MotionPlay_Base) | flags;
    if (!layered)
        pose->playFile(file, play, arg);
    else
        pose->playFileLayered(file, play, arg);
}

// ──────────────────────────────────────────────────────────────────────
// Runs the pose at frame 0 until it stands where the object is; no step is
// left over for the next advance.
// FS2 0x00187870
// callcheck: subAngles, setPosition, rotate, clearPlacement, resetSwingBones, getPosition

void StageModel::applyMotion(const sceVu0FVECTOR position, const sceVu0FVECTOR rotation)
{
    float distance = 0.0f;
    do {
        m_motionEvents = 0;
        if (pose->parentMatrix == nullptr) {
            sceVu0FVECTOR turn;
            subAngles(turn, rotation, pose->rotation);
            pose->setPosition(position);
            pose->rotate(turn);
        } else {
            pose->clearPlacement();
        }
        pose->resetSwingBones();
        pose->update(0.0f, false);
        if (pose->parentMatrix == nullptr) {
            float at[3];
            pose->getPosition(at);
            float dy = at[1] - position[1];
            float dx = at[0] - position[0];
            distance = dx * dx + dy * dy;
        }
    } while (distance > kMotionSettled);
    memset(m_rotationStep, 0, sizeof(m_rotationStep));
    memset(m_positionStep, 0, sizeof(m_positionStep));
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001879b0
// callcheck: FUN_00189050, find (StringMap::find)

ModelMotion *StageModel::findMotion(int index, const char *name)
{
    MotionMap::Node *node = motions[index].find(name);
    return node != nullptr ? node->value : nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x001879f0
// callcheck: FUN_00189050, find (StringMap::find)

void *StageModel::findMotionFile(int index, const char *name)
{
    MotionMap::Node *node = motions[index].find(name);
    return node != nullptr ? node->value->file : nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00187a30
// callcheck: FUN_00189700, balance (StringMap::balance)

void StageModel::balanceMotions()
{
    for (MotionMap &map : motions)
        if (map.root != nullptr)
            map.balance();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00187a90
// callcheck: heapAllocAligned, ModelMotion, FUN_00188df0, insert (the aligned new; FUN_00188df0 is StringMap::insert)

ModelMotion *StageModel::addMotion(void *file, int index, const char *key)
{
    ModelMotion *motion = new ModelMotion(pose, file);
    motions[index].insert(key != nullptr ? key : (const char *)file + kMotionFileName, motion);
    return motion;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00187b40
// callcheck: attach

void StageModel::attachVcm(StageLight *obj)
{
    if (vcm != nullptr)
        vcm->attach(obj);
}

// ──────────────────────────────────────────────────────────────────────
// The nodes bindBones left at the top move under owner, keeping their place
// in the world.
// FS2 0x00187b70
// callcheck: getWorldMatrix, invertRigidMatrix, getWorldMatrix, multiplyMatrix, erase, erase, pushBack, setParent (setParent, inline in the ROM)

void StageModel::placeBones(StageObject *owner)
{
    if (data == nullptr)
        return;

    sceVu0FMATRIX inverse;
    invertRigidMatrix(inverse, owner->getWorldMatrix());
    for (int i = 0; i < m_boneNodeCount; i++) {
        StageBoneNode *node = &m_boneNodes[i];
        if (node->m_parent == nullptr) {
            multiplyMatrix(node->m_local, inverse, node->getWorldMatrix());
            node->setParent(owner);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// A node for every bone the pose does not own, each under its parent's: the
// parent's own node, or the pose's node of that name. The bone's matrix then
// is the node's world matrix. A bone with no parent keeps the matrix it had
// as the node's first world matrix, and placeBones gives it its parent.
// FS2 0x00187d10
// callcheck: operator_new[], getBoneMatrix, alloc, StageNodeBase, getBoneMatrix, getBoneMatrix, find, erase, erase, pushBack, multiplyMatrix, multiplyMatrix, operator_delete[], multiplyMatrix, copyMatrix, erase, erase, pushBack, setParent, getWorldMatrix, get, new, poseOwnsBone (setParent, inline in the ROM)

void StageModel::bindBones()
{
    if (data == nullptr)
        return;

    u_int count = data->boneCount;
    int *nodeOfBone = new int[count];
    int own = 0;
    for (u_int i = 0; i < count; i++) {
        nodeOfBone[i] = -1;
        if (!poseOwnsBone(i))
            nodeOfBone[i] = own++;
    }

    m_boneNodes = (StageBoneNode *)gGlobalHeap.alloc(own * sizeof(StageBoneNode), kAlignNodes);
    for (int i = 0; i < own; i++)
        new (&m_boneNodes[i]) StageBoneNode;
    m_boneNodeCount = own;

    StageBoneNode *node = m_boneNodes;
    for (u_int i = 0; i < count; i++) {
        ModelBone *bone = &data->bones[i];
        if (poseOwnsBone(i))
            continue;

        int parent = bone->node->parent;
        if (parent < 0) {
            multiplyMatrix(node->m_local, data->rootMatrix, data->bindMatrices[i]);
            copyMatrix(node->m_world, bone->matrix);
            bone->matrix = node->getWorldMatrix();
        } else {
            if (!poseOwnsBone(parent)) {
                node->setParent(&m_boneNodes[nodeOfBone[parent]]);
            } else {
                // The ROM does not test the find for null.
                const char *name = data->bones[parent].node->name.get();
                node->setParent(boneMap.find(name)->value);
            }
            sceVu0FMATRIX bind;
            multiplyMatrix(bind, data->rootMatrix, data->bindMatrices[i]);
            multiplyMatrix(node->m_local, data->inverseBinds[parent], bind);
            bone->matrix = node->getWorldMatrix();
        }
        node++;
    }
    delete[] nodeOfBone;
}

// ──────────────────────────────────────────────────────────────────────
// The pose of kind (first call only) with the motion file, one StageMatrixNode
// per pose bone in boneMap, and the instance's bones repointed at the pose's.
// Kinds outside the five leave no pose: the ROM goes on through null.
// FS2 0x00188140
// callcheck: operator_new[], memset, strlen, strcpy, memcpy, operator_delete[], strncasecmp, strncasecmp, strcmp, strncasecmp, alloc, StageNodeBase, FUN_00188f30, FUN_001895c0, insert, balance, load, getBoneMatrix, heapAllocAligned, ModelPose, heapAllocAligned, CarPose, heapAllocAligned, ElevatorPose, heapAllocAligned, NmlPose, heapAllocAligned, SkyPose, nameCompareNoCase (the aligned new; FUN_00188f30, FUN_001895c0 are StringMap::insert and balance)

void StageModel::setBaseMotion(void *file, int kind)
{
    if (pose == nullptr) {
        m_poseKind = kind;
        switch (m_poseKind) {
        case ModelPose_Sky:      pose = new SkyPose(0, 0); break;
        case ModelPose_Nml:      pose = new NmlPose(0, 0, 0, 0); break;
        case ModelPose_Elevator: pose = new ElevatorPose; break;
        case ModelPose_Car:      pose = new CarPose; break;
        case ModelPose_Model:    pose = new ModelPose; break;
        }
    }

    if (data == nullptr) {
        pose->load(file, 0, nullptr, 0);
    } else {
        u_int count = data->boneCount;
        char *names = new char[count * kPoseBoneName];
        memset(names, 0, count * kPoseBoneName);
        for (u_int i = 0; i < count; i++) {
            const char *name = data->bones[i].node->name.get();
            if (strlen(name) < kPoseBoneName)
                strcpy(names + i * kPoseBoneName, name);
            else
                memcpy(names + i * kPoseBoneName, name, kNameInline);
        }
        pose->load(file, 0, names, count);
        delete[] names;
    }

    for (size_t i = 0; i < sizeof(kNeckCharacters) / sizeof(kNeckCharacters[0]); i++) {
        if (nameCompareNoCase(pose->name, kNeckCharacters[i], 3) == 0) {
            pose->hasNeckLimits = 1;
            memcpy(pose->neckLimits, i == 0 ? kNeckLimitsFirst : kNeckLimitsOther, sizeof(sceVu0FVECTOR));
            pose->neckAxis[0] = 0.0f;
            pose->neckAxis[1] = 0.0f;
            pose->neckAxis[2] = 1.0f;
            pose->neckAxis[3] = kPi2;
            break;
        }
    }
    for (const char *model : kNeckBoneModels) {
        if (nameCompareNoCase(pose->name, model, 6) == 0) {
            for (int i = 0; i < pose->boneCount; i++) {
                if (strcmp(pose->bones[i].name, kNeckBone) == 0) {
                    pose->neckBone = &pose->bones[i];
                    break;
                }
            }
            break;
        }
    }
    if (nameCompareNoCase(pose->name, kNeverDistantModel, 4) == 0)
        m_mayGoDistant = 0;

    m_poseNodes = (StageMatrixNode *)gGlobalHeap.alloc(pose->boneCount * sizeof(StageMatrixNode), kAlignNodes);
    for (int i = 0; i < pose->boneCount; i++) {
        StageMatrixNode *node = new (&m_poseNodes[i]) StageMatrixNode;
        node->m_matrix = pose->bones[i].matrix;
        boneMap.insert(pose->bones[i].name, node);
    }
    if (boneMap.root != nullptr)
        boneMap.balance();

    if (data != nullptr) {
        u_int count = data->boneCount;
        for (u_int i = 0; i < count; i++) {
            const sceVu0FVECTOR *matrix = pose->getBoneMatrix(i);
            if (matrix != nullptr)
                data->bones[i].matrix = matrix;
        }
    }
    m_motionFile = file;
}

// ──────────────────────────────────────────────────────────────────────
// PC: the ROM's deleting dtor; the maps go as members.
// FS2 0x00188690
// callcheck: drawSysRemoveModel, operator_delete, ~VcmTable, ~ModelMotion, free, free, __destroy_arr, FUN_00188ea0, operator_delete, ~StageBoneNode, ~StageMatrixNode (the slot dtors with -1)

StageModel::~StageModel()
{
    if (data != nullptr) {
        drawSysRemoveModel(data);
        delete data;
        delete[] m_slots;
        delete vcm;
    }
    for (MotionMap &map : motions)
        for (MotionMap::Node *node = map.first; node != nullptr; node = node->next)
            delete node->value;
    if (pose != nullptr) {
        for (int i = 0; i < pose->boneCount; i++)
            m_poseNodes[i].~StageMatrixNode();
        gGlobalHeap.free(m_poseNodes);
        delete pose;
    }
    for (int i = 0; i < m_boneNodeCount; i++)
        m_boneNodes[i].~StageBoneNode();
    gGlobalHeap.free(m_boneNodes);
}

// ──────────────────────────────────────────────────────────────────────
// The ROM clears every member here (the default initializers). With a model:
// its textures by name, the VCM table, the draw system's list; the root bone
// follows the owner's matrix.
// FS2 0x00188b10
// callcheck: getTextureName, FUN_00189840, find, setTexture, operator_new, VcmTable, drawSysAddModel, heapAllocAligned, FUN_00296cc0, FUN_00296cc0, FUN_00296cc0, drawSysFindKind1Surface (the array's ctor is StageModelSlot's)

void StageModel::init(ModelInstance *instance, StringMap<Tim2Texture *> *textures,
                      const sceVu0FVECTOR *matrix)
{
    data = instance;
    if (data == nullptr)
        return;

    for (u_int i = 0; i < data->node->textureSlotCount; i++) {
        StringMap<Tim2Texture *>::Node *node = textures->find(data->getTextureName(i));
        if (node != nullptr)
            data->setTexture(i, node->value);
    }

    vcm = new VcmTable(data);
    drawSysAddModel(data);
    data->rootBone->matrix = matrix;
    data->_06 = 1;
    m_slots = new StageModelSlot[kModelSlots]();
    m_kind1Surface = drawSysFindKind1Surface(data);
}
