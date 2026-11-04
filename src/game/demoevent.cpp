/* ==========================================================================
 *  game/demoevent.cpp
 *
 *  What starts and frames a scripted event, from the stage's side:
 *    StageFade (0x1512a0..0x1516f0): the full-screen fade every event waits on.
 *    DemoCamera and the pack lookup (0x26c700..0x26d020): the camera of a
 *      sub-scene. Its model is the pack's Model_camera.mdl; each cut is a
 *      motion, and the bones bn_fovy / bn_length and bn_top are the lens and
 *      the view.
 *    StageMission (0x2867a0..0x287ae0): the stage's mission state, among it
 *      the name of the event the scripts ask for (afterScripts starts it).
 *    StageEvent (0x287af0..0x288c00): StageFileEvent reads
 *      demoEU/s<stage>/<name>.edp, its voice and subtitles, and asks the
 *      stage loop for a sub-scene; StageSimpleEvent plays an entry of the
 *      stage's own script.
 *
 *  The order on the way in: a script native (0x1f2, 0x155, 0x15f..0x162 in
 *  game/scriptrunner.cpp) -> StageScene::startFileEvent -> poll each frame in
 *  runStageScripts -> requestSubScene -> stageThread builds a scene over the
 *  same files -> enterSubScene(getSubSceneData()).
 *
 *  The file name is the port's (no source string), and joining the three
 *  runs is a choice: nothing but the event ties them.
 * ======================================================================== */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "game/clock.h"
#include "game/fileloader.h"
#include "game/gamehooks.h"
#include "game/gamepacket.h"
#include "game/gamesystem.h"
#include "game/menupack.h"
#include "game/modelpose.h"
#include "game/scriptrunner.h"
#include "game/stageloop.h"
#include "game/stageobjlist.h"
#include "game/stageresult.h"
#include "game/stagescene.h"
#include "game/stagesound.h"
#include "game/sysdata.h"
#include "game/thread.h"
#include "game/vu0math.h"
#include "sdk/libgraph.h"
#include "sound/sound.h"
#include "yamipan/rendering/yamipan_drawui.h"

// ──────────────────────────────────────────────────────────────────────
// A flat rect in the frame's pixels, blended. PC: the ROM writes the sprite's
// GIF packet (0x2eaeb0's neighbours are the packet library's); the words stay
// as the GS would get them, wrapping included: StageFade's rect starts at
// 0x700, so its far corner wraps past 0xffff to 0 and the sprite covers the
// whole window, which is how the game's fade fills the screen.

constexpr u_int kFillOrigin = 0x700;
constexpr u_int kFillPrim   = 0x146;        // SPRITE, ABE, FST
constexpr u_int kFillZ      = 0xff0fffff;

// FS2 0x002eaeb0
// callcheck: sceVif1PkCnt, sceVif1PkAddCode, sceVif1PkOpenDirectCode, sceVif1PkOpenGifTag, sceVif1PkAddGsData, sceVif1PkAddGsData, sceVif1PkAddGsData, sceVif1PkAddGsData, sceVif1PkCloseGifTag, sceVif1PkCloseDirectCode, sceVif1PkCloseDmaTag (PC: YamiPan_DrawUiPrim, YamiPan_DrawUiBegin, YamiPan_DrawUiVertex)

void gsFillRect(sceVif1Packet *packet, int x, int y, int x2, int y2, u_int rgba)
{
    const u_char rgbaq[4] = {
        (u_char)rgba, (u_char)(rgba >> 8), (u_char)(rgba >> 16), (u_char)(rgba >> 24),
    };
    YamiPan_DrawUiPrim(packet, kFillPrim, rgbaq);
    YamiPan_DrawUiBegin(packet, 0);
    YamiPanUiVertex corner = {};
    corner.z = kFillZ;
    corner.x = (u_short)((x + kFillOrigin) * 0x10);
    corner.y = (u_short)((y + kFillOrigin) * 0x10);
    YamiPan_DrawUiVertex(packet, &corner);
    corner.x = (u_short)((x2 + kFillOrigin) * 0x10);
    corner.y = (u_short)((y2 + kFillOrigin) * 0x10);
    YamiPan_DrawUiVertex(packet, &corner);
}

// ──────────────────────────────────────────────────────────────────────
// The fade.

constexpr u_long kFadeAlpha = 0x44;     // ALPHA_1: the fill over the frame
constexpr float  kFadeAlphaScale = 128.0f;

// FS2 0x00151600
// callcheck: ~Clock, createChild

StageFade::StageFade(int x, int y, int w, int h, Clock *clock)
    : m_x(x), m_y(y), m_w(w), m_h(h)
{
    if (clock == nullptr) {
        m_clockShared = false;
        clock = gGameSystem->m_clock.createChild();
    } else {
        m_clockShared = true;
    }
    m_clock = clock;
}

// FS2 0x00151550
// callcheck: ~Clock, operator_delete

StageFade::~StageFade()
{
    if (m_clock != nullptr && !m_clockShared)
        delete m_clock;
}

// FS2 0x001512a0

bool StageFade::isFullInMode2()
{
    return m_mode == StageFade_Done && m_target == 1.0f;
}

// FS2 0x001512e0

bool StageFade::isFullInMode1()
{
    return m_mode == StageFade_Running && m_target == 1.0f;
}

// FS2 0x00151330
// callcheck: resetTime

void StageFade::start(float seconds, float target, float from)
{
    m_mode = StageFade_Running;
    m_target = target;
    if (from >= 0.0f) {
        m_from = from;
        m_level = from;
    } else {
        m_from = m_level;
    }
    m_seconds = seconds;
    m_clock->resetTime();
}

// FS2 0x001513a0 (0x151390 the slot's thunk)
// callcheck: getSeconds, gsFillRect

void StageFade::draw(sceVif1Packet *packet, TexAreaList *)
{
    if (m_mode == StageFade_Idle)
        return;

    float time = m_clock->getSeconds();
    if (time < m_seconds) {
        m_level = (time / m_seconds) * (m_target - m_from) + m_from + 0.0f;
    } else {
        m_mode = StageFade_Done;
        m_level = m_target;
    }

    float level = m_level;
    if (level < 0.0f)
        level = 0.0f;
    if (level > 1.0f)
        level = 1.0f;
    m_drawn = level;
    if (level != 0.0f) {
        YamiPan_DrawUiReg(packet, SCE_GS_ALPHA_1, kFadeAlpha);
        gsFillRect(packet, m_x, m_y, m_x + m_w, m_y + m_h,
                   m_colour | ((u_int)(level * kFadeAlphaScale) << 24));
    }
}

// ──────────────────────────────────────────────────────────────────────
// An .edp pack: names and data by offsets from the file's start. The script's
// index is apart.
// EE layout.
struct SubScenePackEntry
{
    int nameOffset;             // FS2 +0x0
    int dataOffset;             // FS2 +0x4
};
struct SubScenePack
{
    u_char _00[0xa];
    short  count;               // FS2 +0x0a
    short  scriptIndex;         // FS2 +0x0c: negative: no script
    u_char _0e[0x2];
    SubScenePackEntry entries[1]; // FS2 +0x10
};
static_assert(sizeof(SubScenePackEntry) == 8, "SubScenePackEntry: EE 8");
static_assert(offsetof(SubScenePack, count) == 0xa, "SubScenePack::count");
static_assert(offsetof(SubScenePack, entries) == 0x10, "SubScenePack::entries");

// sprintf out, then the first entry of that name.
static void *findPackFile(void *file, const char *format, const char *name)
{
    char wanted[0x100];
    sprintf(wanted, format, name);
    char *base = (char *)file;
    const SubScenePack *pack = (const SubScenePack *)file;
    for (int i = 0; i < pack->count; i++) {
        if (strcmp(base + pack->entries[i].nameOffset, wanted) == 0)
            return base + pack->entries[i].dataOffset;
    }
    return nullptr;
}

// FS2 0x0026c700
// callcheck: sprintf, strcmp

void *SubSceneData::findModel(const char *name)
{
    return findPackFile(pack, "%s.mdl", name);  /* data 0x308a00 */
}

// FS2 0x0026c7c0
// callcheck: sprintf, strcmp

void *SubSceneData::findMotion(const char *name)
{
    return findPackFile(pack, "%s.mtn", name);  /* data 0x308a08 */
}

// FS2 0x0026c880

StageScript *SubSceneData::getScript()
{
    const SubScenePack *file = (const SubScenePack *)pack;
    if (file->scriptIndex < 0)
        return nullptr;
    return (StageScript *)((char *)pack + file->entries[file->scriptIndex].dataOffset);
}

// FS2 0x0026c8c0
// callcheck: ScriptRunner

DemoExecuter::DemoExecuter(bool skippable) : m_skippable(skippable)
{
}

// ──────────────────────────────────────────────────────────────────────
// The demo camera.

constexpr float kDemoScrZ     = 450.0f;
constexpr float kDemoScreen   = 512.0f;
constexpr float kDemoFarZ     = 1600.0f;
// The motion's speed in the game's frames. 1.2 is 60 / 50: the European
// build's fields (a guess at why).
constexpr float kDemoCutSpeed = 1.2f;
constexpr float kDegToRad     = 3.1415927f / 180.0f;
constexpr u_int kDemoCutFlags = 0x50;

static const char kDemoCameraModel[] = "Model_camera";  /* data 0x308a30 */
static const char kBoneFovy[]        = "bn_fovy";       /* data 0x308a10 */
static const char kBoneLength[]      = "bn_length";     /* data 0x308a20 */
static const char kBoneTop[]         = "bn_top";        /* data 0x308a18 */

// FS2 0x0026ce20
// callcheck: StageCamera, StringMap, setModel, getSubSceneBlock, findModel, setBaseMotion, bindBones, placeBones, setDirty, ~StringMap

DemoCamera::DemoCamera() : StageCamera(kDemoScrZ, kDemoScreen, kDemoScreen, kDemoFarZ)
{
    StringMap<Tim2Texture *> textures;
    setModel(nullptr, &textures);
    void *file = gStageScene->getSubSceneBlock()->findModel(kDemoCameraModel);
    if (file != nullptr) {
        m_model->setBaseMotion(file, 0);
        m_model->bindBones();
        m_model->placeBones(this);
    }
    setDirty(false);
}

// FS2 0x0026cd30 (0x26cf40 the base's copy, 0x26d010 the thunk)
// callcheck: 0x2052d0

DemoCamera::~DemoCamera()
{
    if (m_ownsFilm)
        demoFilmDestroy(m_film);
}

// FS2 0x0026cb70
// callcheck: playMotionFile

void DemoCamera::playCut(void *motionFile)
{
    m_model->playMotionFile(motionFile, false, false, kDemoCutFlags, 0);
}

// FS2 0x0026ca40 (0x26d000 the thunk)
// callcheck: getSubSceneBlock, findMotion, playCut, onViewTop

void DemoCamera::playCutNamed(const char *name)
{
    void *file = gStageScene->getSubSceneBlock()->findMotion(name);
    if (file != nullptr) {
        playCut(file);
        StageCamera::onViewTop();
    }
}

// FS2 0x0026cb90 (0x26cff0 the thunk)
// callcheck: advance, findBone, findBone, getValue, tanf, setScrZ, findBone, copyMatrix, getMatrix, copyMatrix

void DemoCamera::cameraStep()
{
    m_model->advance(kDemoCutSpeed, m_xform.position, m_xform.rotation, 0);

    ModelPose *pose = m_model->pose;
    ModelPoseBone *lens = pose->findBone(kBoneFovy);
    if (lens == nullptr)
        lens = pose->findBone(kBoneLength);
    // PC: the ROM reads every bone unguarded. ModelPose::load is a stub, so a
    // pose has no bones yet and the camera keeps its own place.
    if (lens == nullptr)
        return;
    float angle = lens->getValue();
    setScrZ(m_screenHeight / tanf(angle * kDegToRad));

    ModelPoseBone *top = pose->findBone(kBoneTop);
    if (top == nullptr)
        return;

    // The bone's rows to the view's: y and z swap, z turns over.
    sceVu0FMATRIX view;
    static const int kRowOf[4] = { 2, 0, 1, 3 };
    for (int row = 0; row < 4; row++) {
        const sceVu0FVECTOR &from = top->matrix[kRowOf[row]];
        view[row][0] = from[0];
        view[row][1] = from[2] * -1.0f;
        view[row][2] = from[1];
        view[row][3] = from[3];
    }
    copyMatrix(m_xform.world, view);
}

// The film data's two parts, from the parent's party.
// FS2 0x0026cab0
// callcheck: 0x1fea40, 0x1fea30

void DemoCamera::setOverlay(int overlay)
{
    if ((overlay == DemoOverlay_Film || overlay == DemoOverlay_FilmDark) && m_film == nullptr)
        demoFilmTake(gStageScene->parent->party, &m_film, &m_flecks);
    m_overlay = overlay;
}

// FS2 0x0026c920
// callcheck: openList, 0x2a11d0, 0x2a11c0, 0x204550, closeList, gsFillRect

void DemoCamera::drawOverlay()
{
    if (m_overlay == DemoOverlay_None)
        return;
    constexpr float kFilmLevel = 0.5f;
    constexpr u_long kDarkTest = 0x30000;
    constexpr u_int kDarkFill = 0x20000080;
    constexpr int kFrame = 0x200;

    sceVif1Packet *packet = gGameSystem->m_packets.openList(PacketList_Scene2d);
    if (m_flecks != nullptr) {
        demoFleckStep(m_flecks);
        demoFleckDraw(kFilmLevel, m_flecks, packet);
    }
    demoFilmDraw(kFilmLevel, m_film, packet);
    if (m_overlay == DemoOverlay_FilmDark) {
        YamiPan_DrawUiReg(packet, SCE_GS_TEST_1, kDarkTest);
        gsFillRect(packet, 0, 0, kFrame, kFrame, kDarkFill);
    }
    gGameSystem->m_packets.closeList();
}

// ──────────────────────────────────────────────────────────────────────
// The mission.

constexpr float kMissionValue      = 2000.0f;     // 0x44fa0000
constexpr int   kMission_b0        = 0x800000;
constexpr float kBlockRange        = 400.0f;      // 0x43c80000
constexpr float kPendingFade       = 0.5f;
constexpr int   kSpawnKept         = 2;
constexpr float kFullBlack         = 1.0f;
constexpr float kFullClear         = 0.0f;

// The VISIBILITY_ lists, by the map's object names. data 0x2fb480
static const char *const kVisibilityNames[kMissionVisibilities] = {
    "VISIBILITY_MASK", "VISIBILITY_01", "VISIBILITY_02", "VISIBILITY_03",
};
// Default move scales by action. data 0x2fb490
static const float kMoveScales[6] = { 1.1f, 1.0f, 0.8f, 1.25f, 1.15f, 0.9f };
static const char kBurnerName[] = "I_HATUEN";   /* data 0x3038a8 */
static const char kRootName[]   = "Root";       /* data 0x309180 */

// FS2 0x00287900
// callcheck: reset

StageMission::StageMission()
{
    memset(timed, 0, sizeof(timed));
    memset(spawnPoints, 0, sizeof(spawnPoints));
    notice.enabled = 1;
    reset();
}

// FS2 0x00287690
// callcheck: operator_delete, operator_new, MessageTable

void StageMission::reset()
{
    defaultMoveScale = 0.0f;
    memcpy(moveScale, kMoveScales, sizeof(moveScale));
    value = kMissionValue;
    scriptVar3e = 0;
    scriptVar3f = 0;
    _b0 = kMission_b0;
    purposeText[0] = 0;
    for (int i = 0; i < kMissionTimedRecords; i++)
        timed[i].id = -1;
    timedCount = 0;
    pendingEvent[0] = '\0';
    _d8 = 0;
    _d9 = 0;
    blackout = 0;
    blockRange = kBlockRange;
    delete messages;
    messages = new MessageTable;
    for (int i = 0; i < kMissionSpawnPoints; i++)
        spawnPoints[i].state = 0;
}

// FS2 0x00287820
// callcheck: operator_delete, operator_delete[] x6

StageMission::~StageMission()
{
    delete messages;
    delete[] characters;
    delete[] classBlock;
    delete[] thrownItems.objects;
    delete[] burners.objects;
    delete[] trapItems.objects;
    delete[] scriptObjects;
}

// FS2 0x002867a0

void StageMission::setSpawnPoint(int id, const float *pos, const float *rot)
{
    if (spawnCount < id + 1)
        spawnCount = id + 1;
    MissionSpawnPoint &point = spawnPoints[id];
    if (point.state == 0) {
        memcpy(point.position, pos, sizeof(point.position));
        point.state = kSpawnKept;
        point.rotation[0] = rot[0];
        point.rotation[1] = rot[1];
        point.rotation[2] = rot[2];
        point.rotation[3] = 1.0f;
    }
}

// FS2 0x00286820
// callcheck: strcmp

void StageMission::setVisibilityRange(float range, const char *name)
{
    for (int i = 0; i < kMissionVisibilities; i++) {
        if (strcmp(kVisibilityNames[i], name) == 0) {
            visibility[i].range = range;
            return;
        }
    }
}

// FS2 0x002868b0
// callcheck: strcpy

void StageMission::setPendingEvent(const char *name)
{
    strcpy(pendingEvent, name);
}

// FS2 0x002868c0
// callcheck: 0x29f2a0

void StageMission::onRestore()
{
    if (!restoreAsked) {
        restoreAsked = true;
        restoreAnswer = missionRestoreCheck();
    }
}

// FS2 0x00286910

void StageMission::setRestore(void *arg, StageObject *object)
{
    if (arg != nullptr)
        restoreAsked = false;
    restoreArg = arg;
    keptObject = object;
}

// FS2 0x00286c20
// callcheck: find

const char *StageMission::getItemText(const char *name)
{
    return messages->find(name);
}

// FS2 0x00286c30
// callcheck: setFile

void StageMission::setMessages(void *file)
{
    messages->setFile(file);
}

// FS2 0x00286c40
// callcheck: getMenuMode, start, setMuted, startFileEvent

void StageMission::afterScripts()
{
    StageScene *scene = gStageScene;
    if (scene->getMenuMode() == 1) {
        if (pendingEvent[0] != '\0') {
            scene->sound->setMuted(true);
            scene->startFileEvent(0.0f, pendingEvent);
            pendingEvent[0] = '\0';
        } else if (scene->event == nullptr) {
            scene->fade->start(kPendingFade, kFullClear, kFullBlack);
        }
    }
    _d8 = 0;
    _d9 = 0;
}

// The timed records: the ROM's thunk 0x286ce0 over 0x286e30.
// FS2 0x00286e30
// callcheck: 0x29f810, 0x29fd10, 0x29fb60

void StageMission::tickPending()
{
    int done = 0;
    int seen = 0;
    int live = timedCount;
    for (int i = 0; seen < live && i < kMissionTimedRecords; i++) {
        MissionTimedRecord &record = timed[i];
        if (record.id == -1)
            continue;
        record.frames--;
        if (record.frames < 1) {
            int point = missionPickSpawnPoint(record.position, record.arg);
            if (!missionSpawnPointTaken(&point))
                missionSpawnAt(&point, record.id);
            record.id = -1;
            done++;
        }
        seen++;
    }
    timedCount -= done;
}

// FS2 0x00287050
// callcheck: getStageResult

void StageMission::setResultValue(int index, int value)
{
    getStageResult()->attempt[index] = value;
}

// FS2 0x00287090
// callcheck: getStageResult

int StageMission::getResultValue(int index)
{
    return getStageResult()->attempt[index];
}

// FS2 0x002870c0
// callcheck: getSeconds

void StageMission::markStart()
{
    startTime = gStageScene->clock->getSeconds();
}

// A list of `count` objects from a mask over one group's array.
template <typename Test>
static void walkGroup(StageObjArray &list, Test test)
{
    for (StageObject **slot = list.begin(); slot != nullptr; slot = StageObjArray::next(slot))
        test(*slot);
}

// FS2 0x002870f0
// callcheck: findAll7, operator_new[] x3, getKind, strcmp, getKind, operator_new[] x3, getCharacterClass, strcmp, operator_new[], strcmp

void StageMission::setup()
{
    StageObjectList *objects = gStageScene->objects;

    for (int i = 0; i < kMissionVisibilities; i++) {
        visibility[i].objects = objects->findAll7(kVisibilityNames[i]);
        visibility[i].range = -1.0f;
    }

    // Group 3's thrown and trap items, the burners apart.
    StageObjArray &items = objects->m_lists[StageObjGroup_3];
    int thrown = 0, traps = 0, burning = 0;
    walkGroup(items, [&](StageObject *object) {
        if (object->getKind() == StageObject_ThrownItem) {
            thrown++;
            if (strcmp(object->m_name, kBurnerName) == 0)
                burning++;
        } else if (object->getKind() == StageObject_TrapItem) {
            traps++;
        }
    });
    trapItems.count = traps;
    burners.count = burning;
    thrownItems.count = thrown;
    burners.objects = new StageObject *[burners.count];
    thrownItems.objects = new StageObject *[thrownItems.count];
    trapItems.objects = new StageObject *[trapItems.count];
    thrown = traps = burning = 0;
    walkGroup(items, [&](StageObject *object) {
        if (object->getKind() == StageObject_ThrownItem) {
            thrownItems.objects[thrown++] = object;
            if (strcmp(object->m_name, kBurnerName) == 0)
                burners.objects[burning++] = object;
        } else if (object->getKind() == StageObject_TrapItem) {
            trapItems.objects[traps++] = object;
        }
    });

    // The characters by their numbers.
    StageObjArray &people = objects->m_lists[StageObjGroup_Characters];
    characterCount = people.m_count;
    characters = new StageObject *[characterCount];
    for (int i = 0; i < characterCount; i++)
        characters[i] = nullptr;
    walkGroup(people, [&](StageObject *object) {
        characters[object->m_id - 1] = object;
    });

    // Each class mask's characters, a row of the block with nulls behind.
    classBlock = new StageObject *[characterCount * kMissionClassLists];
    for (int mask = 0; mask < kMissionClassLists; mask++) {
        StageObject **row = classBlock + mask * characterCount;
        byClass[mask].objects = row;
        int count = 0;
        for (int i = 0; i < characterCount; i++) {
            StageObject *object = characters[i];
            if ((mask & object->getCharacterClass()) != 0)
                row[count++] = object;
        }
        byClass[mask].count = count;
        for (int i = count; i < characterCount; i++)
            row[i] = nullptr;
    }

    // What runScripts steps: the running objects, Root first, every
    // character.
    int count = 0;
    bool hasRoot = false;
    for (int group = 0; group < StageObjGroup_Count; group++) {
        StageObjArray &list = objects->m_lists[group];
        if (group == StageObjGroup_Characters) {
            count += list.m_count;
            continue;
        }
        walkGroup(list, [&](StageObject *object) {
            if (object->isRunning()) {
                if (strcmp(object->m_name, kRootName) == 0)
                    hasRoot = true;
                count++;
            }
        });
    }
    scriptObjectCount = count;
    scriptObjects = new StageObject *[scriptObjectCount];
    if (count > 0) {
        int next = hasRoot ? 1 : 0;
        scriptObjects[0] = nullptr;
        for (int group = 0; group < StageObjGroup_Count; group++) {
            walkGroup(objects->m_lists[group], [&](StageObject *object) {
                if (group == StageObjGroup_Characters || object->isRunning()) {
                    if (strcmp(object->m_name, kRootName) == 0)
                        scriptObjects[0] = object;
                    else
                        scriptObjects[next++] = object;
                }
            });
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// The event's voice: a SoundStream with a directory and subtitles. setFont and
// drawSubtitles are still stubs, so no subtitle shows.

constexpr int kDemoVoiceBuffer = 0x10000;
static const char kFsmMagic[] = "fsm";

// FS2 0x002b1490
// callcheck: SoundStream, operator_delete[]

DemoStream::DemoStream()
{
}

// FS2 0x002b13e0
// callcheck: closeBank, operator_delete[] x2, ~SoundStream

DemoStream::~DemoStream()
{
    closeBank();
    delete[] m_dir;
    delete[] m_path;
}

// FS2 0x002b1370
// callcheck: operator_delete[], strlen, operator_new[], strcpy

void DemoStream::setDir(const char *dir)
{
    delete[] m_dir;
    m_dir = nullptr;
    if (dir != nullptr) {
        m_dir = new char[strlen(dir) + 1];
        strcpy(m_dir, dir);
    }
}

// FS2 0x002b1140

bool DemoStream::isPlaying()
{
    return m_state == SoundStream_Playing || m_state == SoundStream_Paused;
}

// FS2 0x002b1170
// callcheck: stop

int DemoStream::stop()
{
    int position = SoundStream::stop(false);
    return position < 0 ? -1 : position;
}

// FS2 0x002b1100

void DemoStream::update()
{
    SoundStream::update();
}

// FS2 0x002b11a0
// callcheck: fsmRelocate

bool DemoStream::setFsm(void *fsm)
{
    if (fsm == nullptr || memcmp(fsm, kFsmMagic, sizeof(kFsmMagic)) != 0)
        return false;
    fsmRelocate(fsm);
    m_fsm = fsm;
    return true;
}

// FS2 0x002b1270
// callcheck: operator_delete[], strlen, strlen, operator_new[], sprintf, setBuffer, openBankFile

bool DemoStream::open(const void *header, const char *name)
{
    delete[] m_path;
    m_path = nullptr;
    if (name == nullptr)
        return false;
    m_path = new char[(m_dir != nullptr ? strlen(m_dir) : 0) + strlen(name) + 1];
    sprintf(m_path, "%s%s", m_dir != nullptr ? m_dir : "", name);   /* data 0x30b378, 0x30b380 */
    m_header = header;
    setBuffer(kDemoVoiceBuffer, nullptr);
    return openBankFile(m_header, m_path) >= 0;
}

// FS2 0x002b1240
// callcheck: playVolume

bool DemoStream::play(int volume)
{
    return playVolume(0, 0, volume, volume) >= 0;
}

// FS2 0x002b1110
// callcheck: pause

bool DemoStream::setPaused(bool on)
{
    pause(on);
    return on;
}

// ──────────────────────────────────────────────────────────────────────
// The events.

// Is the language one the first subtitle font serves (its set of
// getLanguage's values)?
static bool languageUsesGameFont()
{
    getSaveData();
    int language = getLanguage();
    return language == 1 || language == 5 || language == 6 || language == 7
        || language == 8 || language == 9;
}

constexpr int kSoundSettleFrames = 0x18;
constexpr int kFlagGroupFirstStage = 1;     // SaveData::testStageFlags
constexpr int kFlagWatchedOpening  = 0x15;
static const char kEventOpening[] = "d0101";    /* data 0x309188 */

// FS2 0x00287af0
// callcheck: getSaveData, getLanguage

bool StageEvent::usesLanguageFont()
{
    return languageUsesGameFont();
}

// FS2 0x00287b70
// callcheck: strcmp, getSystemData, testStageFlags

bool StageEvent::isSkippable()
{
    if (strcmp(name, kEventOpening) != 0)
        return true;
    return getSystemData()->save.testStageFlags(kFlagGroupFirstStage, kFlagWatchedOpening);
}

// The voice's file, without an extension: 2 figures each from d<aa><bb>[a-e]
// with the set's suffix; null for a name of another shape.
// FS2 0x00288930
// callcheck: getSaveData, getFileSuffix, sscanf, sscanf, sprintf, sprintf

static const char *voiceFileName(char *out, const char *name)
{
    const char *suffix = getFileSuffix(getSystemData()->options._04);
    int stage, number;
    char letter = -1;
    int fields = sscanf(name, "d%2d%2d%c", &stage, &number, &letter);   /* data 0x3091c8 */
    if (fields == 3) {
        if (letter != 'e' && letter != 'd' && letter != 'c' && letter != 'b' && letter != 'a')
            return nullptr;
        letter -= 0x20;
    } else {
        if (sscanf(name, "d%2d%2d", &stage, &number) != 2)              /* data 0x3091f0 */
            return nullptr;
        letter = -1;
    }
    if (letter < 0)
        sprintf(out, "EV%02d_%02d%s", stage, number, suffix);           /* data 0x3091f8 */
    else
        sprintf(out, "EV%02d_%02d%c%s", stage, number, letter, suffix); /* data 0x3091e0 */
    return out;
}

// FS2 0x00287bb0
// callcheck: 0x288930, strcat

const char *StageEvent::getVoicePath(char *out)
{
    if (voiceFileName(out, name) == nullptr)
        return nullptr;
    strcat(out, ".xwb");    /* data 0x309190 */
    return out;
}

// name + the language's letters.
// FS2 0x00288a90
// callcheck: getSaveData, getLanguage, sprintf

static const char *languageFileName(char *out, const char *name)
{
    const char *suffix;
    getSaveData();
    switch (getLanguage()) {
    case 2:  suffix = "KR"; break;      /* data 0x309218 */
    case 4:  suffix = "Ct"; break;      /* data 0x309210 */
    case 5:  suffix = "EN"; break;      /* data 0x309220 */
    case 6:  suffix = "DE"; break;      /* data 0x309228 */
    case 7:  suffix = "FR"; break;      /* data 0x309230 */
    case 8:  suffix = "ES"; break;      /* data 0x309238 */
    case 9:  suffix = "IT"; break;      /* data 0x309240 */
    default: suffix = "J";  break;      /* data 0x309208 */
    }
    sprintf(out, "%s%s", name, suffix);     /* data 0x309248 */
    return out;
}

StageEvent::StageEvent(float seconds, const char *eventName)
    : fadeSeconds(seconds), state(StageEvent_Starting), runner(nullptr)
{
    strcpy(name, eventName);
}

// FS2 0x002888a0 (the base part of the two below)
// callcheck: deleting dtor of the runner

StageEvent::~StageEvent()
{
    delete runner;
}

void StageEvent::finish()
{
    if (state != StageEvent_Finished) {
        state = StageEvent_Finished;
        if (!hidesText())
            gStageScene->pauseState->clearList();
    }
}

// FS2 0x002882b0
// callcheck: strcpy, FileLoader

StageFileEvent::StageFileEvent(float seconds, const char *eventName)
    : StageEvent(seconds, eventName), m_loader(new FileLoader(nullptr))
{
}

// FS2 0x00288210
// callcheck: ~FileLoader

StageFileEvent::~StageFileEvent()
{
    delete m_loader;
}

// FS2 0x00287f20
// callcheck: 0x263b90, setMuted, getTopView, getPlayerAltFlag, playerOrTargetInView, update, threadYield, isSettled, getTopView, getPlayerAltFlag, playerOrTargetInView, update, threadYield, isSettled, 0x288bc0, strcpy, 0x288b80, addFile, 0x288a90, sprintf, addFile, getSaveData, getLanguage, sprintf, addFile, sprintf, addFile, 0x288930, strcat, strcpy, addFile, startLoad

void StageFileEvent::queueFiles()
{
    StageScene *scene = gStageScene;
    StageSound *sound = scene->sound;
    sound->setMuted(true);

    auto stepSound = [&]() {
        sound->update(scene->getTopView(), scene->getPlayerAltFlag(),
                      scene->playerOrTargetInView());
        threadYield(1);
    };
    for (int i = 0; i < kSoundSettleFrames; i++)
        stepSound();
    while (!sound->isSettled())
        stepSound();

    char path[0x100];
    sprintf(path, "demoEU/s%02d/", getStageSelect()->stage);    /* data 0x309280 */
    m_loader->setDir(path);
    sprintf(path, "%s.edp", name);                              /* data 0x309278 */
    m_packIndex = m_loader->addFile(path);

    char base[0x80];
    languageFileName(base, name);
    sprintf(path, "%s.fsm", base);                              /* data 0x3091a0 */
    m_fsmIndex = m_loader->addFile(path);

    if (!languageUsesGameFont()) {
        sprintf(path, "%s.tm2", base);                          /* data 0x3091b8 */
        m_imageIndex = m_loader->addFile(path);
        sprintf(path, "%s.cod", base);                          /* data 0x3091c0 */
        m_fontIndex = m_loader->addFile(path);
    } else {
        m_imageIndex = -1;
        m_fontIndex = -1;
    }

    char voiceName[0x80];
    if (voiceFileName(voiceName, name) == nullptr) {
        m_voiceIndex = -1;
    } else {
        strcat(voiceName, ".xwh");                              /* data 0x309198 */
        m_loader->setDir("streamEU/demo/");                     /* data 0x3091a8 */
        m_voiceIndex = m_loader->addFile(voiceName);
    }
    m_loader->startLoad();
}

// FS2 0x00287db0
// callcheck: waitLoaded, getItem x5, setMuted, requestSubScene

void StageFileEvent::takeFiles()
{
    m_loader->waitLoaded(0, false);
    pack = m_loader->getItem(m_packIndex);
    if (m_voiceIndex >= 0)
        voice = m_loader->getItem(m_voiceIndex);
    if (m_fsmIndex >= 0) {
        fsm = m_loader->getItem(m_fsmIndex);
        if (m_imageIndex >= 0) {
            subtitleImage = m_loader->getItem(m_imageIndex);
            if (m_fontIndex >= 0)
                subtitleFont = m_loader->getItem(m_fontIndex);
        }
    }

    if (pack == nullptr) {
        gStageScene->sound->setMuted(false);
        finish();
    } else {
        gStageScene->requestSubScene(name);
        if (state != StageEvent_Active)
            state = StageEvent_Active;
    }
}

// FS2 0x00287c00
// callcheck: isCleared, isGameOver, 0x207440, isFullInMode2, queueFiles, takeFiles, start, queueFiles, takeFiles

int StageFileEvent::poll()
{
    StageScene *scene = gStageScene;
    if (scene->isCleared() || scene->isGameOver()) {
        if (state == StageEvent_Starting || state == StageEvent_Fading)
            finish();
        return state;
    }

    switch (state) {
    case StageEvent_Starting:
        if (fadeSeconds > 0.0f) {
            scene->fade->start(fadeSeconds, kFullBlack, kFullClear);
            state = StageEvent_Fading;
        } else {
            queueFiles();
            takeFiles();
        }
        break;
    case StageEvent_Fading:
        if (scene->fade->isFullInMode2()) {
            queueFiles();
            takeFiles();
        }
        break;
    case StageEvent_Active:
        finish();
        break;
    default:
        break;
    }
    return state;
}

// FS2 0x00288850
// callcheck: strcpy

StageSimpleEvent::StageSimpleEvent(float seconds, const char *eventName)
    : StageEvent(seconds, eventName)
{
}

// FS2 0x002887a0
// callcheck: operator_delete[]

StageSimpleEvent::~StageSimpleEvent()
{
    delete[] (char *)m_30;
}

// FS2 inline in 0x288340 (twice)
// callcheck: operator_new, ScriptRunner, findEntry, setEntry, isFullInMode2, start, saveFlags, stageHideObjects, getStageResult

void StageSimpleEvent::begin()
{
    StageScene *scene = gStageScene;
    if (runner == nullptr)
        runner = new ScriptRunner;
    runner->setEntry(scene->scripts[0]->findEntry(name));
    if (!runner->isRunning()) {
        if (scene->fade->isFullInMode2())
            scene->fade->start(fadeSeconds, kFullClear, kFullBlack);
        finish();
    } else {
        scene->arrays->saveFlags();
        stageHideObjects();
        getStageResult()->paused = 1;
        state = StageEvent_Active;
    }
}

// FS2 0x00288340
// callcheck: isCleared, isGameOver, 0x214f20, 0x1dedb0, run, restoreFlags, getStageResult, 0x207440, isFullInMode2, begin, isFullInMode1, start

int StageSimpleEvent::poll()
{
    StageScene *scene = gStageScene;
    if (scene->isCleared() || scene->isGameOver()) {
        if (state == StageEvent_Starting || state == StageEvent_Fading)
            finish();
        return state;
    }

    if (scene->player != nullptr && scene->player->isHoldingAction())
        scene->player->releaseAction();

    switch (state) {
    case StageEvent_Starting:
        if (fadeSeconds <= 0.0f) {
            begin();
        } else if (!scene->fade->isFullInMode1() && !scene->fade->isFullInMode2()) {
            scene->fade->start(fadeSeconds, kFullBlack, kFullClear);
            state = StageEvent_Fading;
        }
        break;
    case StageEvent_Fading:
        if (scene->fade->isFullInMode2())
            begin();
        break;
    case StageEvent_Active:
        runner->run(0);
        if (!runner->isRunning()) {
            scene->arrays->restoreFlags();
            getStageResult()->paused = 0;
            finish();
        }
        break;
    default:
        break;
    }
    return state;
}
