/* ==========================================================================
 *  rena/rena_draw.cpp
 *
 *  rena's frame on the EE: the calls drawSysRender makes and the walk over a
 *  record's models (the file name is the port's; the run 0x2e5450..0x2e9220
 *  is still one piece in the ROM's order, rena_draw.h has the calls).
 *
 *  The ROM writes a VU1 frame here: the program and its constants, then for
 *  each mesh a matrix and a DMA call into the model file. The host keeps the
 *  EE's decisions -- which models, parts and materials, in which order, with
 *  which matrix and lights -- and hands each batch to the draw module
 *  (yamipan_drawmodel.h) instead of chaining it; what VU1 does is the
 *  module's. Left out, with no host use: renaBeginFrame's upload, the fog and
 *  clip constants (0x2e5a00, 0x2e5da0, 0x2e5bb0) as packets, the GS set-up
 *  0x2e7c30 and 0x2e7e90 (ALPHA_1 0x2a / 0x44, TEST_1 0x5050b / 0x50000,
 *  CLAMP_1 repeat, TEX1_1 linear: the module's opaque and blended draws).
 *
 *  Not written yet (open-items.md "Next up"): a ModelMaterial_Kind1's frames,
 *  the ghost's pass 0x2e7960, the fade 0x2e9220 /
 *  0x2e96a0, mips.
 * ======================================================================== */
#include "rena/rena_draw.h"

#include <algorithm>
#include <math.h>
#include <memory>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "game/drawsys.h"
#include "game/gamehooks.h"
#include "game/scenerender.h"
#include "game/vu0math.h"
#include "yamipan/rendering/yamipan_drawmodel.h"

namespace {

// What renaSetLights keeps: the record's, for every model that has none of
// its own (bss 0x5a5240.., 0x5a52c0). layout: host.
struct RecordLights
{
    sceVu0FVECTOR ambient;
    sceVu0FVECTOR colors[DrawSysLights::kLightCount];
    sceVu0FVECTOR directions[DrawSysLights::kLightCount];
};
RecordLights sLights;

// The frame's spot: the record and its picture (renaSetSpotRecord, renaSetSpotlight).
const RenaSpotLight *sSpot;
YamiPanModelShadows sShadows;   // what the shadow set-up gave VU1: count, TEX0, addresses 64.. (the pass takes a copy)
const Tim2Texture   *sSpotTexture;

// What a skinned mesh's parts share: draw state +0x21c, +0x1f8. layout: host.
struct Skin
{
    const ModelMesh *mesh;
    const float     (*meshBind)[4]; // boneBinds[mesh.bone] or unit
};

// One material of a blended model part, waiting for its place (0x20 bytes in
// the table at 0x59d240, sorted through the keys at 0x59b240). layout: host.
struct Deferred
{
    ModelInstance           *model;
    const ModelMeshMaterial *material;
    const ModelPartRec      *rec;
    const float             (*matrix)[4];
    const Skin              *skin;
    int                      key;
    bool                     unlit;
};
std::vector<Deferred> sDeferred;

constexpr int   kNoSurfaceUvSet = 1;    // what 0x2e6b70 hands on without a surface
constexpr float kFrameHalf = 256.0f;
constexpr float kCullWidthMargin = 3.0f; // PC: every aspect ratio stays inside the box test
constexpr int   kMaxPalette = 256;      // ModelPrim::paletteCount is a byte
constexpr int   kBoxVectors = 2;        // ModelMeshPart::lists[0] per bone
constexpr float kSkinBoxLimit = 1e30f;  // the ROM's 0x310610 / 0x310620 start the min and the max

// A skinned part's matrix: its vertices are in the world after the palette.
sceVu0FMATRIX sUnitMatrix = { { 1, 0, 0, 0 }, { 0, 1, 0, 0 }, { 0, 0, 1, 0 }, { 0, 0, 0, 1 } };
// The Skins of the frame's skinned meshes that wait in sDeferred.
std::vector<std::unique_ptr<Skin> > sDeferredSkins;

void copyVector3(float *out, const float *in)
{
    out[0] = in[0];
    out[1] = in[1];
    out[2] = in[2];
}

// The point through a matrix (sceVu0 layout).
void transformPoint(float *out, const float (*matrix)[4], const float *point)
{
    for (int i = 0; i < 4; i++) {
        out[i] = matrix[0][i] * point[0] + matrix[1][i] * point[1]
               + matrix[2][i] * point[2] + matrix[3][i] * point[3];
    }
}

// The part's box: its centre, then its half size.
const float *partCenter(const ModelMeshPart *part)
{
    return (const float *)part->_00;
}

// The depth 0x2e6590 sorts a blended part by: the screen z of its centre,
// before the divide.
float pointScreenZ(const float *center, const float (*world)[4])
{
    const float point[4] = { center[0], center[1], center[2], 1.0f };
    float placed[4];
    float screen[4];
    transformPoint(placed, world, point);
    transformPoint(screen, (const float (*)[4])gDrawSysState.viewScreen, placed);
    return screen[2];
}

// Whether a box lies wholly outside the frame: what 0x2e2a60 answers with 2.
bool boxOutside(const float *center, const float *half, const float (*world)[4])
{
    u_int outside = ~0u;
    for (int corner = 0; corner < 8; corner++) {
        const float point[4] = {
            center[0] + ((corner & 1) ? half[0] : -half[0]),
            center[1] + ((corner & 2) ? half[1] : -half[1]),
            center[2] + ((corner & 4) ? half[2] : -half[2]),
            1.0f,
        };
        float placed[4];
        float screen[4];
        transformPoint(placed, world, point);
        transformPoint(screen, (const float (*)[4])gDrawSysState.viewScreen, placed);

        const float w = screen[3];
        const float dx = screen[0] - gDrawSysState.centerX * w;
        const float dy = screen[1] - gDrawSysState.centerY * w;
        const float rangeX = kFrameHalf * kCullWidthMargin * w;
        const float rangeY = kFrameHalf * w;
        u_int bits = 0;
        if (w <= 0.0f)
            bits |= 0x01;
        if (dx < -rangeX)
            bits |= 0x02;
        if (dx > rangeX)
            bits |= 0x04;
        if (dy < -rangeY)
            bits |= 0x08;
        if (dy > rangeY)
            bits |= 0x10;
        outside &= bits;
        if (outside == 0)
            return false;
    }
    return true;
}

bool partOutside(const ModelMeshPart *part, const float (*world)[4])
{
    const float *center = partCenter(part);
    return boxOutside(center, center + 4, world);
}

// A bone's matrix for a skinned vertex: M, then the inverse of the bind, then
// the bone as it stands. Written twice in the ROM, both inline: 0x2e68a0 and
// 0x2e9480.
void skinMatrix(sceVu0FMATRIX out, const ModelInstance *model, int bone, const float (*meshBind)[4])
{
    sceVu0FMATRIX fromMesh;
    multiplyMatrix(fromMesh, (const sceVu0FVECTOR *)model->inverseBinds[bone], (const sceVu0FVECTOR *)meshBind);
    multiplyMatrix(out, (const sceVu0FVECTOR *)model->bones[bone].matrix, (const sceVu0FVECTOR *)fromMesh);
}

// The box of a skinned part in the world: each of its bones' boxes through
// that bone's matrix, all corners joined. Then the same test as a rigid part,
// around the box's own centre. The ROM stores the centre only when 0x2e2a60
// answers 1, so a part wholly in frame sorts by the last one that did; the
// host's test has no such answer and always stores it.
// FS2 0x002e68a0
// callcheck: boxClipTest (rena_camera.cpp: it needs the planes 0x2e5bb0 builds, not built on the host; boxOutside stands in)

bool skinnedPartOutside(float *center, const ModelInstance *model, const Skin &skin, const ModelMeshPart *part,
                        int boneCount)
{
    float low[3] = { kSkinBoxLimit, kSkinBoxLimit, kSkinBoxLimit };
    float high[3] = { -kSkinBoxLimit, -kSkinBoxLimit, -kSkinBoxLimit };
    const float *box = part->lists.get()[0].get();
    const short *boneMap = skin.mesh->boneMap.get();
    for (int b = 0; b < boneCount; b++, box += kBoxVectors * 4) {
        sceVu0FMATRIX matrix;
        skinMatrix(matrix, model, boneMap[b], skin.meshBind);
        const float *half = box + 4;
        for (int corner = 0; corner < 8; corner++) {
            const float point[4] = {
                box[0] + ((corner & 1) ? half[0] : -half[0]),
                box[1] + ((corner & 2) ? half[1] : -half[1]),
                box[2] + ((corner & 4) ? half[2] : -half[2]),
                1.0f,
            };
            float placed[4];
            transformPoint(placed, (const float (*)[4])matrix, point);
            for (int axis = 0; axis < 3; axis++) {
                low[axis] = std::min(low[axis], placed[axis]);
                high[axis] = std::max(high[axis], placed[axis]);
            }
        }
    }
    float half[3];
    for (int axis = 0; axis < 3; axis++) {
        center[axis] = (high[axis] + low[axis]) * 0.5f;
        half[axis] = (high[axis] - low[axis]) * 0.5f;
    }
    return boxOutside(center, half, (const float (*)[4])sUnitMatrix);
}

// The prim's palette: what the ROM uploads to VU address 228. Returns its
// first float, null when the prim has none.
// FS2 0x002e9480

const float *buildPalette(sceVu0FMATRIX *palette, const ModelInstance *model, const ModelPrim &prim, const Skin &skin)
{
    if (prim.paletteCount == 0)
        return nullptr;
    const u_short *bones = prim.paletteBones.get();
    const short *boneMap = skin.mesh->boneMap.get();
    for (int i = 0; i < prim.paletteCount; i++)
        skinMatrix(palette[i], model, boneMap[bones[i]], skin.meshBind);
    return &palette[0][0][0];
}

// The lights of one model: each its own where drawMode says so (the head of
// 0x2e62f0).
void setModelLights(sceVif1Packet *packet, const ModelInstance *model)
{
    YamiPanModelLights lights;
    copyVector3(lights.ambient, (model->drawMode & kModelAmbient) ? model->lightColors[0] : sLights.ambient);
    for (int i = 0; i < DrawSysLights::kLightCount; i++) {
        if (model->drawMode & (1 << i)) {
            copyVector3(lights.colors[i], model->lightColors[i + 1]);
            lights.directions[i][0] = model->lightDirs[0][i];
            lights.directions[i][1] = model->lightDirs[1][i];
            lights.directions[i][2] = model->lightDirs[2][i];
        } else {
            copyVector3(lights.colors[i], sLights.colors[i]);
            copyVector3(lights.directions[i], sLights.directions[i]);
        }
    }
    lights.takesPoint = (model->flags & ModelFlag_TakesPoint) != 0;
    lights.takesSpot = (model->flags & ModelFlag_TakesSpot) != 0 && model->spot != nullptr && model->spot->isActive();
    lights.takesShadow = (model->flags & ModelFlag_TakesShadow) != 0;
    lights.cullBack = (model->flags & ModelFlag_TagC) != 0;
    YamiPan_ModelSetLights(packet, &lights);
}

// The GS and VU1 state of a surface: FS2 0x002e7240, as far as a host draw
// callcheck: FUN_002e0850, FUN_002e09b0, FUN_002e0a10, FUN_002e16c0, FUN_002e16d0, FUN_002e6ef0, FUN_002e7070, logf (the texture upload, mips and GS registers: the module binds the texture itself)
// has a use for it.
void setSurface(sceVif1Packet *packet, const ModelInstance *model, const ModelSurface *surface, bool blended,
                bool unlit)
{
    YamiPanModelMaterial material = {};
    material.blended = blended;
    material.unlit = unlit;
    if (surface != nullptr) {
        if (surface->textureSlot >= 0)
            material.texture = model->textures[surface->textureSlot];
        material.scrollU = surface->scrollU;
        material.scrollV = surface->scrollV;
        material.pointFlat = (surface->material->flags & ModelMaterialFlag_PointFlat) != 0;
        material.noSpot = (surface->material->flags & ModelMaterialFlag_Foliage) != 0;
        material.noShadow = (surface->material->flags & (ModelMaterialFlag_Blended | ModelMaterialFlag_NoShadow)) != 0;
        if (surface->texture2Slot >= 0)
            material.reflection = model->textures[surface->texture2Slot];
        material.reflectionScale = surface->levelScale;
    }
    material.noDepthWrite = (model->flags & ModelFlag_Ghost) != 0;
    YamiPan_ModelSetMaterial(packet, &material);
}

// A list's entry, or the first one where the ROM would send none.
const void *listEntry(const ModelTag &tag, int index)
{
    S2dRef<S2dRef<void> > &ref = reinterpret_cast<S2dRef<S2dRef<void> > &>(const_cast<ModelTag &>(tag).next);
    S2dRef<void> *list = ref.get();
    const void *entry = list[index].get();
    return entry != nullptr ? entry : list[0].get();
}

// The batches of a material's prims. The ROM's 0x2e83b0 chains each one (or,
// with the lists, a copy of its tags: 0x2e95d0, with a palette 0x2e9370);
// here its data goes straight to the module.
// FS2 0x002e83b0
// callcheck: chainFadeBatches, chainListBatches, chainPaletteFadeBatches, chainPaletteListBatches (the copies of the tags: the module takes the data in place; the fades are not written)

void drawPrims(sceVif1Packet *packet, const ModelInstance *model, const ModelMeshMaterial *material,
               const float (*world)[4], const Skin *skin, int colorSet, int uvSet)
{
    sceVu0FMATRIX palette[kMaxPalette];
    const ModelPrim *prims = material->prims.get();
    for (int p = 0; p < material->primCount; p++) {
        const ModelPrim &prim = prims[p];
        const int tagCount = prim.paletteCount != 0 ? ModelBatch::kLongTags : ModelBatch::kShortTags;
        const int stride = (prim.paletteCount != 0 ? ModelBatch::kLongSize : ModelBatch::kShortSize)
                         + (prim.listFlags ? 0 : ModelBatch::kPadSize);
        const float *matrices = skin != nullptr ? buildPalette(palette, model, prim, *skin) : nullptr;
        const char *record = (const char *)prim.batches.get();
        for (int b = 0; b < prim.batchCount; b++, record += stride) {
            const ModelTag *tags = ((const ModelBatch *)record)->tags;
            const ModelPrim::Header *header = (const ModelPrim::Header *)tags[0].next.get();

            YamiPanModelBatch batch;
            batch.positions = (const float *)tags[1].next.get();
            batch.normals = (const float *)tags[2].next.get();
            if (prim.listFlags) {
                batch.colors = (const uint8_t *)listEntry(tags[tagCount - 2], colorSet);
                batch.uvs = (const int16_t *)listEntry(tags[tagCount - 1], uvSet);
            } else {
                batch.colors = (const uint8_t *)tags[tagCount - 2].next.get();
                batch.uvs = (const int16_t *)tags[tagCount - 1].next.get();
            }
            batch.count = header->vertexCount;
            batch.strip = (header->flags & ModelPrim::Header::Strip) != 0;
            batch.palette = matrices;
            batch.boneIds = nullptr;
            batch.weights = nullptr;
            batch.bonesPerVertex = 0;
            batch.firstBoneOnly = 0;
            batch.owner = model;    // PC: the renderer pairs frames by it (smooth motion)
            if (matrices != nullptr) {
                batch.boneIds = (const int8_t *)tags[3].next.get();
                batch.weights = (const float *)tags[4].next.get();
                batch.bonesPerVertex = header->weightsLess1 + 1;
                batch.firstBoneOnly = (model->flags & ModelFlag_Distant) != 0;
            }
            YamiPan_ModelDrawBatch(packet, world, &batch);
        }
    }
}

// One material. The ROM's second half, a ModelMaterial_Kind1's two frames
// over its base, is not written yet (file header): the base is drawn.
// FS2 0x002e6b70
// callcheck: FUN_002e7e90 (packets and VU1 state: the draw module's on the host)

void drawMaterial(sceVif1Packet *packet, ModelInstance *model, const ModelMeshMaterial *material,
                  const ModelPartRec *rec, const float (*world)[4], const Skin *skin, bool blended, bool unlit)
{
    const ModelSurface *surface = material->material < 0 ? nullptr : model->surfaceOfMaterial[material->material];
    if (surface != nullptr && surface->material->kind != ModelMaterial_Textured) {
        const short base = surface->material->uvSet;
        if (base < 0)
            return;
        surface = model->surfaceOfMaterial[base];
    }
    setSurface(packet, model, surface, blended, unlit);
    drawPrims(packet, model, material, world, skin, rec->vcmFadeBits,
              surface != nullptr ? surface->material->uvSet : kNoSurfaceUvSet);
}

// One part: its opaque materials now, the others into the table.
// FS2 0x002e6590

void drawPart(sceVif1Packet *packet, ModelInstance *model, const ModelPartRec *rec, const ModelMeshPart *part,
              const float *center, const float (*world)[4], const Skin *skin, bool ghost, bool unlit)
{
    const ModelMeshMaterial *materials = part->materials.get();
    for (int i = 0; i < part->materialCount; i++) {
        const ModelMeshMaterial *material = &materials[i];
        const ModelSurface *surface = material->material < 0 ? nullptr : model->surfaceOfMaterial[material->material];
        const bool blended = surface != nullptr && (surface->material->flags & ModelMaterialFlag_Blended) != 0;
        if (!blended && !ghost) {
            drawMaterial(packet, model, material, rec, world, skin, false, unlit);
            continue;
        }
        Deferred entry = { model, material, rec, world, skin, 0, unlit };
        if (skin != nullptr)
            entry.skin = sDeferredSkins.emplace_back(std::make_unique<Skin>(*skin)).get();
        entry.key = (int)pointScreenZ(center, world) + (int)sDeferred.size();
        sDeferred.push_back(entry);
    }
}

// A skinned mesh: each part with its own box. The ROM asks for the unit matrix
// (0x2e7610) before each part; the batches take it from drawPrims.
// FS2 0x002e43e0
// callcheck: uploadMeshMatrix (packets and VU1 state: the draw module's on the host)

void drawSkinnedMesh(sceVif1Packet *packet, ModelInstance *model, const ModelPartRec *rec, const ModelMesh *mesh,
                     const ModelMeshParts *parts, bool ghost, bool unlit)
{
    Skin skin = { mesh, (const float (*)[4])sUnitMatrix };
    if (mesh->bone >= 0)
        skin.meshBind = (const float (*)[4])model->boneBinds[mesh->bone];
    const ModelMeshPart *list = parts->parts.get();
    for (int p = 0; p < parts->count; p++) {
        if (list[p].materials.rel == 0)
            continue;
        float center[3];
        if (skinnedPartOutside(center, model, skin, &list[p], parts->boneCount))
            continue;
        drawPart(packet, model, rec, &list[p], center, (const float (*)[4])sUnitMatrix,
                 list[p].paletted != 0 ? &skin : nullptr, ghost, unlit);
    }
}

// A model's mesh tree, skipping what the switches of its records hide.
// FS2 0x002e41a0
// callcheck: rigidPartClip, uploadMeshMatrix (the matrix is handed to each batch; the prefetches and the rigid test are partOutside)

void drawTree(sceVif1Packet *packet, ModelInstance *model, bool ghost)
{
    const ModelFileNode *node = model->node;
    if (node->meshCount == 0)
        return;
    const ModelMesh *meshes = node->meshes.get();
    u_int index = 0;
    do {
        const ModelPartRec *rec = &model->partRecs[index];
        if (rec->clipped != 0 || (rec->drawMode & 2) != 0) {
            index = rec->next;
            continue;
        }
        if (rec->hasMesh && rec->drawMode == 0) {
            const ModelMesh *mesh = &meshes[index];
            const ModelMeshParts *parts = mesh->parts.get();
            if (parts->boneCount != 0) {
                drawSkinnedMesh(packet, model, rec, mesh, parts, ghost, (mesh->vuFlags & ModelMeshFlag_Unlit) != 0);
            } else {
                const float (*world)[4] = mesh->bone < 0 ? (const float (*)[4])model->rootBone->matrix
                                                         : (const float (*)[4])model->bones[mesh->bone].matrix;
                const ModelMeshPart *list = parts->parts.get();
                const bool unlit = (mesh->vuFlags & ModelMeshFlag_Unlit) != 0;
                for (int p = 0; p < parts->count; p++) {
                    if (list[p].materials.rel != 0 && !partOutside(&list[p], world))
                        drawPart(packet, model, rec, &list[p], partCenter(&list[p]), world, nullptr, ghost, unlit);
                }
            }
        }
        index = rec->firstChild != 0 ? rec->firstChild : rec->next;
    } while (index != 0);
}

// The record's models of a group, each with its lights.
// FS2 0x002e62f0
// callcheck: FUN_002e7820 (packets and VU1 state: the draw module's on the host)

void drawModels(sceVif1Packet *packet, DrawSysRecord *record, int mask)
{
    for (ModelInstance *model : record->models) {
        if ((mask & model->drawGroup) == 0 || model->clipped != 0 || model->shown == 0)
            continue;
        setModelLights(packet, model);
        drawTree(packet, model, (model->flags & ModelFlag_Ghost) != 0);
    }
}

// One entry of the table.
// FS2 0x002e5860
// callcheck: FUN_002e7960, FUN_002e7e90, uploadMeshMatrix (the ghost pass is not written; the matrix and blend state are the module draw)

void drawDeferred(sceVif1Packet *packet, const Deferred &entry)
{
    setModelLights(packet, entry.model);
    drawMaterial(packet, entry.model, entry.material, entry.rec, entry.matrix, entry.skin, true, entry.unlit);
}

} // namespace

// The VU1 program and the frame's GS state: nothing for the host.
// FS2 0x002e86b0
// callcheck: FUN_0010ad90, FUN_0010ae28, FUN_0010b028, FUN_0010b6e0, getDrawTarget, sceVif1PkAddCode, sceVif1PkCloseDmaTag, sceVif1PkCloseGifTag, sceVif1PkCnt, sceVif1PkOpenDirectCode, sceVif1PkOpenGifTag (packets and VU1 state: the draw module's on the host)

void renaBeginFrame(sceVif1Packet *packet)
{
    (void)packet;
    sShadows.texture = nullptr; // PC: the boss stage sets none, its target is not written yet
}

// FS2 0x002e87f0
// callcheck: FUN_0010aef0, FUN_0010af80, FUN_0010b5f0, FUN_002e1b10, sceVif1PkCloseDmaTag, sceVif1PkCnt, upload (the GS state and the upload: the module binds the picture itself)

void renaSetSpotlight(sceVif1Packet *packet, Tim2Texture *texture, int block)
{
    (void)packet;
    (void)block;
    sSpotTexture = texture;
}

// FS2 0x002d5710
// callcheck: FUN_0010aef0, FUN_0010af80, FUN_0010b208, sceVif1PkCloseDmaTag, sceVif1PkCnt (packets: fillSpotLight reads the record in each pass)

void renaSetSpotRecord(const RenaSpotLight *spot)
{
    sSpot = spot;
}

// FS2 0x002e82a0
// callcheck: FUN_002e1b10, upload (the upload and TEX0: the module binds the picture itself)

void renaSetShadowTexture(sceVif1Packet *packet, Tim2Texture *texture, int block)
{
    (void)packet;
    (void)block;
    sShadows.texture = texture;
}

// The record's lights, for the models that have none of their own.
// FS2 0x002e8080
// callcheck: getDrawTarget (its answer is not used)

void renaSetLights(sceVif1Packet *packet, DrawSysLights *lights)
{
    (void)packet;
    copyVector3(sLights.ambient, lights->getAmbient()->getColor());
    for (int i = 0; i < DrawSysLights::kLightCount; i++) {
        DrawSysDirLight *light = lights->getLight(i);
        copyVector3(sLights.colors[i], light->getColor());
        copyVector3(sLights.directions[i], light->getDirection());
    }
}

// FS2 0x002e8300
// callcheck: FUN_0010aef0, FUN_0010af80, FUN_0010b5f0, sceVif1PkCloseDmaTag, sceVif1PkCnt (packets: the pass carries them)

void renaSetShadowMatrices(sceVif1Packet *packet, int index, sceVu0FMATRIX projection, sceVu0FMATRIX clip)
{
    (void)packet;
    if (index < 0 || index >= kYamiPanModelMaxShadows)
        return;
    memcpy(sShadows.projection[index], projection, sizeof(sceVu0FMATRIX));
    memcpy(sShadows.clip[index], clip, sizeof(sceVu0FMATRIX));
}

// FS2 0x002e8570
// callcheck: FUN_0010aef0, FUN_0010af80, FUN_0010b288, sceVif1PkCloseDmaTag, sceVif1PkCnt (packets: the pass carries it)

void renaSetShadowCount(sceVif1Packet *packet, int count)
{
    (void)packet;
    sShadows.count = count;
}

// The point light's constants, VU addresses 176..179; only a pass with a
// scene's render has them, the ROM zeroes VU 176 otherwise (0x2e5450).
// FS2 0x002e05f0

static void fillPointLight(YamiPanModelPointLight &out)
{
    const SceneRender *render = gDrawSysState.render;
    if (render == nullptr || !render->isActive())
        return;
    out.active = 1;
    copyVector3(out.position, render->getMatrix()[3]);
    copyVector3(out.color, render->color);
    copyVector3(out.shade, render->shade);
    out.weight = render->weight;
    out.slope = -render->weight / render->radius;
}

// The spot's constants, and the planes VU1 0x66b holds each batch's box against:
// the far one, then the cone's four sides through the lamp, normals inwards.
// FS2 0x002e00f0
// callcheck: FUN_0010b220, sceVu0ApplyMatrix, sceVu0OuterProduct, FUN_0010b820 (the packet and the transposes: the planes are worked out in the world here)

static void fillSpotLight(YamiPanModelSpotLight &out)
{
    // The object's axes are x right, y ahead, z up; the light's x, up, ahead.
    static sceVu0FMATRIX sLightAxes = { { 1, 0, 0, 0 }, { 0, 0, 1, 0 }, { 0, 1, 0, 0 }, { 0, 0, 0, 1 } };
    constexpr float kReachLimit = 1.2f; // 0x2d578c

    if (sSpot == nullptr || sSpotTexture == nullptr || !sSpot->isActive())
        return;

    const sceVu0FVECTOR *matrix = sSpot->getMatrix();
    sceVu0FMATRIX toObject;
    for (int column = 0; column < 3; column++) {
        for (int row = 0; row < 3; row++)
            toObject[column][row] = matrix[row][column];
        toObject[column][3] = 0.0f;
    }
    for (int row = 0; row < 3; row++) {
        toObject[3][row] = -(matrix[row][0] * matrix[3][0] + matrix[row][1] * matrix[3][1]
                             + matrix[row][2] * matrix[3][2]);
    }
    toObject[3][3] = 1.0f;

    sceVu0FMATRIX toLight;
    sceVu0MulMatrix(toLight, sLightAxes, toObject);

    out.active = 1;
    out.texture = sSpotTexture;
    memcpy(out.worldToLight, toLight, sizeof(out.worldToLight));
    out.scaleX = (1.0f / sSpot->intensity) / sSpot->tangent;
    out.scaleY = 1.0f / sSpot->tangent;
    out.falloff[0] = sSpot->_90[0];
    out.falloff[1] = sSpot->_90[1] / sSpot->radius;
    out.falloff[2] = sSpot->_90[2] / (sSpot->radius * sSpot->radius);
    out.limit = sSpot->reach * kReachLimit;

    constexpr float kDefaultReach = 1200.0f;
    const float halfTangent = tanf(sSpot->angle * 0.5f);
    const float side = halfTangent * sSpot->intensity;
    const float *ahead = matrix[1];
    const float *place = matrix[3];
    const float aheadAtLamp = ahead[0] * place[0] + ahead[1] * place[1] + ahead[2] * place[2];
    out.planes[0][0] = -ahead[0];
    out.planes[0][1] = -ahead[1];
    out.planes[0][2] = -ahead[2];
    out.planes[0][3] = (sSpot->reach != 0.0f ? out.limit : kDefaultReach) + aheadAtLamp;

    const float corners[4][3] = {
        { side, 1.0f, halfTangent },
        { side, 1.0f, -halfTangent },
        { -side, 1.0f, -halfTangent },
        { -side, 1.0f, halfTangent },
    };
    for (int i = 0; i < 4; i++) {
        const float *a = corners[i];
        const float *b = corners[(i + 1) % 4];
        const float normal[3] = { a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0] };
        float *plane = out.planes[i + 1];
        for (int axis = 0; axis < 3; axis++)
            plane[axis] = matrix[0][axis] * normal[0] + matrix[1][axis] * normal[1] + matrix[2][axis] * normal[2];
        plane[3] = -(plane[0] * place[0] + plane[1] * place[1] + plane[2] * place[2]);
    }
}

// The camera in the world, from its matrix: VU address 4, a part of 0x2e5da0.
// FS2 0x002e5da0
// callcheck: FUN_0010aef0, FUN_0010af80, FUN_0010b220, FUN_0010b5f0, FUN_002e5fc0, sceVif1PkCloseDmaTag, sceVif1PkCnt (the frame's constants and clip planes: the module's)

static void fillEye(float *out)
{
    const sceVu0FVECTOR *view = (const sceVu0FVECTOR *)gDrawSysState.view;
    for (int axis = 0; axis < 3; axis++)
        out[axis] = -(view[axis][0] * view[3][0] + view[axis][1] * view[3][1] + view[axis][2] * view[3][2]);
}

// A pass: every model of the groups in mask, then what blends, far to near
// (the ROM's own sort is 0x2e8990, a quicksort on the key).
// FS2 0x002e5450
// callcheck: sceVu0UnitMatrix, FUN_002e5da0, FUN_002e5bb0, FUN_002e5a00, FUN_002e7e90, FUN_002e7c30, drawModels, FUN_002e8990, drawDeferred (the first three and the GS set-up are VU1's constants: file header)

void renaDrawModels(sceVif1Packet *packet, DrawSysRecord *record, int mask)
{
    YamiPanModelPass pass = {};
    for (int row = 0; row < 4; row++) {
        for (int column = 0; column < 4; column++)
            pass.viewScreen[row][column] = gDrawSysState.viewScreen[row][column];
    }
    pass.screenCenterX = gDrawSysState.centerX;
    pass.screenCenterY = gDrawSysState.centerY;
    pass.screenHalfWidth = (float)gDrawSysState.width / 2.0f;
    pass.screenHalfHeight = (float)gDrawSysState.height / 2.0f;
    pass.depthMin = 1.0f;
    pass.depthMax = 16777215.0f;
    pass.fogA = 255.0f;
    pass.fogNearValue = 255.0f;     // the ROM's no fog (0x2e5a00)
    pass.fogFarValue = 255.0f;
    pass.clearDepth = (mask & (DrawSysGroup_Far1 | DrawSysGroup_Far2 | DrawSysGroup_Far3)) == 0;
    fillEye(pass.eye);
    fillPointLight(pass.point);
    fillSpotLight(pass.spot);
    pass.shadows = sShadows;

    const DrawSysFog *fog = gDrawSysState.fog;
    if (fog != nullptr && fog->isEnabled()) {
        copyVector3(pass.fogColor, fog->getColor());
        pass.fogA = fog->getCoefficientA();
        pass.fogB = fog->getCoefficientB();
        pass.fogNearValue = fog->getNearValue();
        pass.fogFarValue = fog->getFarValue();
    }

    YamiPan_ModelBeginPass(packet, &pass);
    drawModels(packet, record, mask);

    std::stable_sort(sDeferred.begin(), sDeferred.end(),
                     [](const Deferred &a, const Deferred &b) { return a.key < b.key; });
    for (const Deferred &entry : sDeferred)
        drawDeferred(packet, entry);
    sDeferred.clear();
    sDeferredSkins.clear();
}

void renaBeginEffects(sceVif1Packet *packet, const sceVu0FVECTOR *screen)
{
    YamiPanModelPass pass = {};
    for (int row = 0; row < 4; row++) {
        for (int column = 0; column < 4; column++)
            pass.viewScreen[row][column] = screen[row][column];
    }
    pass.screenCenterX = gDrawSysState.centerX;
    pass.screenCenterY = gDrawSysState.centerY;
    pass.screenHalfWidth = (float)gDrawSysState.width / 2.0f;
    pass.screenHalfHeight = (float)gDrawSysState.height / 2.0f;
    pass.depthMin = 1.0f;
    pass.depthMax = 16777215.0f;
    pass.fogA = 255.0f;
    pass.fogNearValue = 255.0f;
    pass.fogFarValue = 255.0f;
    const DrawSysFog *fog = drawSysCurrent()->lights->getFog();
    if (fog != nullptr)
        copyVector3(pass.fogColor, fog->getColor());
    pass.viewSpace = 1;     // PC: smooth motion moves these with the camera
    YamiPan_ModelBeginPass(packet, &pass);
}

// FS2 0x002e85e0
// callcheck: FUN_0010ad90, FUN_0010b028, FUN_0010b6e0, getDrawTarget, sceVif1PkAddGsAD, sceVif1PkCloseDmaTag, sceVif1PkCloseGifTag, sceVif1PkCnt, sceVif1PkOpenDirectCode, sceVif1PkOpenGifTag (packets and VU1 state: the draw module's on the host)

void renaEndPass(sceVif1Packet *packet)
{
    YamiPan_ModelEndPass(packet);
}
