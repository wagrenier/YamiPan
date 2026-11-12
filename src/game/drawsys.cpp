/* ==========================================================================
 *  game/drawsys.cpp -- the draw system, 0x2d46a0..0x2dac70 (game/drawsys.h)
 *
 *  The record the stage draws through, its camera, lights and fogs, the
 *  shadow list and the frame's passes. The file name is the port's: there is
 *  no string, and the code knows stages and objects, so it is game code on
 *  top of rena (rena/rena_draw.h has the calls it makes).
 *
 *  The passes of drawSysRender, in the ROM's list order:
 *    list 0  the frame's set-up: VU1, textures, lights, shadow matrices
 *    list 2  far_ with fog 0              at 1 / 20 scale, no shadows
 *    list 4  far_1..3 with fogs 1..3      the same
 *    list 5  depth clear, then the models with the shadows and the lights'
 *            fog
 *    list 7  (with the target view flag) depth clear, the target's models
 *
 *  PC: the ROM's GS memory work is gone (the user's rule: textures are
 *  decoded at their source): the per-frame GsRegionAlloc on the scratchpad,
 *  the writes to the packet's pointer that switch it to the uncached mirror
 *  (0x30000000), FlushCache and the TEST_1 packets around the depth clear.
 *  The boss stage's shadow target goes to the host's module whole
 *  (yamipan/rendering/yamipan_drawshadow.h): its frames, texture pages, the
 *  T4 / T8H copy that reads the result as an 8-bit image and the CLUT upload
 *  are GS memory work; its sizes, taps and alpha ramp stay here.
 * ======================================================================== */
#include <algorithm>
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include "common/yamipan_stub.h"

#include "game/drawsys.h"
#include "game/gamehooks.h"
#include "game/gamepacket.h"
#include "game/gfxtarget.h"
#include "game/gsblockmap.h"
#include "game/mathutil.h"
#include "game/scenerender.h"
#include "game/stagescene.h"
#include "game/sysdata.h"
#include "game/vu0math.h"
#include "rena/rena_draw.h"
#include "yamipan/rendering/yamipan_drawshadow.h"

DrawSysShadow gDrawSysShadows[kDrawSysMaxShadows];  /* bss 0x59ae90 */
DrawSysState  gDrawSysState;                        /* bss 0x59b0c0 */

static DrawSysRecord *sCurrent;         /* sbss 0x31aedc (gp-0x7994) */
static DrawSysRecord *sSaved;           /* sbss 0x31aee0 (gp-0x7990) */
static float          sDrawDistance;    /* sbss 0x31aee4 (gp-0x798c) */
static SceneRender   *sSceneRender;     /* sbss 0x31aee8 (gp-0x7988) */
static GsRegionAlloc *sRegionAlloc;     /* sbss 0x31aeec (gp-0x7984) */
static Tim2Texture   *sSpotlightTexture; /* sbss 0x31aef0 (gp-0x7980) */
static Tim2Texture   *sShadowTexture;   /* sbss 0x31aef4 (gp-0x797c) */
static int            sSpotlightBlock;  /* sbss 0x31aef8 (gp-0x7978) */
static int            sShadowBlock;     /* sbss 0x31aefc (gp-0x7974) */
static int            sShadowCount;     /* sbss 0x31af08 (gp-0x7968) */
static int            sReservedBlock;   /* sbss 0x31af0c (gp-0x7964): 0x120 blocks of the TexArea (unread) */
static ModelInstance *sBossModel;      /* sbss 0x31af10 (gp-0x7960) */
static u_char         sTargetView;      /* sbss 0x31af14 (gp-0x795c) */

static DrawSysFog *sFogs[kDrawSysFogCount]; /* bss 0x59ae60 */

// The stage whose one shadow is the boss's (StageSelect::stage).
constexpr int kStageBoss = 0x1b;
constexpr float kPi = 3.14159274f;             /* sdata 0x31ad08 */
constexpr float kFarPassScale = 0.05f;
constexpr float kDefaultDrawDistance = 200000.0f;
constexpr float kShadowBehindCamera = 100000.0f;
// The shadow's tilt stays between these (radians), off straight down.
constexpr float kShadowPitchMax = 2.96706f;     /* 0x30fbc0 */
constexpr float kShadowPitchMin = 2.61799f;     /* 0x30fbc8 */
// The screen: 512 away, 4 : 3 of 640 x 448 in the 512-tall frame.
constexpr float kScreenDistance = 512.0f;
constexpr float kScreenAspectY = 0.93333334f;
constexpr float kScreenCenter = 2048.0f;
constexpr float kScreenZMin = 1.0f;
constexpr float kScreenZMax = 16777215.0f;
constexpr float kScreenNear = 5.0f;
constexpr int   kReservedBlocks = 0x120;
// A model's name, for the flags and groups it asks for.
constexpr const char kModelExtGhost[] = "ext_ghost";    /* 0x30fbf0 */
constexpr const char kModelTagged0[]  = "oXX27_11";     /* 0x30fc00 */
constexpr const char kModelTagged1[]  = "oXX27_05";     /* 0x30fc10 */
constexpr const char kMaterialWave[]  = "WaveComposit"; /* 0x30fc40 */
constexpr float kGhostParam0 = 191.0f;
constexpr float kGhostParam1 = -52.0f;
constexpr char  kNameTagC = 'c';
constexpr int   kNameTagSpace = 0x10;
// The far models, by name (strcasecmp): far_, far_1, far_2, far_3.
static const struct { const char *name; int group; } kFarModels[] = {
    { "far_",  DrawSysGroup_Far0 }, /* 0x30fc20 */
    { "far_1", DrawSysGroup_Far1 }, /* 0x30fc28 */
    { "far_2", DrawSysGroup_Far2 }, /* 0x30fc30 */
    { "far_3", DrawSysGroup_Far3 }, /* 0x30fc38 */
};
// The plant textures, sorted for the search (0x2fd9c0..0x2fd9e4).
static const char *const kFoliageTextures[] = {
    "tCM99_PLTbrchA_01", "tCM99_PLTbrchB_01", "tCM99_PLTgrsC_01",
    "tCM99_PLTgrsD_01",  "tCM99_PLTgrsE_03",  "tCM99_PLTgrsE_04",
    "tCM99_PLTleafA_01", "tCM99_PLTleafB_01", "tCM99_PLTleafC_01",
};
// The boss's shadow target: 256 square, seen from this far above the shadow.
constexpr int   kBossShadowSize = 0x100;
constexpr int   kBossShadowMinSize = 0x40;
constexpr float kBossShadowHeight = 800.0f;
constexpr float kBossShadowScreenDistance = 256.0f;
constexpr u_short kBossShadowDrawMode = 0x87;
// The blend of one blit's three passes, alpha 1, 1/2, 1/3 (0x30fa40 / 50 / 60,
// one table three times): the mean of the three.
static const u_int kBossShadowTaps[kYamiPanShadowTaps] = { 0x80808080, 0x40808080, 0x2b808080 };
constexpr int   kBossShadowAlphaMax = 0x80;     /* 0x31acf8 */
constexpr int   kBossShadowAlphaMin = 0;        /* 0x31acfc */

// PC: the ROM's libc strcasecmp / strncasecmp (0x127ea0..); MSVC has no such
// names.
static int nameCompareNoCase(const char *a, const char *b, size_t length)
{
    for (size_t i = 0; i < length; i++, a++, b++) {
        int ca = tolower((u_char)*a);
        int cb = tolower((u_char)*b);
        if (ca != cb || ca == 0)
            return ca - cb;
    }
    return 0;
}
static bool nameEqualsNoCase(const char *a, const char *b)
{
    return nameCompareNoCase(a, b, (size_t)-1) == 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002d4780

void drawSysClearTargetView()
{
    sTargetView = 0;
}

// FS2 0x002d4790

void drawSysSetTargetView()
{
    sTargetView = 1;
}

// FS2 0x002d47a0

DrawSysFog *drawSysGetFog(int index)
{
    return sFogs[index];
}

// The first surface of kind 1 over the model's meshes, null with none:
// StageModel::init keeps it (+0xa8, unread).
// FS2 0x002d46a0

ModelSurface *drawSysFindKind1Surface(ModelInstance *model)
{
    const ModelFileNode *node = model->node;
    for (int m = 0; m < node->meshCount; m++) {
        const ModelMeshParts *parts = node->meshes.get()[m].parts;
        if (parts == nullptr)
            continue;
        for (int p = 0; p < parts->count; p++) {
            const ModelMeshPart &part = parts->parts.get()[p];
            for (int i = 0; i < part.materialCount; i++) {
                int material = part.materials.get()[i].material;
                if (material < 0)
                    continue;
                // PC: the ROM reads through a null entry; every material a
                // mesh names has a surface.
                ModelSurface *surface = model->surfaceOfMaterial[material];
                if (surface != nullptr && surface->material->kind == ModelMaterial_Frames)
                    return surface;
            }
        }
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The two walks of the shadow code: the group 8 objects when there are any,
// else the characters, the active ones only.

static StageObject **firstActive(StageObjArray &list)
{
    StageObject **slot = list.begin();
    while (slot != nullptr && !(*slot)->m_active)
        slot = StageObjArray::next(slot);
    return slot;
}

static StageObject **nextActive(StageObject **slot)
{
    slot = StageObjArray::next(slot);
    while (slot != nullptr && !(*slot)->m_active)
        slot = StageObjArray::next(slot);
    return slot;
}

static StageObjArray &shadowCasters()
{
    StageObjectList *objects = gStageScene->objects;
    StageObjArray &group8 = objects->m_lists[StageObjGroup_8];
    return group8.m_count != 0 ? group8 : objects->m_lists[StageObjGroup_Characters];
}

// A caster before the sort. layout: host -- EE 0x60 on the scratchpad at
// 0x70000000.
struct ShadowWork
{
    sceVu0FMATRIX   matrix;     // FS2 +0x00
    sceVu0FVECTOR   size;       // FS2 +0x40
    StageObject    *object;     // FS2 +0x50
    StageModel     *model;      // FS2 +0x54
    ModelInstance *data;       // FS2 +0x58
};
// Its sort key. layout: host -- EE 8, after the records.
struct ShadowKey
{
    float depth;                // FS2 +0x0
    int   index;                // FS2 +0x4
};

// ──────────────────────────────────────────────────────────────────────
// The frame's shadows: a matrix and size for each shown caster nearer than
// the draw distance, the seven nearest kept in gDrawSysShadows. Answers how
// many. The boss stage has one, its own. Outside a sub-scene, a far caster's
// model is also marked distant.
// FS2 0x002d47c0
// callcheck: FUN_00136400, FUN_002da2e0 (FUN_00136400 is identityMatrix; the ROM sort is the MSL instance 0x2da2e0, std::sort here, listed at the end of the file)

static int drawSysBuildShadows(const sceVu0FVECTOR *view)
{
    if (getStageSelect()->stage == kStageBoss)
        return drawSysBuildBossShadow();

    static std::vector<ShadowWork> works;
    static std::vector<ShadowKey> keys;
    works.clear();
    keys.clear();

    for (StageObject **slot = firstActive(shadowCasters()); slot != nullptr; slot = nextActive(slot)) {
        StageObject *object = *slot;
        StageModel *model = object->m_model;
        ModelInstance *data = model->data;
        if (data == nullptr || (data->flags & ModelFlag_CastsShadow) == 0 || !data->shown
            || (data->drawGroup & DrawSysGroup_Main) == 0)
            continue;

        ShadowWork work;
        const sceVu0FVECTOR *bone = data->bones[2].matrix;
        identityMatrix(work.matrix);

        float x = model->shadowDirection[0];
        float y = model->shadowDirection[1];
        float z = model->shadowDirection[2];
        float yaw = atan2f(-x, y);
        float pitch = atan2f(sqrtf(x * x + y * y), -z);
        pitch = std::max(std::min(pitch, kShadowPitchMax), kShadowPitchMin);
        rotateMatrixX(pitch, work.matrix, work.matrix);
        rotateMatrixZ(yaw, work.matrix, work.matrix);

        sceVu0FVECTOR offset;
        memcpy(work.matrix[3], bone[3], sizeof(sceVu0FVECTOR));
        applyMatrix(offset, bone, model->shadowOffset);
        for (int lane = 0; lane < 4; lane++)
            work.matrix[3][lane] += offset[lane];

        memcpy(work.size, model->shadowSize, sizeof(sceVu0FVECTOR));
        // sinCos mode 1 is the cosine; the pitch is past a quarter turn.
        work.size[1] = model->shadowSize[1] / -sinCos(pitch, 1);

        sceVu0FVECTOR viewPosition;
        applyMatrix(viewPosition, view, work.matrix[3]);
        float depth = viewPosition[1];
        if (depth < 0.0f)
            depth = kShadowBehindCamera;
        if (depth <= sDrawDistance) {
            work.object = object;
            work.model = model;
            work.data = data;
            keys.push_back({depth, (int)works.size()});
            works.push_back(work);
        }
    }

    std::sort(keys.begin(), keys.end(),
              [](const ShadowKey &a, const ShadowKey &b) { return a.depth < b.depth; });

    int count = std::min((int)keys.size(), kDrawSysMaxShadows);
    for (int i = 0; i < count; i++) {
        const ShadowWork &work = works[keys[i].index];
        memcpy(gDrawSysShadows[i].matrix, work.matrix, sizeof(sceVu0FMATRIX));
        memcpy(gDrawSysShadows[i].size, work.size, sizeof(sceVu0FVECTOR));
    }

    if (!gStageScene->isSubScene) {
        float farSq = sDrawDistance * sDrawDistance;
        float distantSq = farSq * 0.5f * 0.5f;
        sceVu0FMATRIX eye;
        invertRigidMatrix(eye, view);
        for (int i = 0; i < count; i++) {
            const ShadowWork &work = works[keys[i].index];
            float distanceSq = vecDistanceSq(eye[3], work.object->m_xform.position);
            if (distanceSq <= distantSq || work.model->m_mayGoDistant == 0)
                work.data->flags &= ~ModelFlag_Distant;
            else
                work.data->flags |= ModelFlag_Distant;
            if (farSq < distanceSq)
                work.data->clipped = 1;
        }
    }
    return count;
}

// The boss stage's shadow: straight down from F_E27_BOSS's left calf. Answers
// 1, or 0 with no boss.
// FS2 0x002d4cf0
// callcheck: strcmp, strcmp, sceVu0UnitMatrix, sceVu0RotMatrixX

int drawSysBuildBossShadow()
{
    StageObject **slot = firstActive(shadowCasters());
    while (slot != nullptr && strcmp("F_E27_BOSS", (*slot)->m_name) != 0)
        slot = nextActive(slot);
    if (slot == nullptr) {
        sBossModel = nullptr;
        return 0;
    }

    ModelInstance *data = (*slot)->m_model->data;
    sBossModel = data;
    ModelBone *bone = data->bones;
    for (u_int i = 0; i < data->boneCount; i++) {
        bone = &data->bones[i];
        if (strcmp(bone->node->name, "bn_l_calf") == 0)
            break;
    }

    sceVu0FMATRIX matrix;
    sceVu0UnitMatrix(matrix);
    sceVu0RotMatrixX(matrix, matrix, kPi);

    static const sceVu0FVECTOR kBossShadowSize = { 400.0f, 400.0f, 1200.0f, 1.0f }; /* 0x30fbb0 */
    DrawSysShadow &shadow = gDrawSysShadows[0];
    memcpy(shadow.matrix, matrix, sizeof(sceVu0FMATRIX));
    memcpy(shadow.matrix[3], bone->matrix[3], sizeof(sceVu0FVECTOR));
    memcpy(shadow.size, kBossShadowSize, sizeof(sceVu0FVECTOR));
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002d4ec0

void drawSysSetShadowTexture(Tim2Texture *texture, int block)
{
    sShadowTexture = texture;
    sShadowBlock = block;
}

// FS2 0x002d4ed0

void drawSysSetSpotlightTexture(Tim2Texture *texture, int block)
{
    sSpotlightTexture = texture;
    sSpotlightBlock = block;
}

// FS2 0x002d4ee0

void drawSysSetSceneRender(SceneRender *render)
{
    sSceneRender = render;
}

// FS2 0x002d4ef0
// callcheck: renaStepTexAnim

void drawSysTick(int ticks)
{
    for (ModelInstance *model : sCurrent->models)
        for (int i = 0; i < model->texAnimCount; i++)
            renaStepTexAnim(model->texAnims[i], ticks);
}

// FS2 0x002d4fd0

DrawSysRecord *drawSysCurrent()
{
    return sCurrent;
}

// A name's tags, the last first: "name_a_b" gives "ba". A tag is one letter
// after an underscore; the walk stops at the first thing that is not one.
// FS2 0x002d54a0

static void parseNameTags(char *tags, int size, const char *name)
{
    const char *cursor = name + strlen(name);
    if (cursor - name < 3) {
        *tags = '\0';
        return;
    }
    const char *last = tags + size - 1;
    char pending = '\0';
    for (;;) {
        cursor--;
        if (pending == '\0') {
            pending = *cursor;
            if (pending == '_')
                break;
        } else {
            if (*cursor != '_')
                break;
            *tags++ = pending;
            pending = '\0';
        }
        // PC: <= where the ROM has <, which reads the byte before the name.
        if (cursor <= name || last <= tags)
            break;
    }
    *tags = '\0';
}

// Is the texture one of the plants? The ROM's search is the MSL lower_bound
// instance over the table.
// FS2 0x002da220
// callcheck: strcmp

static bool isFoliageTexture(const char *name)
{
    const char *const *end = kFoliageTextures + sizeof(kFoliageTextures) / sizeof(kFoliageTextures[0]);
    const char *const *found = std::lower_bound(kFoliageTextures, end, name,
        [](const char *entry, const char *key) { return strcmp(entry, key) < 0; });
    return found != end && strcmp(*found, name) == 0;
}

// A new model joins the current record. Its name picks the far pass or the
// main one and a few flags; the same pass over its materials finds the water
// surfaces and the plants.
// FS2 0x002d5140
// callcheck: strcasecmp, strcmp, strcmp, strcasecmp, insert, strcmp, strcasecmp, strcasecmp, strcasecmp, strlen, strncasecmp, strlen (nameEqualsNoCase; insert is the vector insert, push_back here; the second strcmp is isFoliageTexture's)

void drawSysAddModel(ModelInstance *model)
{
    const ModelFileNode *node = model->node;
    const char *name = node->name;

    if (nameEqualsNoCase(name, kModelExtGhost)) {
        model->flags |= ModelFlag_Ghost;
        model->ghostParam0 = kGhostParam0;
        model->ghostParam1 = kGhostParam1;
    }
    if (strcmp(name, kModelTagged0) == 0)
        model->flags |= ModelFlag_TagC;
    if (strcmp(name, kModelTagged1) == 0) {
        model->flags &= ~ModelFlag_CastsShadow;
        model->flags |= ModelFlag_TagC;
    }

    char tags[kNameTagSpace];
    parseNameTags(tags, sizeof(tags), name);
    for (const char *tag = tags; *tag != '\0'; tag++)
        if (*tag == kNameTagC)
            model->flags |= ModelFlag_TagC;

    model->drawGroup = DrawSysGroup_Main;
    for (const auto &entry : kFarModels) {
        if (nameEqualsNoCase(name, entry.name)) {
            model->drawGroup = (u_char)entry.group;
            break;
        }
    }

    sCurrent->models.push_back(model);

    const size_t waveLength = strlen(kMaterialWave);
    for (int i = 0; i < node->surfaceCount; i++) {
        ModelSurface &surface = model->surfaces[i];
        const char *material = surface.material->name;
        if (nameCompareNoCase(material, kMaterialWave, waveLength) == 0) {
            surface.frame0 = 0;
            surface.frame1 = 1;
            surface.blendFix = atoi(material + waveLength);
        }
    }

    ModelFile *file = node->file;
    for (int i = 0; i < file->materialCount; i++) {
        ModelMaterial *material = file->materials.get()[i];
        if (material->kind == ModelMaterial_Textured && material->texture >= 0
            && isFoliageTexture(file->textureNames.get()[material->texture]))
            material->flags |= ModelMaterialFlag_Foliage;
    }

    model->flags |= ModelFlag_Frozen;
}

// Finds model and erases it from the record's list, when it is there.
// FS2 0x002d4fe0
// callcheck: FUN_002d5070, FUN_002d50a0, FUN_002d50c0, FUN_002d5110, FUN_002d5130 (the iterator helpers listed below: std::find and erase)

void drawSysRemoveModel(ModelInstance *model)
{
    std::vector<ModelInstance *> &models = sCurrent->models;
    auto found = std::find(models.begin(), models.end(), model);
    if (found != models.end())
        models.erase(found);
}
//   FS2 0x002d5070  vector erase (0x1927b0) through an iterator
//   FS2 0x002d50a0  iterator !=
//   FS2 0x002d50c0  std::find over ints
//   FS2 0x002d5110  vector end
//   FS2 0x002d5130  vector begin

// ──────────────────────────────────────────────────────────────────────
// The 1 / 20 scale view of the far passes: state.view = view scaled,
// state.viewScreen = that through the screen, state.screen = the screen.

static void setFarMatrices(DrawSysCamera *camera)
{
    sceVu0FMATRIX scale;
    sceVu0UnitMatrix(scale);
    scale[0][0] = kFarPassScale;
    scale[1][1] = kFarPassScale;
    scale[2][2] = kFarPassScale;
    sceVu0FMATRIX view;
    copyMatrix(view, camera->getView());
    sceVu0MulMatrix(gDrawSysState.view, scale, view);
    copyMatrix(gDrawSysState.screen, camera->getScreen());
    sceVu0MulMatrix(gDrawSysState.viewScreen, gDrawSysState.screen, gDrawSysState.view);
}

// What every pass but the last sets the same.
static void setPassState(SceneRender *render, DrawSysFog *fog)
{
    gDrawSysState.render = render;
    gDrawSysState.fog = fog;
    gDrawSysState._d0 = 1.0f;
    gDrawSysState._d4 = 1.0f;
    gDrawSysState._d8 = 7.0f;
    gDrawSysState.drawDistance = sDrawDistance;
    gDrawSysState.centerX = kScreenCenter;
    gDrawSysState.centerY = kScreenCenter;
}

// The frame's 3D draw (the passes: file header).
// FS2 0x002d5560
// callcheck: sceVu0UnitMatrix, FUN_0010ad90, FUN_0010ae28, FUN_0010aef0, FUN_0010af80, FUN_0010b208, FUN_002e8560, FlushCache, GsRegionAlloc, sceVif1PkCloseDmaTag, sceVif1PkCnt (dropped, file header: the frame GsRegionAlloc and its FUN_002e8560, FlushCache, the raw TEST_1 packets and the ref of the depth clear; sceVif1PkCall for flags, which no caller sets; the VU1 upload of SceneRender +0x80 and +0xf4 * 1.2, which waits for the SceneRender TU)

void drawSysRender(GamePacketManager *packets, const sceGsDrawEnv1 *env, int flags)
{
    (void)flags;
    DrawSysCamera *camera = sCurrent->camera;
    sShadowCount = drawSysBuildShadows(camera->getView());

    if (env != nullptr)
        gDrawSysState.env = *env;
    getDrawTarget()->env = env;

    sceVif1Packet *packet = packets->openList(PacketList_RenaFrame);
    renaBeginFrame(packet);
    renaSetSpotlight(packet, sSpotlightTexture, sSpotlightBlock);
    if (getStageSelect()->stage == kStageBoss)
        drawSysBuildBossShadowTexture(packet);
    else
        renaSetShadowTexture(packet, sShadowTexture, sShadowBlock);

    renaSetSpotRecord(gStageScene != nullptr && gStageScene->render != nullptr ? gStageScene->render->getSpot() : nullptr);

    gDrawSysState.width = getDrawTarget()->width;
    gDrawSysState.height = getDrawTarget()->height;
    renaSetLights(packet, sCurrent->lights);
    drawSysUploadShadows(packet);
    packets->closeList();

    // The far models, each with its fog.
    setFarMatrices(camera);
    setPassState(nullptr, sFogs[0]);
    renaSetShadowCount(packet, 0);

    packet = packets->openList(PacketList_RenaFar0);
    renaDrawModels(packet, sCurrent, DrawSysGroup_Far0);
    renaEndPass(packet);
    packets->closeList();

    for (int i = 1; i < kDrawSysFarPasses; i++) {
        gDrawSysState.fog = sFogs[i];
        packet = packets->openList(PacketList_RenaFar);
        renaDrawModels(packet, sCurrent, DrawSysGroup_Far0 << i);
        renaEndPass(packet);
        packets->closeList();
    }

    // The frame's models, on a fresh depth.
    packet = packets->openList(PacketList_RenaMain);
    copyMatrix(gDrawSysState.viewScreen, camera->getViewScreen());
    copyMatrix(gDrawSysState.view, camera->getView());
    copyMatrix(gDrawSysState.screen, camera->getScreen());
    setPassState(sSceneRender, sCurrent->lights->getFog());
    renaSetShadowCount(packet, sShadowCount);
    renaDrawModels(packet, sCurrent, DrawSysGroup_Main);
    renaEndPass(packet);
    packets->closeList();

    if (sTargetView) {
        packet = packets->openList(PacketList_RenaTarget);
        renaSetShadowCount(packet, 0);
        renaDrawModels(packet, sCurrent, DrawSysGroup_Target);
        renaEndPass(packet);
        packets->closeList();
    }
}

// ──────────────────────────────────────────────────────────────────────
// The boss's shadow target. In the ROM these write sprites a page wide, each
// with the XYOFFSET and SCISSOR of the size; a blit's source wraps (CLAMP_1
// REPEAT), which is how its first tap reaches before the texture's edge.

// Half the size, the taps along u.
// FS2 0x002d77d0

static void bossShadowBlitDown(sceVif1Packet *packet, int size)
{
    YamiPanShadowBlit blit = { size, size / 2, YamiPanShadowAxis_U, 1.0f, 1.0f, kBossShadowTaps };
    YamiPan_ShadowBlit(packet, &blit);
}

// The same size, the taps along v.
// FS2 0x002d7540

static void bossShadowBlitBlur(sceVif1Packet *packet, int size)
{
    YamiPanShadowBlit blit = { size, size, YamiPanShadowAxis_V, 0.5f, 0.5f, kBossShadowTaps };
    YamiPan_ShadowBlit(packet, &blit);
}

// Twice the size, the taps along u.
// FS2 0x002d72c0

static void bossShadowBlitUp(sceVif1Packet *packet, int size)
{
    YamiPanShadowBlit blit = { size, size * 2, YamiPanShadowAxis_U, 0.25f, 1.0f, kBossShadowTaps };
    YamiPan_ShadowBlit(packet, &blit);
}

// The ramp from the result's brightness to the shadow's alpha: white nothing,
// black half of full (0x80 is 1.0). The ROM builds it inline in the function below, as
// the CLUT (alpha in the top byte) on the scratchpad.
static void bossShadowBuildRamp(u_char *ramp)
{
    for (int i = 0; i < 0x100; i++) {
        int alpha = (int)((float)((0x100 - i) / 2) * 0.5f);
        ramp[i] = (u_char)std::max(std::min(alpha, kBossShadowAlphaMax), kBossShadowAlphaMin);
    }
}

// No host counterpart, YamiPan_ShadowEnd takes the result and the ramp:
// FS2 0x002d7030  bossShadowResolve: the result through a T4 read and a frame mask into a page that reads as 8 bits (T8H -> T8, BITBLTBUF in the function below)
// FS2 0x002d6dc0  bossShadowUploadClut: the ramp, swizzled, to the CLUT block

// The boss stage's shadow texture, every frame the boss is there
// (drawSysBuildBossShadow found it): the boss black on white from above its
// shadow, blurred down to 64 and back, through the alpha ramp. Leaves the
// frame's env as it was; the matrices are the next pass's to set.
// FS2 0x002d5e10
// callcheck: sceVif1PkCnt, sceVif1PkAddCode, FUN_0010afd0, FUN_0010b2d8, FUN_0010b668, FUN_0010b028, sceVif1PkAddCode, sceVif1PkCloseDmaTag, gsFillRect, sceVif1PkCnt, sceVif1PkAddCode, sceVif1PkCloseDmaTag, FlushCache, bossShadowResolve, sceVif1PkCnt, sceVif1PkOpenDirectCode, sceVif1PkOpenGifTag, sceVif1PkAddGsAD, sceVif1PkAddGsAD, sceVif1PkAddGsAD, sceVif1PkAddGsAD, sceVif1PkCloseGifTag, sceVif1PkCloseDirectCode, sceVif1PkCloseDmaTag, bossShadowUploadClut, FUN_002e8290, sceVif1PkCnt, FUN_0010afd0, FUN_0010b2d8, FUN_0010b668, FUN_0010b028, sceVif1PkCloseDmaTag, FUN_0010ae98, sceVif1PkCloseDmaTag (the GS env, frames and copies: file header; gsFillRect is YamiPan_ShadowBegin's clear, FUN_002e8290 its YamiPan_ShadowEnd; FUN_0010ae98 and the DISPFB2 debug view hang on gp-0x7958 / gp-0x7954, which nothing writes)

void drawSysBuildBossShadowTexture(sceVif1Packet *packet)
{
    ModelInstance *boss = sBossModel;
    if (boss == nullptr)
        return;

    gDrawSysState.render = nullptr;
    gDrawSysState.width = kBossShadowSize;
    gDrawSysState.height = kBossShadowSize;

    // A view straight down, centred on the shadow.
    const sceVu0FVECTOR &origin = gDrawSysShadows[0].matrix[3];
    sceVu0FMATRIX &view = gDrawSysState.view;
    static const sceVu0FVECTOR kViewX = {  1.0f, 0.0f, 0.0f, 0.0f };
    static const sceVu0FVECTOR kViewY = {  0.0f, 0.0f, 1.0f, 0.0f };
    static const sceVu0FVECTOR kViewZ = {  0.0f, -1.0f, 0.0f, 0.0f };
    memcpy(view[0], kViewX, sizeof(sceVu0FVECTOR));
    memcpy(view[1], kViewY, sizeof(sceVu0FVECTOR));
    memcpy(view[2], kViewZ, sizeof(sceVu0FVECTOR));
    view[3][0] = -origin[0];
    view[3][1] = origin[2] + kBossShadowHeight;
    view[3][2] = -origin[1];
    view[3][3] = 1.0f;

    // The screen, turned a quarter about x: the view's y is its depth.
    sceVu0FMATRIX &screen = gDrawSysState.screen;
    sceVu0ViewScreenMatrix(screen, kBossShadowScreenDistance, 1.0f, 1.0f, kScreenCenter, kScreenCenter,
                           1.0f, 0.0f, 0.0f, 1.0f);
    sceVu0FVECTOR row;
    memcpy(row, screen[1], sizeof(sceVu0FVECTOR));
    memcpy(screen[1], screen[2], sizeof(sceVu0FVECTOR));
    for (int lane = 0; lane < 4; lane++)
        screen[2][lane] = -row[lane];
    sceVu0MulMatrix(gDrawSysState.viewScreen, screen, view);

    YamiPan_ShadowBegin(packet, kBossShadowSize);

    // The boss alone, without its lights.
    u_short drawMode = boss->drawMode;
    u_char drawGroup = boss->drawGroup;
    sceVu0FMATRIX lightColors;
    memcpy(lightColors, boss->lightColors, sizeof(sceVu0FMATRIX));
    boss->drawMode = kBossShadowDrawMode;
    memset(boss->lightColors, 0, sizeof(sceVu0FMATRIX));
    boss->drawGroup = DrawSysGroup_BossShadow;
    renaDrawModels(packet, sCurrent, DrawSysGroup_BossShadow);
    boss->drawGroup = drawGroup;
    memcpy(boss->lightColors, lightColors, sizeof(sceVu0FMATRIX));
    boss->drawMode = drawMode;

    // Down to 64 and back to 256, a blur at each size but the last.
    int size = kBossShadowSize;
    do {
        int half = size >> 1;
        bossShadowBlitDown(packet, size);
        bossShadowBlitBlur(packet, half);
        if (half >= kBossShadowMinSize)
            size = half;
    } while (size > kBossShadowMinSize);
    while (size < kBossShadowSize) {
        int twice = size * 2;
        bossShadowBlitUp(packet, size);
        if (twice < kBossShadowSize)
            bossShadowBlitBlur(packet, twice);
        size = twice;
    }

    u_char ramp[0x100];
    bossShadowBuildRamp(ramp);
    YamiPan_ShadowEnd(packet, ramp);
}

// Each shadow's two matrices, from the inverse of its box: the texture
// projection (x, y to 0..1 over the box, a fade along z) and the clip.
// FS2 0x002d7a60
// callcheck: sceVu0InversMatrix, sceVu0UnitMatrix, sceVu0MulMatrix, sceVu0UnitMatrix, sceVu0MulMatrix, renaSetShadowMatrices

void drawSysUploadShadows(sceVif1Packet *packet)
{
    for (int i = 0; i < sShadowCount; i++) {
        DrawSysShadow &shadow = gDrawSysShadows[i];
        sceVu0FMATRIX inverse;
        sceVu0InversMatrix(inverse, shadow.matrix);

        sceVu0FMATRIX projection;
        sceVu0UnitMatrix(projection);
        float fade = -(128.0f / shadow.size[2]);
        projection[0][0] = 0.5f / shadow.size[0];
        projection[1][1] = 0.5f / shadow.size[1];
        projection[2][2] = fade;
        projection[2][3] = fade;
        projection[3][0] = 0.5f;
        projection[3][1] = 0.5f;
        projection[3][3] = 128.0f;
        sceVu0MulMatrix(projection, projection, inverse);

        sceVu0FMATRIX clip;
        sceVu0UnitMatrix(clip);
        clip[0][0] = 1.0f / shadow.size[0];
        clip[1][1] = 1.0f / shadow.size[1];
        clip[2][2] = 2.0f / shadow.size[2];
        clip[3][2] = -1.0f;
        sceVu0MulMatrix(clip, clip, inverse);

        renaSetShadowMatrices(packet, i, projection, clip);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002d7c00
// callcheck: operator_delete, operator_delete (the record's list, the record)

void drawSysPop()
{
    delete sCurrent;
    sCurrent = sSaved;
    sSaved = nullptr;
}

// A new record with the old one's camera and lights.
// FS2 0x002d7c70
// callcheck: operator_new, FUN_00241da0 (new DrawSysRecord: the reserve is its ctor)

void drawSysPush()
{
    sSaved = sCurrent;
    sCurrent = new DrawSysRecord;
    sCurrent->lights = sSaved->lights;
    sCurrent->camera = sSaved->camera;
}

// What the depth clear covers. The ROM's packet is a sprite in columns 64
// wide (the GS's page, to keep the texture cache out of it), Z 0, in a
// block on the heap; the passes ref it with TEST_1 ZB_ONLY.
// FS2 0x002d9760

static void drawSysBuildDepthClear(int width, int height, int x, int y)
{
    gDrawSysState.clearX = x;
    gDrawSysState.clearY = y;
    gDrawSysState.clearWidth = width;
    gDrawSysState.clearHeight = height;
}

// One directional light, as the leaf ctor and then drawSysInit set it.
static DrawSysDirLight *newDirLight(const sceVu0FVECTOR color, const sceVu0FVECTOR direction)
{
    DrawSysDirLight *light = new DrawSysDirLight;
    light->setColor(color);
    light->setDirection(direction);
    return light;
}

// One fog, from drawSysInit.
static DrawSysFog *newFog()
{
    static const sceVu0FVECTOR kBlack = { 0.0f, 0.0f, 0.0f, 1.0f };
    DrawSysFog *fog = new DrawSysFog;
    fog->setColor(kBlack);
    fog->setNear(500.0f);
    fog->setFar(2000.0f);
    fog->setNearValue(255.0f);
    fog->setFarValue(0.0f);
    return fog;
}

// The draw system: the record with its lights, the fogs, the camera, the
// frame's size and the depth clear.
// FS2 0x002d7cf0
// callcheck: operator_new, FUN_002d8dd0, FUN_002e2970, FUN_00241da0, FUN_002d91e0, free, setVector4 (FUN_002d8dd0 and FUN_002e2970 are the DrawSysAmbient and DrawSysCamera ctors; the reserve in the DrawSysRecord ctor; FUN_002d91e0 and setVector4 are the DrawSysDirLight ctor; free with the heap block of the depth clear, gone with its packet)

void drawSysInit(int originX, int originY, int width, int height, GsRegionAlloc *alloc)
{
    sCurrent = new DrawSysRecord;
    sCurrent->lights = new DrawSysLights;

    static const sceVu0FVECTOR kLight0Color = { 0.4f, 0.4f, 0.4f, 1.0f };
    static const sceVu0FVECTOR kLight0Direction = { 0.0f, 0.2f, 0.8f, 1.0f };
    static const sceVu0FVECTOR kLight1Color = { 0.3f, 0.3f, 0.3f, 1.0f };
    static const sceVu0FVECTOR kLight2Color = { 0.2f, 0.2f, 0.2f, 1.0f };
    static const sceVu0FVECTOR kLight12Direction = { 0.1f, 0.0f, 0.0f, 1.0f };
    static const sceVu0FVECTOR kAmbientColor = { 0.1f, 0.1f, 0.1f, 1.0f };
    sCurrent->lights->setLight(0, newDirLight(kLight0Color, kLight0Direction));
    sCurrent->lights->setLight(1, newDirLight(kLight1Color, kLight12Direction));
    sCurrent->lights->setLight(2, newDirLight(kLight2Color, kLight12Direction));

    DrawSysAmbient *ambient = new DrawSysAmbient;
    ambient->setColor(kAmbientColor);
    sCurrent->lights->setAmbient(ambient);

    sCurrent->lights->setFog(newFog());
    for (int i = 0; i < kDrawSysFogCount; i++)
        sFogs[i] = newFog();

    sCurrent->camera = new DrawSysCamera;
    sceVu0FMATRIX screen;
    sceVu0ViewScreenMatrix(screen, kScreenDistance, 1.0f, kScreenAspectY, kScreenCenter, kScreenCenter,
                           kScreenZMin, kScreenZMax, kScreenNear, kDefaultDrawDistance);
    sCurrent->camera->setScreenMatrix(screen);

    DrawTarget *target = getDrawTarget();
    target->originX = originX;
    target->originY = originY;
    target->width = width;
    target->height = height;

    sSpotlightTexture = nullptr;
    sShadowTexture = nullptr;
    sSceneRender = nullptr;
    sRegionAlloc = alloc;
    sReservedBlock = gfxGetTexArea()->alloc(kReservedBlocks, 0, 0);
    sDrawDistance = kDefaultDrawDistance;
    drawSysBuildDepthClear(width, height, originX, originY);
}

// ──────────────────────────────────────────────────────────────────────
// The classes. Their ctors and dtors in the ROM are a chain over the empty
// bases, which the host does not have:
//   FS2 0x002d8690  base of the camera, deleting dtor (vtable 0x31a418)
//   FS2 0x002d86e0  base of the camera, deleting dtor (vtable 0x31a460)
//   FS2 0x002d8730  light base dtor (vtable 0x31a4f0)
//   FS2 0x002d87f0  light base dtor (vtable 0x31a550)
//   FS2 0x002d8840  light base dtor (vtable 0x31a568)
//   FS2 0x002d8890  ambient base dtor (vtable 0x31a380)
//   FS2 0x002d8930  ambient base dtor (vtable 0x31a340)
//   FS2 0x002d8a40  light base dtor (vtable 0x31a200)
//   FS2 0x002d8b00  light base dtor (vtable 0x31a140)
//   FS2 0x002d8bc0  light base dtor (vtable 0x31a0f0)
//   FS2 0x002d8d10  lights base dtor (vtable 0x31a680)
//   FS2 0x002d8d60  lights base dtor (vtable 0x31a6b0)
//   FS2 0x002d8ec0  ambient base dtor (vtable 0x31a300)
//   FS2 0x002d8ff0  ambient base dtor (vtable 0x31a2c0)
//   FS2 0x002d90d0  ambient base dtor (vtable 0x31a2c0's base)
//   FS2 0x002d9270  light base dtor (vtable 0x31a1a0)
//   FS2 0x002d9330  light base dtor (vtable 0x31a140's base)
//   FS2 0x002d93f0  light base dtor (vtable 0x31a0a0)
//   FS2 0x002d9500  light base ctor, every vtable of the chain
//   FS2 0x002d9630  light base dtor (vtable 0x31a050)
//   FS2 0x002d9710  light base dtor (vtable 0x31a030)
// and the base's own copies of DrawSysLights' calls, on data at its +0:
//   FS2 0x002d9ec0  getAmbient
//   FS2 0x002d9ed0  setAmbient
//   FS2 0x002d9ee0  getLight
//   FS2 0x002d9ef0  setLight
//   FS2 0x002d9f10  getFog
//   FS2 0x002d9f20  setFog
// The base's default (0x2e2970 builds it with an aspect of 1.1666) and the
// identity view; drawSysInit replaces the screen at once.
// FS2 0x002e2970
// callcheck: sceVu0ViewScreenMatrix, sceVu0UnitMatrix

DrawSysCamera::DrawSysCamera()
{
    sceVu0ViewScreenMatrix(m_screen, kScreenDistance, 1.0f, 1.1666666f, kScreenCenter, kScreenCenter,
                           kScreenZMin, kScreenZMax, kScreenNear, kDefaultDrawDistance);
    sceVu0UnitMatrix(m_view);
    updateViewScreen();
}

void DrawSysCamera::updateViewScreen()
{
    sceVu0MulMatrix(m_viewScreen, m_screen, m_view);
}

// FS2 0x002d9ff0

void DrawSysCamera::setViewMatrix(const void *matrix)
{
    memcpy(m_view, matrix, sizeof(sceVu0FMATRIX));
    updateViewScreen();
}

// FS2 0x002da060

void DrawSysCamera::setScreenMatrix(const void *matrix)
{
    memcpy(m_screen, matrix, sizeof(sceVu0FMATRIX));
    updateViewScreen();
}

// FS2 0x002d91e0
// callcheck: FUN_002d9500 (the empty bases; the white default and forward direction are the leaf ctor, inline in drawSysInit)

DrawSysDirLight::DrawSysDirLight()
{
    static const sceVu0FVECTOR kWhite = { 1.0f, 1.0f, 1.0f, 1.0f };
    static const sceVu0FVECTOR kForward = { 0.0f, 0.0f, 1.0f, 1.0f };
    setEnabled(true);
    setColor(kWhite);
    setDirection(kForward);
}

// FS2 0x002d91a0

void DrawSysDirLight::setColor(const float *color)
{
    memcpy(m_color, color, sizeof(sceVu0FVECTOR));
}

// FS2 0x002d9190

void DrawSysDirLight::setDirection(const float *direction)
{
    memcpy(m_direction, direction, sizeof(sceVu0FVECTOR));
}

// FS2 0x002d8dd0
// callcheck: setEnabled, setVector4, setColor (the leaf ctor, inline in drawSysInit)

DrawSysAmbient::DrawSysAmbient()
{
    static const sceVu0FVECTOR kWhite = { 1.0f, 1.0f, 1.0f, 1.0f };
    setEnabled(true);
    setColor(kWhite);
}

// FS2 0x002d8db0

void DrawSysAmbient::setColor(const float *color)
{
    memcpy(m_color, color, sizeof(sceVu0FVECTOR));
}

DrawSysFog::DrawSysFog()
{
    memset(m_color, 0, sizeof(sceVu0FVECTOR));
}

// FS2 0x002d9be0

float DrawSysFog::getCoefficientA() const
{
    // PC: eeDivide: a fog with nothing set has far and near both 0.
    return (m_farValue + m_nearValue
            + eeDivide((m_farValue - m_nearValue) * (m_far + m_near), m_far - m_near)) / 2.0f;
}

// FS2 0x002d9c30

float DrawSysFog::getCoefficientB() const
{
    return eeDivide(m_far * m_near * (m_nearValue - m_farValue), m_far - m_near);
}

// FS2 0x002d9c70

int DrawSysFog::getFogValue(float z) const
{
    float a = getCoefficientA();
    float b = getCoefficientB();
    float nearValue = getNearValue();
    float farValue = getFarValue();
    float value = a + b / z;
    if (value < farValue)
        value = farValue;
    else if (nearValue < value)
        value = nearValue;
    return (int)value;
}

// FS2 0x001f8590

void DrawSysFog::setColor(const float *color)
{
    memcpy(m_color, color, sizeof(sceVu0FVECTOR));
}

// FS2 0x002da170

DrawSysAmbient *DrawSysLights::setAmbient(DrawSysAmbient *ambient)
{
    DrawSysAmbient *old = m_ambient;
    m_ambient = ambient;
    return old;
}

// FS2 0x002da1c0

DrawSysFog *DrawSysLights::setFog(DrawSysFog *fog)
{
    DrawSysFog *old = m_fog;
    m_fog = fog;
    return old;
}

// FS2 0x002d9aa0

float getDrawDistance()
{
    return sDrawDistance;
}

// FS2 0x002d9ab0

void setDrawDistance(float distance)
{
    sDrawDistance = distance;
}

// The MSL std::sort instance over the shadow keys (std::sort above):
//   FS2 0x002da2e0  quicksort
//   FS2 0x002da6f0  median of three
//   FS2 0x002da7f0  selection sort under 21 items

// The adjustor thunks that end the file (the host's compiler makes its own):
//   FS2 0x002da890  DrawSysLights, -0x18
//   FS2 0x002da8a0  DrawSysLights, -0x18
//   FS2 0x002da8b0  DrawSysLights, -0x18
//   FS2 0x002da8c0  DrawSysLights, -0x18
//   FS2 0x002da8d0  DrawSysLights, -0x18
//   FS2 0x002da8e0  DrawSysLights, -0x18
//   FS2 0x002da8f0  DrawSysLights, -0x18
//   FS2 0x002da900  DrawSysDirLight
//   FS2 0x002da910  DrawSysDirLight
//   FS2 0x002da920  DrawSysDirLight
//   FS2 0x002da930  DrawSysAmbient
//   FS2 0x002da940  DrawSysAmbient
//   FS2 0x002da950  DrawSysFog
//   FS2 0x002da960  DrawSysFog
//   FS2 0x002da970  DrawSysFog
//   FS2 0x002da980  DrawSysFog
//   FS2 0x002da990  DrawSysFog
//   FS2 0x002da9a0  DrawSysFog
//   FS2 0x002da9b0  DrawSysFog
//   FS2 0x002da9c0  DrawSysFog
//   FS2 0x002da9d0  DrawSysCamera, -0xd4
//   FS2 0x002da9e0  the ambient bases
//   FS2 0x002da9f0  the ambient bases
//   FS2 0x002daa00  the ambient bases
//   FS2 0x002daa10  DrawSysAmbient
//   FS2 0x002daa20  DrawSysAmbient
//   FS2 0x002daa30  the ambient bases
//   FS2 0x002daa40  the ambient bases
//   FS2 0x002daa50  the ambient bases
//   FS2 0x002daa60  DrawSysAmbient
//   FS2 0x002daa70  DrawSysAmbient
//   FS2 0x002daa80  the ambient bases
//   FS2 0x002daa90  the ambient bases
//   FS2 0x002daaa0  the ambient bases
//   FS2 0x002daab0  the ambient bases
//   FS2 0x002daac0  the light bases
//   FS2 0x002daad0  the light bases
//   FS2 0x002daae0  the light bases
//   FS2 0x002daaf0  the light bases
//   FS2 0x002dab00  DrawSysDirLight
//   FS2 0x002dab10  DrawSysDirLight
//   FS2 0x002dab20  the light bases
//   FS2 0x002dab30  the light bases
//   FS2 0x002dab40  the light bases
//   FS2 0x002dab50  the light bases
//   FS2 0x002dab60  the light bases
//   FS2 0x002dab70  the light bases
//   FS2 0x002dab80  the light bases
//   FS2 0x002dab90  DrawSysDirLight
//   FS2 0x002daba0  DrawSysDirLight
//   FS2 0x002dabb0  the light bases
//   FS2 0x002dabc0  the light bases
//   FS2 0x002dabd0  the light bases
//   FS2 0x002dabe0  the light bases
//   FS2 0x002dabf0  DrawSysDirLight
//   FS2 0x002dac00  DrawSysDirLight
//   FS2 0x002dac10  the light bases
//   FS2 0x002dac20  the light bases
//   FS2 0x002dac30  the light bases
//   FS2 0x002dac40  the light bases
//   FS2 0x002dac50  the light bases
//   FS2 0x002dac60  the light bases

// Inline in drawsys.h:
//   FS2 0x002d8dc0  DrawSysAmbient::setEnabled
//   FS2 0x002d91b0  setVector4 (the lane copy of the ctors)
//   FS2 0x002d91d0  DrawSysDirLight::setEnabled
//   FS2 0x002d9ac0  DrawSysFog::~DrawSysFog
//   FS2 0x002d9ba0  DrawSysFog::getNear
//   FS2 0x002d9bb0  DrawSysFog::getFar
//   FS2 0x002d9bc0  DrawSysFog::getNearValue
//   FS2 0x002d9bd0  DrawSysFog::getFarValue
//   FS2 0x002d9d40  DrawSysFog::getColor
//   FS2 0x002d9d50  DrawSysFog::isEnabled
//   FS2 0x002d9d60  DrawSysAmbient::~DrawSysAmbient
//   FS2 0x002d9e00  DrawSysDirLight::~DrawSysDirLight
//   FS2 0x002d9f30  DrawSysCamera::~DrawSysCamera
//   FS2 0x002d9fc0  DrawSysCamera::getViewScreen
//   FS2 0x002d9fd0  DrawSysCamera::getView
//   FS2 0x002d9fe0  DrawSysCamera::getScreen
//   FS2 0x002da0d0  DrawSysLights::~DrawSysLights
//   FS2 0x002da160  DrawSysLights::getAmbient
//   FS2 0x002da180  DrawSysLights::getLight
//   FS2 0x002da190  DrawSysLights::setLight
//   FS2 0x002da1b0  DrawSysLights::getFog
//   FS2 0x002da1d0  DrawSysDirLight::isEnabled
//   FS2 0x002da1e0  DrawSysAmbient::isEnabled
//   FS2 0x002da1f0  DrawSysDirLight::getDirection
//   FS2 0x002da200  DrawSysDirLight::getColor
//   FS2 0x002da210  DrawSysAmbient::getColor
