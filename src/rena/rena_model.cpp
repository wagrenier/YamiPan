/* ==========================================================================
 *  rena/rena_model.cpp
 *
 *  The RM2 model file and its instances (the structs are in
 *  game/gamehooks.h).
 *
 *  The file: checked and relocated in place. On the disc every pointer is
 *  the address the tool gave it (the file's base at +0x30 plus an offset);
 *  relocate moves them to where the file was loaded. Every such field is an
 *  S2dRef.
 *
 *  An instance: ModelFile::createInstance makes a ModelInstance of one node
 *  of the file: its bind matrices in the node's space, a ModelSurface per
 *  material the node uses, a ModelPartRec per mesh. All of it shares one
 *  block of the global heap; only the sizes differ from the ROM's (host
 *  pointers), the order is the same.
 *
 *  Not here: the draws between these functions (0x2e41a0, 0x2e43e0), which
 *  stay with the rena draw run.
 * ======================================================================== */
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <new>

#include "game/gamehooks.h"
#include "game/heap.h"
#include "game/vu0math.h"
#include "rena/rena_2d_texture_tim2.h"
#include "rena/rena_draw.h"
#include "yamipan/rendering/yamipan_drawmodel.h"
#include "rx/rxsys.h"

static const char kModelMagic[] = "RM2";    /* data 0x30fe00, gp-0x7b48 */
static const char kBaseMeshSuffix[] = "000"; /* data 0x30fe58 */
static const char kPremultipliedSuffix[] = "_s";
// rxSysPrintf's messages, printed only: UTF-8 here (Shift-JIS in the ROM).
static const char kBaseMoved[] =    /* data 0x30fe10 */
    "全てのモデルを解放せずに データアドレスの変更を行おうとしました。\n";
static const char kNotRmd[] =       /* data 0x30feb0 */
    "error: データポインタ先のデータは RMDデータではありません。\n";
static const char kNoSuchNumber[] = /* data 0x30fef0 */
    "error: %d番のモデルは 存在しません。\n";
static const char kNoSuchName[] =   /* data 0x30fe80 */
    "error: %sの名前のエキストラは存在しません。\n";
static const char kNoMemory[] = "not enough memory.\n"; /* data 0x30fe60 */

constexpr int kModelMagicSize = 4;
constexpr int kNoBone = -1;
// An instance mesh's name: i, two letters, two digits, _, a number (not 100).
constexpr int kInstanceMeshMinName = 7;
constexpr int kInstanceMeshBase = 100;

constexpr u_int kInstanceBlockAlign = 0x40;
constexpr u_int kInstanceItemAlign  = 0x10;   // PC: the ROM's items are all 0x10 multiples or follow none that need it

// Which ModelSurface::level a material's _14 falls in.
constexpr float kSurfaceLevelSteps[] = { 0.15f, 0.4f, 0.6f, 0.8f };
constexpr float kSurfaceMinScale = 0.2f;    // under this the level is 0

constexpr float kPi = 3.1415927f;
constexpr float kTwoPi = 6.2831855f;
constexpr int   kTexAnimTicks = 300;
constexpr int   kScrollMask = 0xfff;
constexpr int   kBlendFixOne = 0x80;

// A CLUT's entries by its Tim2ClutKind.
constexpr u_int kClutEntries4 = 0x10;
constexpr u_int kClutEntries8 = 0x100;
constexpr u_int kClutAlign    = 0x10;
constexpr u_int kOpaqueAlpha  = 0x80000000;

namespace {

// Where a file's pointers move: what the file calls its base to the file.
// layout: host -- the port's helper.
struct Relocation
{
    char *file;
    int   oldBase;

    template <class T>
    void fix(S2dRef<T> &ref) const
    {
        if (ref.rel != 0)
            ref.set(file + (ref.rel - oldBase));
    }
    template <class T>
    void fixList(S2dRef<S2dRef<T> > &list, int count) const
    {
        S2dRef<T> *item = list.get();
        for (int i = 0; i < count; i++)
            fix(item[i]);
    }
};

}  // namespace

// FS2 0x002e2cf0
// callcheck: memcmp

bool ModelFile::isValid()
{
    return memcmp(this, kModelMagic, kModelMagicSize) == 0;
}

// FS2 0x002e5200

int ModelFile::getNodeCount()
{
    return nodeCount;
}

// FS2 0x002e47d0

const char *ModelInstance::getTextureName(int slot) const
{
    return node->file.get()->textureNames.get()[node->textureSlots.get()[slot]].get();
}

// A texture whose name ends in _s gets its premultiplied palette first.
// FS2 0x002e4550
// callcheck: strlen

void ModelInstance::setTexture(int slot, Tim2Texture *texture)
{
    const char *name = getTextureName(slot);
    size_t length = strlen(name);
    if (length > 2 && name[length - 2] == kPremultipliedSuffix[0] && name[length - 1] == kPremultipliedSuffix[1])
        modelTexturePremultiplyPalette(texture);
    textures[slot] = texture;
    YamiPan_ModelForgetTextures();  // PC: a stage's pack reuses the last one's addresses
}

// A packet's five or seven tags, and with skinning its two lists of
// pointers. One function here, the ROM's two differ only in the tag count:
//   FS2 0x002e3900  5 tags (modelRelocateBatch)
//   FS2 0x002e3a10  7 tags (modelRelocateLongBatch)

static void relocateBatch(ModelBatch *batch, const ModelPrim *prim, const Relocation &to)
{
    int tagCount = prim->isLong ? ModelBatch::kLongTags : ModelBatch::kShortTags;
    for (int i = 0; i < tagCount; i++)
        to.fix(batch->tags[i].next);
    if (prim->skinned) {
        to.fixList(reinterpret_cast<S2dRef<S2dRef<void> > &>(batch->tags[tagCount - 2].next), prim->listACount);
        to.fixList(reinterpret_cast<S2dRef<S2dRef<void> > &>(batch->tags[tagCount - 1].next), prim->listBCount);
    }
}

// FS2 0x002e37d0
// callcheck: modelRelocateBatch, modelRelocateLongBatch

static void relocatePrim(ModelPrim *prim, const Relocation &to)
{
    to.fix(prim->batches);
    to.fix(prim->_0);
    int stride = (prim->isLong ? ModelBatch::kLongSize : ModelBatch::kShortSize)
                 + (prim->skinned ? 0 : ModelBatch::kPadSize);
    char *batch = (char *)prim->batches.get();
    for (int i = 0; i < prim->batchCount; i++, batch += stride)
        relocateBatch((ModelBatch *)batch, prim, to);
}

// FS2 0x002e3640
// callcheck: modelRelocatePrim

static void relocateMeshPart(ModelMeshPart *part, const Relocation &to, int listCount)
{
    if (part->materials.rel == 0)
        return;
    to.fix(part->materials);
    to.fix(part->lists);
    ModelMeshMaterial *materials = part->materials.get();
    for (int i = 0; i < part->materialCount; i++) {
        ModelMeshMaterial &material = materials[i];
        to.fix(material.prims);
        ModelPrim *prims = material.prims.get();
        for (int p = 0; p < material.primCount; p++)
            relocatePrim(&prims[p], to);
    }
    to.fixList(part->lists, listCount);
}

// A mesh that repeats one of the first node's: i + two letters + two digits
// + _ + a number other than 100.
// FS2 0x002e3540
// callcheck: strlen, strrchr, strtol

static bool isInstanceMesh(const char *name)
{
    if (strlen(name) < kInstanceMeshMinName)
        return false;
    if (name[0] != 'i')
        return false;
    if (!isalpha((u_char)name[1]) || !isalpha((u_char)name[2]))
        return false;
    if (!isdigit((u_char)name[3]) || !isdigit((u_char)name[4]))
        return false;
    if (name[5] != '_')
        return false;
    char *end;
    int number = (int)strtol(strrchr(name, '_') + 1, &end, 10);
    return *end == '\0' && number != kInstanceMeshBase;
}

// The first node's mesh of the same name with the number 000.
// FS2 0x002e3450
// callcheck: strlen, operator_new[], strcpy, strrchr, strcpy, strcmp, operator_delete[]

static ModelMesh *findBaseMesh(const ModelMesh *mesh, ModelFileNode *baseNode)
{
    const char *meshName = mesh->name.get();
    char *name = new char[strlen(meshName) + 4];
    strcpy(name, meshName);
    char *number = strrchr(name, '_');
    if (number == nullptr)
        return nullptr;     // as the ROM, name is not freed here
    strcpy(number + 1, kBaseMeshSuffix);

    ModelMesh *found = nullptr;
    ModelMesh *meshes = baseNode->meshes.get();
    for (int i = 0; i < baseNode->meshCount; i++) {
        if (strcmp(meshes[i].name.get(), name) == 0) {
            found = &meshes[i];
            break;
        }
    }
    delete[] name;
    return found;
}

// FS2 0x002e30e0
// callcheck: modelIsInstanceMesh, modelFindBaseMesh, strcmp

static void relocateNode(ModelFileNode *node, ModelFile *file, const Relocation &to)
{
    node->file.set(file);
    to.fix(node->bones);
    to.fix(node->textureSlots);
    to.fix(node->surfaceMaterials);
    to.fix(node->meshes);
    to.fix(node->name);
    to.fix(node->matrix);

    ModelNodeBone *bones = node->bones.get();
    for (int i = 0; i < node->boneCount; i++) {
        to.fix(bones[i].name);
        to.fix(bones[i].matrix);
    }
    ModelMesh *meshes = node->meshes.get();
    for (int i = 0; i < node->meshCount; i++) {
        to.fix(meshes[i].name);
        to.fix(meshes[i].boneMap);
        to.fix(meshes[i].parts);
    }

    // An instance mesh's bones the tool left -1 come from the first node's,
    // by name. The first such mesh ends the search.
    ModelFileNode *baseNode = file->nodes.get();
    if (node == baseNode)
        return;
    for (int i = 0; i < node->meshCount; i++) {
        ModelMesh *mesh = &meshes[i];
        const ModelMeshParts *parts = mesh->parts.get();
        if (parts == nullptr || !isInstanceMesh(mesh->name.get()))
            continue;
        const ModelMesh *base = findBaseMesh(mesh, baseNode);
        if (base == nullptr)
            continue;
        const ModelMeshParts *baseParts = base->parts.get();
        if (baseParts == nullptr || baseParts->boneCount != parts->boneCount)
            continue;
        for (int b = 0; b < parts->boneCount; b++) {
            short *map = mesh->boneMap.get();
            if (map[b] != kNoBone)
                continue;
            const char *boneName = baseNode->bones.get()[base->boneMap.get()[b]].name.get();
            for (int own = 0; own < node->boneCount; own++) {
                if (strcmp(bones[own].name.get(), boneName) == 0) {
                    map[b] = (short)own;
                    break;
                }
            }
        }
        return;
    }
}

// FS2 0x002e2d80
// callcheck: modelRelocateMeshPart, modelRelocateNode

void ModelFile::relocateFrom(int oldBase)
{
    Relocation to = { (char *)this, oldBase };
    base.set(this);
    to.fix(nodes);
    to.fix(_2c);
    to.fix(textureNames);
    to.fix(materials);
    to.fix(_40);
    to.fix(meshParts);
    to.fix(_44);

    to.fixList(textureNames, textureCount);

    ModelMeshParts *partsList = meshParts.get();
    for (int i = 0; i < meshPartCount; i++) {
        ModelMeshParts &entry = partsList[i];
        to.fix(entry.parts);
        ModelMeshPart *parts = entry.parts.get();
        for (int p = 0; p < entry.count; p++)
            relocateMeshPart(&parts[p], to, entry.boneCount);
        if (entry.material.rel != 0) {
            to.fix(entry.material);
            to.fix(entry.material->name);
        }
    }

    ModelFileNode *nodeList = nodes.get();
    for (int i = 0; i < nodeCount; i++)
        relocateNode(&nodeList[i], this, to);

    S2dRef<ModelMaterial> *materialList = materials.get();
    for (int i = 0; i < materialCount; i++) {
        to.fix(materialList[i]);
        ModelMaterial *material = materialList[i].get();
        to.fix(material->name);
        to.fix(material->texAnim);
        if (material->kind != ModelMaterial_Kind1) {
            to.fix(material->ext);
            if (material->ext.rel != 0)
                to.fix(material->ext->data);
        }
    }
}

// FS2 0x002e2d20
// callcheck: rxSysPrintf, ModelFile::relocateFrom

void ModelFile::relocate()
{
    if (base.get() == this)
        return;
    if (base.rel != 0 && userCount != 0)
        rxSysPrintf(kBaseMoved);
    relocateFrom(base.rel);
}

// ──────────────────────────────────────────────────────────────────────
// A surface: the slots of the material's textures, the start of its wave.

// The slot holding texture, -1 without one (a negative texture matches none).
static short findTextureSlot(const ModelFileNode *node, int texture)
{
    const u_short *slots = node->textureSlots.get();
    for (int i = 0; i < node->textureSlotCount; i++)
        if (texture == slots[i])
            return (short)i;
    return -1;
}

// What a new swing of a wave draws.
static float drawWaveAmplitude(const ModelTexAnim *anim)
{
    u_int roll = randNext() & 0xff;
    return anim->wave.amplitude * ((float)roll + 256.0f) / 512.0f;
}

static const ModelTexAnim *runningTexAnim(const ModelMaterial *material)
{
    if (material->kind != ModelMaterial_Textured)
        return nullptr;
    return material->texAnim.get();
}

// FS2 0x002e3e10
// callcheck: randNext

ModelSurface::ModelSurface(ModelMaterial *surfaceMaterial, const ModelFileNode *node)
    : _00(), waveValue(0.0f), _0c(0), wavePhase(0.0f), waveAmplitude(0.0f),
      material(surfaceMaterial), textureSlot(-1), extTextureSlot(-1), blend(1.0f),
      blendFix(0), frame0(0), frame1(0), scrollU(0), scrollV(0), texture2Slot(-1),
      level(0), levelScale(0.0f)
{
    if (material->kind == ModelMaterial_Textured) {
        textureSlot = findTextureSlot(node, material->texture);
        const ModelMaterialExt *ext = material->ext.get();
        if (ext != nullptr)
            extTextureSlot = findTextureSlot(node, ext->texture);
        texture2Slot = findTextureSlot(node, material->texture2);
    }

    const ModelTexAnim *anim = runningTexAnim(material);
    if (anim != nullptr && anim->kind == ModelTexAnim_Wave) {
        waveAmplitude = drawWaveAmplitude(anim);
        wavePhase = anim->wave.phase * kTwoPi / anim->wave.period - kPi;
        material->flags |= ModelMaterialFlag_Wave;
    }

    if (material->kind == ModelMaterial_Textured) {
        for (float step : kSurfaceLevelSteps)
            if (material->_14 >= step)
                level++;
        levelScale = material->_10;
        if (levelScale < kSurfaceMinScale)
            level = 0;
    }
}

// FS2 0x002e3d90

void ModelSurface::setFrames(float amount, int first, int second)
{
    if (second == 0) {
        frame0 = 0;
        frame1 = first;
        blend = 1.0f - amount;
        blendFix = (int)(amount * (float)kBlendFixOne);
    } else {
        frame0 = first;
        frame1 = second;
        blend = amount;
        blendFix = kBlendFixOne - (int)(amount * (float)kBlendFixOne);
    }
}

// FS2 0x002e3bf0
// callcheck: sinf, randNext

void renaStepTexAnim(ModelSurface *surface, int ticks)
{
    const ModelTexAnim *anim = runningTexAnim(surface->material);
    if (anim == nullptr)
        return;
    if (anim->kind == ModelTexAnim_Scroll) {
        surface->scrollU = (surface->scrollU + anim->scroll.speedU * ticks / kTexAnimTicks) & kScrollMask;
        surface->scrollV = (surface->scrollV + anim->scroll.speedV * ticks / kTexAnimTicks) & kScrollMask;
    }
    if (anim->kind == ModelTexAnim_Wave) {
        surface->waveValue = surface->waveAmplitude * sinf(surface->wavePhase);
        surface->wavePhase += (float)ticks * kTwoPi / (anim->wave.period * (float)kTexAnimTicks);
        if (surface->wavePhase > kPi) {
            surface->wavePhase -= kTwoPi;
            surface->waveAmplitude = drawWaveAmplitude(anim);
        }
    }
}

// FS2 0x002e5190

int ModelFileNode::countTexAnims() const
{
    S2dRef<ModelMaterial> *materials = file->materials.get();
    const u_short *indices = surfaceMaterials.get();
    int count = 0;
    for (int i = 0; i < surfaceCount; i++) {
        const ModelTexAnim *anim = runningTexAnim(materials[indices[i]].get());
        if (anim != nullptr && anim->kind != ModelTexAnim_None)
            count++;
    }
    return count;
}

// ──────────────────────────────────────────────────────────────────────
// The mesh records. The EE's virtuals are the trivial ones: slots 0 and 1
// below, slot 2 the empty deleting dtor (0x31a700 is its base's), and the
// placement new the ROM calls; neither has a body here:
//   FS2 0x002e3b70  ModelPartRec::~ModelPartRec
//   FS2 0x002e5180  modelPlacementNew

// FS2 0x002e3ba0

ModelPartRec::ModelPartRec(const ModelMesh *mesh, u_short nextRec)
    : firstChild(mesh->firstChild), next(nextRec), childCount(mesh->childCount),
      vcmAlpha(0), vcmFadeBits(0), vcmOffBits(0), drawMode(0), flagE(0),
      hasMesh(mesh->parts.rel != 0)
{
}

// FS2 0x002e3b50

void ModelPartRec::setFlagE(u_char value)
{
    flagE = value;
}

// FS2 0x002e3b60

u_char ModelPartRec::getFlagE() const
{
    return flagE;
}

// The ROM unrolls one level of this recursion.
// FS2 0x002e4f50
// callcheck: ModelPartRec, modelPlacementNew

void ModelInstance::buildPartRecs(int index, u_short next)
{
    const ModelMesh *mesh = &node->meshes.get()[index];
    new (&partRecs[index]) ModelPartRec(mesh, next);
    for (int i = 0; i < mesh->childCount; i++) {
        int child = mesh->firstChild + i;
        buildPartRecs(child, i == mesh->childCount - 1 ? next : (u_short)(child + 1));
    }
}

// FS2 0x002e4100

ModelPartRec *ModelInstance::findGroup(const ModelPartRec *rec) const
{
    int index = (int)(rec - partRecs);
    for (int i = 0; i < node->meshCount; i++) {
        ModelPartRec *group = &partRecs[i];
        if (group->childCount != 0 && group->firstChild <= index
            && index < group->firstChild + group->childCount)
            return group;
    }
    return nullptr;
}

// The record of the first mesh whose name starts with length characters of
// name; the fourth argument is not read.
// FS2 0x002e4610
// callcheck: strncmp

ModelPartRec *modelFindPart(ModelInstance *model, const char *name, size_t length, int)
{
    const ModelMesh *meshes = model->node->meshes.get();
    for (int i = 0; i < model->node->meshCount; i++)
        if (strncmp(name, meshes[i].name.get(), length) == 0)
            return &model->partRecs[i];
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The instance.

// FS2 0x002e9790

ModelBone::ModelBone(const ModelNodeBone *rec)
    : node(rec), matrix(rec->matrix)
{
}

// FS2 0x002e40e0

void ModelInstance::setDistant(u_char value)
{
    distant = value;
}

// FS2 0x002e40f0

u_char ModelInstance::getDistant() const
{
    return distant;
}

// FS2 0x002e44e0

void ModelInstance::setAmbient(const sceVu0FVECTOR color)
{
    memcpy(lightColors[0], color, sizeof(sceVu0FVECTOR));
    drawMode |= kModelAmbient;
}

// FS2 0x002e4500

void ModelInstance::setLight(u_int index, const sceVu0FVECTOR direction, const sceVu0FVECTOR color)
{
    lightDirs[0][index] = direction[0];
    lightDirs[1][index] = direction[1];
    lightDirs[2][index] = direction[2];
    memcpy(lightColors[index + 1], color, sizeof(sceVu0FVECTOR));
    drawMode |= (u_short)(1 << index);
}

// What count T take of the block.
template <class T>
static u_int blockBytes(u_int count)
{
    return (count * (u_int)sizeof(T) + kInstanceItemAlign - 1) & ~(kInstanceItemAlign - 1);
}

template <class T>
T *ModelInstance::take(u_int count)
{
    if (count == 0)
        return nullptr;
    T *items = reinterpret_cast<T *>(block + blockUsed);
    blockUsed += blockBytes<T>(count);
    return items;
}

// FS2 0x002e4800
// callcheck: countTexAnims, alloc, rxSysPrintf, ModelSurface, ModelBone, ModelBone, buildPartRecs

void ModelInstance::init(ModelFileNode *fileNode)
{
    shown = 1;
    flags = 0;
    distant = 0;
    ModelFile *file = fileNode->file;
    file->userCount++;
    node = fileNode;
    boneCount = node->boneCount;
    texAnimCount = node->countTexAnims();

    blockSize = blockBytes<sceVu0FMATRIX>(1 + boneCount * 2)
              + blockBytes<ModelSurface>(node->surfaceCount)
              + blockBytes<ModelBone>(1)
              + blockBytes<ModelBone>(boneCount)
              + blockBytes<const sceVu0FVECTOR *>(boneCount)
              + blockBytes<ModelPartRec>(node->meshCount)
              + blockBytes<Tim2Texture *>(node->textureSlotCount)
              + blockBytes<ModelSurface *>(file->materialCount)
              + blockBytes<ModelSurface *>(texAnimCount);
    block = (char *)gGlobalHeap.alloc(blockSize, kInstanceBlockAlign);
    if (block == nullptr) {
        rxSysPrintf(kNoMemory);
        abort();        // the ROM's break
    }

    sceVu0FMATRIX *inverseNode = take<sceVu0FMATRIX>(1);
    rootMatrix = *inverseNode;
    inverseBinds = take<sceVu0FMATRIX>(boneCount);
    boneBinds = take<sceVu0FMATRIX>(boneCount);

    surfaces = take<ModelSurface>(node->surfaceCount);
    S2dRef<ModelMaterial> *materials = file->materials.get();
    const u_short *surfaceMaterials = node->surfaceMaterials.get();
    for (int i = 0; i < node->surfaceCount; i++)
        new (&surfaces[i]) ModelSurface(materials[surfaceMaterials[i]].get(), node);

    rootBone = take<ModelBone>(1);
    bones = take<ModelBone>(boneCount);
    bindMatrices = take<const sceVu0FVECTOR *>(boneCount);
    new (rootBone) ModelBone(reinterpret_cast<const ModelNodeBone *>(node));

    invertAffineMatrix(*inverseNode, node->matrix);
    const ModelNodeBone *nodeBones = node->bones.get();
    for (u_int i = 0; i < boneCount; i++) {
        new (&bones[i]) ModelBone(&nodeBones[i]);
        const sceVu0FVECTOR *matrix = nodeBones[i].matrix;
        multiplyMatrix(boneBinds[i], rootMatrix, matrix);
        invertAffineMatrix(inverseBinds[i], boneBinds[i]);
        bindMatrices[i] = matrix;
    }

    partRecs = take<ModelPartRec>(node->meshCount);
    // PC: a node without meshes; the ROM reads past its list.
    if (node->meshCount != 0)
        buildPartRecs(0, 0);

    textures = take<Tim2Texture *>(node->textureSlotCount);
    for (int i = 0; i < node->textureSlotCount; i++)
        textures[i] = nullptr;

    surfaceOfMaterial = take<ModelSurface *>(file->materialCount);
    for (int i = 0; i < file->materialCount; i++)
        surfaceOfMaterial[i] = nullptr;
    for (int i = 0; i < node->surfaceCount; i++)
        surfaceOfMaterial[surfaceMaterials[i]] = &surfaces[i];

    texAnims = take<ModelSurface *>(texAnimCount);
    int animCount = 0;
    for (int i = 0; i < node->surfaceCount; i++) {
        const ModelTexAnim *anim = runningTexAnim(surfaces[i].material);
        if (anim != nullptr && anim->kind != ModelTexAnim_None)
            texAnims[animCount++] = &surfaces[i];
    }
}

// The ROM also calls each ModelPartRec's empty deleting dtor; delete of the
// object itself is the caller's here (slot 0 takes a flag on the EE).
// FS2 0x002e46d0
// callcheck: free, operator_delete

ModelInstance::~ModelInstance()
{
    node->file->userCount--;
    gGlobalHeap.free(block);
}

// createInstance inlines this and the next one.

ModelFileNode *ModelFile::findNode(u_int index)
{
    if (index < nodeCount)
        return &nodes.get()[index];
    rxSysPrintf(kNoSuchNumber, index);
    return nullptr;
}


ModelFileNode *ModelFile::findNode(const char *name)
{
    ModelFileNode *list = nodes.get();
    for (int i = 0; i < nodeCount; i++)
        if (strcmp(list[i].name.get(), name) == 0)
            return &list[i];
    rxSysPrintf(kNoSuchName, name);
    return nullptr;
}

// The ROM has this twice, by index and by name, around the inlined ctor.
// FS2 0x002e5210
// callcheck: heapAllocAligned, isValid, rxSysPrintf, relocate, strcmp, rxSysPrintf, init, heapAllocAligned, isValid, rxSysPrintf, relocate, rxSysPrintf, init

ModelInstance *ModelFile::createInstance(u_int index, const char *name)
{
    ModelFileNode *node = nullptr;
    if (!isValid()) {
        rxSysPrintf(kNotRmd);
    } else {
        relocate();
        node = name != nullptr ? findNode(name) : findNode(index);
    }
    // PC: the ROM goes on through the null node.
    if (node == nullptr)
        return nullptr;
    ModelInstance *instance = new ModelInstance;
    instance->init(node);
    return instance;
}

// A texture named .._s: its palette times its alpha, opaque, kept as the
// second CLUT.
// FS2 0x002e1700
// callcheck: alloc

void modelTexturePremultiplyPalette(Tim2Texture *texture)
{
    if (texture->m_altClut != nullptr || texture->m_clutKind == Tim2Clut_None)
        return;
    u_int count = texture->m_clutKind == Tim2Clut_Index4 ? kClutEntries4 : kClutEntries8;
    texture->m_altClut = gGlobalHeap.alloc(count * sizeof(u_int), kClutAlign);
    const u_int *from = (const u_int *)texture->m_clut;
    u_int *to = (u_int *)texture->m_altClut;
    for (u_int i = 0; i < count; i++) {
        u_int color = from[i];
        u_int alpha = color >> 24;
        u_int red   = ( color        & 0xff) * alpha >> 7;
        u_int green = ((color >> 8)  & 0xff) * alpha >> 7;
        u_int blue  = ((color >> 16) & 0xff) * alpha >> 7;
        to[i] = (blue << 16) | (green << 8) | red | kOpaqueAlpha;
    }
}
