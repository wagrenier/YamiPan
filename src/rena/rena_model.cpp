/* ==========================================================================
 *  rena/rena_model.cpp
 *
 *  The RM2 model file (the structs are in game/gamehooks.h): checking it
 *  and relocating it in place, 0x2e2cf0..0x2e3a10 of the run
 *  unassigned@002e2970. On the disc every pointer is the address the tool
 *  gave it (the file's base at +0x30 plus an offset); relocate moves them
 *  to where the file was loaded. Every such field is an S2dRef.
 *
 *  The instances (ModelFile::createInstance 0x2e5210 and its 0x2e4800,
 *  0x2e3e10) are the next part of this file: still stubs; only the two
 *  texture calls of StageModel::init are written.
 * ======================================================================== */
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#include "game/gamehooks.h"
#include "rx/rxsys.h"

static const char kModelMagic[] = "RM2";    /* data 0x30fe00, gp-0x7b48 */
static const char kBaseMeshSuffix[] = "000"; /* data 0x30fe58 */
static const char kPremultipliedSuffix[] = "_s";
static const char kBaseMoved[] =
    "\x91\x53\x82\xc4\x82\xcc\x83\x82\x83\x66\x83\x8b\x82\xf0\x89\xf0\x95\xfa\x82\xb9\x82\xb8\x82\xc9";
    // 全てのモデルを解放せずに ... (data 0x30fe10; the tail is not kept)

constexpr int kModelMagicSize = 4;
constexpr int kNoBone = -1;
// An instance mesh's name: i, two letters, two digits, _, a number (not 100).
constexpr int kInstanceMeshMinName = 7;
constexpr int kInstanceMeshBase = 100;

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
}

// A packet's five or seven tags, and with skinning its two lists of
// pointers. FS2 0x002e3900 (ModelBatch::tags count 5), 0x002e3a10 (7).

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
    to.fix(node->_20);
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
