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
 *  Not written yet (open-items.md "Next up"): skinned meshes 0x2e43e0 /
 *  0x2e68a0, a ModelMaterial_Kind1's frames, the shadow projections, the
 *  spotlight, the ghost's pass 0x2e7960, the fade 0x2e9220 / 0x2e96a0, mips.
 * ======================================================================== */
#include "rena/rena_draw.h"

#include <algorithm>
#include <stdlib.h>
#include <string.h>
#include <vector>

#include "game/drawsys.h"
#include "game/gamehooks.h"
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

// One material of a blended model part, waiting for its place (0x20 bytes in
// the table at 0x59d240, sorted through the keys at 0x59b240). layout: host.
struct Deferred
{
    ModelInstance           *model;
    const ModelMeshMaterial *material;
    const ModelPartRec      *rec;
    const float             (*matrix)[4];
    int                      key;
    bool                     unlit;
};
std::vector<Deferred> sDeferred;

constexpr int   kNoSurfaceUvSet = 1;    // what 0x2e6b70 hands on without a surface
constexpr float kFrameHalf = 256.0f;
constexpr float kCullWidthMargin = 3.0f; // PC: every aspect ratio stays inside the box test

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
float partScreenZ(const ModelMeshPart *part, const float (*world)[4])
{
    const float *center = partCenter(part);
    const float point[4] = { center[0], center[1], center[2], 1.0f };
    float placed[4];
    float screen[4];
    transformPoint(placed, world, point);
    transformPoint(screen, (const float (*)[4])gDrawSysState.viewScreen, placed);
    return screen[2];
}

// Whether the part's box lies wholly outside the frame: what 0x2e2a60
// answers with 2.
bool partOutside(const ModelMeshPart *part, const float (*world)[4])
{
    const float *center = partCenter(part);
    const float *half = center + 4;
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
    YamiPan_ModelSetLights(packet, &lights);
}

// The GS and VU1 state of a surface: FS2 0x002e7240, as far as a host draw
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
    }
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
// with the lists, a copy of its tags: 0x2e95d0); here its data goes straight
// to the module.
// FS2 0x002e83b0

void drawPrims(sceVif1Packet *packet, const ModelMeshMaterial *material, const float (*world)[4], int colorSet,
               int uvSet)
{
    const ModelPrim *prims = material->prims.get();
    for (int p = 0; p < material->primCount; p++) {
        const ModelPrim &prim = prims[p];
        const int tagCount = prim.isLong ? ModelBatch::kLongTags : ModelBatch::kShortTags;
        const int stride = (prim.isLong ? ModelBatch::kLongSize : ModelBatch::kShortSize)
                         + (prim.skinned ? 0 : ModelBatch::kPadSize);
        const char *record = (const char *)prim.batches.get();
        for (int b = 0; b < prim.batchCount; b++, record += stride) {
            const ModelTag *tags = ((const ModelBatch *)record)->tags;
            const ModelPrim::Header *header = (const ModelPrim::Header *)tags[0].next.get();

            YamiPanModelBatch batch;
            batch.positions = (const float *)tags[1].next.get();
            batch.normals = (const float *)tags[2].next.get();
            if (prim.skinned) {
                batch.colors = (const uint8_t *)listEntry(tags[tagCount - 2], colorSet);
                batch.uvs = (const int16_t *)listEntry(tags[tagCount - 1], uvSet);
            } else {
                batch.colors = (const uint8_t *)tags[tagCount - 2].next.get();
                batch.uvs = (const int16_t *)tags[tagCount - 1].next.get();
            }
            batch.count = header->vertexCount;
            batch.strip = (header->flags & ModelPrim::Header::Strip) != 0;
            YamiPan_ModelDrawBatch(packet, world, &batch);
        }
    }
}

// One material. The ROM's second half, a ModelMaterial_Kind1's two frames
// over its base, is not written yet (file header): the base is drawn.
// FS2 0x002e6b70

void drawMaterial(sceVif1Packet *packet, ModelInstance *model, const ModelMeshMaterial *material,
                  const ModelPartRec *rec, const float (*world)[4], bool blended, bool unlit)
{
    const ModelSurface *surface = material->material < 0 ? nullptr : model->surfaceOfMaterial[material->material];
    if (surface != nullptr && surface->material->kind != ModelMaterial_Textured) {
        const short base = surface->material->uvSet;
        if (base < 0)
            return;
        surface = model->surfaceOfMaterial[base];
    }
    setSurface(packet, model, surface, blended, unlit);
    drawPrims(packet, material, world, rec->vcmFadeBits,
              surface != nullptr ? surface->material->uvSet : kNoSurfaceUvSet);
}

// One part: its opaque materials now, the others into the table.
// FS2 0x002e6590

void drawPart(sceVif1Packet *packet, ModelInstance *model, const ModelPartRec *rec, const ModelMeshPart *part,
              const float (*world)[4], bool ghost, bool unlit)
{
    const ModelMeshMaterial *materials = part->materials.get();
    for (int i = 0; i < part->materialCount; i++) {
        const ModelMeshMaterial *material = &materials[i];
        const ModelSurface *surface = material->material < 0 ? nullptr : model->surfaceOfMaterial[material->material];
        const bool blended = surface != nullptr && (surface->material->flags & ModelMaterialFlag_Blended) != 0;
        if (!blended && !ghost) {
            drawMaterial(packet, model, material, rec, world, false, unlit);
            continue;
        }
        Deferred entry = { model, material, rec, world, 0, unlit };
        entry.key = (int)partScreenZ(part, world) + (int)sDeferred.size();
        sDeferred.push_back(entry);
    }
}

// A model's mesh tree, skipping what the switches of its records hide.
// FS2 0x002e41a0

void drawTree(sceVif1Packet *packet, ModelInstance *model, bool ghost)
{
    const ModelFileNode *node = model->node;
    if (node->meshCount == 0)
        return;
    const ModelMesh *meshes = node->meshes.get();
    u_int index = 0;
    do {
        const ModelPartRec *rec = &model->partRecs[index];
        if (rec->flagE != 0 || (rec->drawMode & 2) != 0) {
            index = rec->next;
            continue;
        }
        if (rec->hasMesh && rec->drawMode == 0) {
            const ModelMesh *mesh = &meshes[index];
            const ModelMeshParts *parts = mesh->parts.get();
            // The skinned path, 0x2e43e0, is not written yet.
            if (parts->boneCount == 0) {
                const float (*world)[4] = mesh->bone < 0 ? (const float (*)[4])model->rootBone->matrix
                                                         : (const float (*)[4])model->bones[mesh->bone].matrix;
                const ModelMeshPart *list = parts->parts.get();
                const bool unlit = (mesh->vuFlags & ModelMeshFlag_Unlit) != 0;
                for (int p = 0; p < parts->count; p++) {
                    if (list[p].materials.rel != 0 && !partOutside(&list[p], world))
                        drawPart(packet, model, rec, &list[p], world, ghost, unlit);
                }
            }
        }
        index = rec->firstChild != 0 ? rec->firstChild : rec->next;
    } while (index != 0);
}

// The record's models of a group, each with its lights.
// FS2 0x002e62f0

void drawModels(sceVif1Packet *packet, DrawSysRecord *record, int mask)
{
    for (ModelInstance *model : record->models) {
        if ((mask & model->drawGroup) == 0 || model->distant != 0 || model->shown == 0)
            continue;
        setModelLights(packet, model);
        drawTree(packet, model, (model->flags & ModelFlag_Ghost) != 0);
    }
}

// One entry of the table.
// FS2 0x002e5860

void drawDeferred(sceVif1Packet *packet, const Deferred &entry)
{
    setModelLights(packet, entry.model);
    drawMaterial(packet, entry.model, entry.material, entry.rec, entry.matrix, true, entry.unlit);
}

} // namespace

// PC: YAMIPAN_CAMERA=auto, until a character links the main camera: it looks
// at the biggest part of the map model from above its side, in the model's
// own axes (y up), once.
// callcheck: none (host only)

static void aimDebugCamera(DrawSysRecord *record)
{
    static bool sDone = false;
    const char *mode = getenv("YAMIPAN_CAMERA");
    if (sDone || mode == nullptr || strcmp(mode, "auto") != 0)
        return;
    for (ModelInstance *model : record->models) {
        if (strcmp(model->node->name.get(), "Root") != 0)
            continue;
        const ModelMeshPart *best = nullptr;
        float bestSize = 0.0f;
        const float (*world)[4] = nullptr;
        const ModelMesh *meshes = model->node->meshes.get();
        for (int m = 0; m < model->node->meshCount; m++) {
            const ModelMeshParts *parts = meshes[m].parts.get();
            if (parts == nullptr || parts->boneCount != 0)
                continue;
            const ModelMeshPart *list = parts->parts.get();
            for (int p = 0; p < parts->count; p++) {
                const float *half = partCenter(&list[p]) + 4;
                const float size = half[0] * half[2];
                if (size > bestSize) {
                    bestSize = size;
                    best = &list[p];
                    world = meshes[m].bone < 0 ? (const float (*)[4])model->rootBone->matrix
                                               : (const float (*)[4])model->bones[meshes[m].bone].matrix;
                }
            }
        }
        if (best == nullptr)
            return;
        const float *center = partCenter(best);
        const float kHeight = 120.0f;
        const float kBack = 420.0f;
        const float eyePoint[4] = { center[0], center[1] + kHeight, center[2] - kBack, 1.0f };
        const float upPoint[4] = { 0.0f, 1.0f, 0.0f, 0.0f };
        float eye[4];
        float target[4];
        float up[4];
        transformPoint(eye, world, eyePoint);
        transformPoint(target, world, center);
        transformPoint(up, world, upPoint);
        stageCameraDebugLookAt(eye, target, up);
        sDone = true;
        return;
    }
}

// The VU1 program and the frame's GS state: nothing for the host.
// FS2 0x002e86b0

void renaBeginFrame(sceVif1Packet *packet)
{
    (void)packet;
}

// FS2 0x002e87f0

void renaSetSpotlight(sceVif1Packet *packet, Tim2Texture *texture, int block)
{
    (void)packet;
    (void)texture;
    (void)block;
}

// FS2 0x002e82a0

void renaSetShadowTexture(sceVif1Packet *packet, Tim2Texture *texture, int block)
{
    (void)packet;
    (void)texture;
    (void)block;
}

// PC, TEMPORARY: while SceneRender's lights are stubs every model has drawSysInit's ambient, 0.1,
// and the VU1 formula draws the map almost black. YAMIPAN_AMBIENT=<level> replaces it, to look at
// one. Delete this with the pinned task game/scenerender.cpp (open-items.md, top of "Next up").
// callcheck: none (host only)

static void applyDebugAmbient(float *ambient)
{
    const char *text = getenv("YAMIPAN_AMBIENT");
    if (text == nullptr)
        return;
    const float level = (float)atof(text);
    ambient[0] = ambient[1] = ambient[2] = level;
}

// The record's lights, for the models that have none of their own.
// FS2 0x002e8080
// callcheck: getDrawTarget (its answer is not used)

void renaSetLights(sceVif1Packet *packet, DrawSysLights *lights)
{
    (void)packet;
    copyVector3(sLights.ambient, lights->getAmbient()->getColor());
    applyDebugAmbient(sLights.ambient);
    for (int i = 0; i < DrawSysLights::kLightCount; i++) {
        DrawSysDirLight *light = lights->getLight(i);
        copyVector3(sLights.colors[i], light->getColor());
        copyVector3(sLights.directions[i], light->getDirection());
    }
}

// FS2 0x002e8300

void renaSetShadowMatrices(sceVif1Packet *packet, int index, sceVu0FMATRIX projection, sceVu0FMATRIX clip)
{
    (void)packet;
    (void)index;
    (void)projection;
    (void)clip;
}

// FS2 0x002e8570

void renaSetShadowCount(sceVif1Packet *packet, int count)
{
    (void)packet;
    (void)count;
}

// A pass: every model of the groups in mask, then what blends, far to near
// (the ROM's own sort is 0x2e8990, a quicksort on the key).
// FS2 0x002e5450
// callcheck: sceVu0UnitMatrix, FUN_002e5da0, FUN_002e5bb0, FUN_002e5a00, FUN_002e7e90, FUN_002e7c30, FUN_002e62f0, FUN_002e8990, FUN_002e5860 (the first three and the GS set-up are VU1's constants: file header)

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
    pass.clearDepth = (mask & (DrawSysGroup_Far1 | DrawSysGroup_Far2 | DrawSysGroup_Far3)) == 0;

    const DrawSysFog *fog = gDrawSysState.fog;
    if (fog != nullptr && fog->isEnabled()) {
        copyVector3(pass.fogColor, fog->getColor());
        pass.fogA = fog->getCoefficientA();
        pass.fogB = fog->getCoefficientB();
    }

    aimDebugCamera(record);
    YamiPan_ModelBeginPass(packet, &pass);
    drawModels(packet, record, mask);

    std::stable_sort(sDeferred.begin(), sDeferred.end(),
                     [](const Deferred &a, const Deferred &b) { return a.key < b.key; });
    for (const Deferred &entry : sDeferred)
        drawDeferred(packet, entry);
    sDeferred.clear();
}

// FS2 0x002e85e0

void renaEndPass(sceVif1Packet *packet)
{
    YamiPan_ModelEndPass(packet);
}
