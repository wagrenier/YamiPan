// game/stageresult.cpp -- the stage result and the game-pack loader
// (game/stageresult.h).

#include "game/stageresult.h"

#include <string.h>

#include "game/gamehooks.h"
#include "game/sysdata.h"
#include "game/thread.h"
#include "sdk/scetypes.h"

static const char kGamePackPath[] = "dataEU/packgameEU.pak";    /* data 0x30b340 */

// The countdown cues (StageSound names).
static const char kCueCount[] = "SYS_COUNT1";                   /* data 0x30b368 */
static const char kCueZero[] = "SYS_COUNT2";                    /* data 0x30b358 */
constexpr float kCueVolume = 1.0f;

// Constructed by the static initialiser 0x310f60, destroyed through
// __register_global_object (the host runtime does both).
StageResult gStageResult;           /* bss 0x599700 */

// ──────────────────────────────────────────────────────────────────────
// The init-pack item for the current language (DE / FR / ES / IT have
// their own, every other language the English one).
// FS2 0x002afc20

void *getInitItemForLanguage()
{
    // The ROM fetches the options and drops them (an inline accessor's
    // leftover); the language is a global of its own.
    getGameOptions();
    int language = getLanguage();
    if (language == Language_IT)
        return getInitPackItem(InitItem_LangIT);
    if (language == Language_ES)
        return getInitPackItem(InitItem_LangES);
    if (language == Language_FR)
        return getInitPackItem(InitItem_LangFR);
    if (language == Language_DE)
        return getInitPackItem(InitItem_LangDE);
    return getInitPackItem(InitItem_LangEN);
}

// ──────────────────────────────────────────────────────────────────────
// The same lookup for an item every language shares.
// FS2 0x002afcd0

void *getInitItem21()
{
    getGameOptions();
    getLanguage();
    return getInitPackItem(InitItem_21);
}

// ──────────────────────────────────────────────────────────────────────
// Once before the scene loop (gameSceneThread): the game pack is read in
// thread slot 3 while this slot waits for it.
// FS2 0x002afd00

void loadGamePack()
{
    threadSlotCreate(loadGamePackThread, ThreadSlot_Game, nullptr, nullptr, 0, nullptr);
    do
        threadYield(1);
    while (threadSlotState(ThreadSlot_Game) != ThreadState_Free);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002afd50

void loadGamePackThread(void *arg)
{
    (void)arg;
    loadGamePackTables();
    threadExit();
}

// ──────────────────────────────────────────────────────────────────────
// Loads packgameEU.pak, parses its tables and frees both again: what
// PackTables::parse keeps lives elsewhere (not read yet).
// FS2 0x002afd80
// callcheck: operator_new, operator_delete, PackTables (the new- and delete-expressions)
// callcheck: ~NameMap, ~ListMap (PackTables' implicit destructor, inline: its eight maps from +0x70 down)

void loadGamePackTables()
{
    PackFile *pack = new PackFile(kGamePackPath, 0);
    pack->load(0, 0);
    PackTables *tables = new PackTables;
    tables->parse(pack);
    delete tables;
    // PC: the ROM deletes through the pack's vtable slot 0 (deleting dtor).
    delete pack;
}

// ──────────────────────────────────────────────────────────────────────
// Whether this play beats the save's record for the play record's stage,
// kind and variant: stage 29 keeps the higher second clock, every other
// stage the lower clock (as SystemData::recordStageResult stores them).
// FS2 0x002aff80

bool StageResult::beatsRecord() const
{
    SystemData *data = reinterpret_cast<SystemData *>(getSaveData());
    const StageSelect &select = data->select;
    u_int value = select.stage == kStageHighestWins ? clock2.value : clock.value;
    u_int best = data->save.getRecord(select.kind, select.variant, select.stage);
    if (select.stage == kStageHighestWins)
        return value > best;
    return value < best;
}

// ──────────────────────────────────────────────────────────────────────
// The medal bits of this play: one per counter that stayed at 0.
// FS2 0x002b00d0

u_int StageResult::getPerfectBits() const
{
    u_int bits = 0;
    if (counter[0] == 0)
        bits = PerfectBit_NoContinue;
    if (counter[1] == 0)
        bits |= PerfectBit_Counter1;
    if (counter[2] == 0)
        bits |= PerfectBit_Counter2;
    if (counter[3] == 0)
        bits |= PerfectBit_Counter3;
    return bits;
}

// ──────────────────────────────────────────────────────────────────────
// Every frame of a stage (the stage loop, packet list 10): tick, then the
// clock layout advances, updates and draws unless the game is paused.
// FS2 0x002b0120

void StageResult::update(sceVif1Packet *packet, TexAreaList *texList)
{
    tick();
    if (paused == 0 && layout != nullptr) {
        layout->advance();
        layout->update();
        layout->draw(packet, texList);
    }
}

// ──────────────────────────────────────────────────────────────────────
// One tick: the save's play time, then (unless paused, or held in
// countdown mode) both clocks; the countdown cues each 25 ticks over its
// last 250 and a second cue at 0.
// FS2 0x002b01a0

void StageResult::tick()
{
    SystemData *data = getSystemData();
    if (data->save.playTime != kCounterStopped)
        data->save.playTime++;

    bool running = false;
    if (paused == 0) {
        if (mode == StageClock_Countdown) {
            if (hold == 0)
                running = true;
        } else {
            running = true;
        }
    }
    if (!running)
        return;

    u_int before = clock.value;
    clock.advance();
    clock2.advance();
    if (mode == StageClock_Countdown && clock.value != before && clock.value <= kCountdownCueFrom
        && clock.value % kCountdownCueEvery == 0) {
        if (clock.value == 0)
            gStageScene->sound->playNamed(kCueZero, kCueVolume);
        else
            gStageScene->sound->playNamed(kCueCount, kCueVolume);
    }
}

// ──────────────────────────────────────────────────────────────────────
// A continue from a checkpoint (the game-over and pause menus).
// FS2 0x002b0310

void StageResult::addContinue()
{
    counter[0]++;
}

// ──────────────────────────────────────────────────────────────────────
// Inline in the ROM (restart, begin): both clocks set up for the mode, the
// pause bytes and the four counters cleared.

inline void StageResult::resetPlay()
{
    clock.state = 0;
    clock.value = 0;
    clock.step = 0;
    clock.target = 0;
    clock2.state = 0;
    clock2.value = 0;
    clock2.step = 0;
    clock2.target = 0;
    if (mode == StageClock_Countdown) {
        clock.value = kCountdownStart;
        clock.step = -1;
    } else if (mode == StageClock_CountUp) {
        clock.step = 1;
    }
    paused = 0;
    hold = 0;
    counter[0] = 0;
    counter[1] = 0;
    counter[2] = 0;
    counter[3] = 0;
}

// ──────────────────────────────────────────────────────────────────────
// The stage starts over (the menus' restart answers): the play and the
// attempt block go, the mode, the stage block and the layout stay.
// FS2 0x002b0320

void StageResult::restart()
{
    resetPlay();
    memset(attempt, 0, sizeof(attempt));
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002b03b0

bool StageResult::hasAttemptFlag() const
{
    return attempt[0] != 0;
}

// ──────────────────────────────────────────────────────────────────────
// End of a stage (the stage loop): the clock layout goes.
// FS2 0x002b03c0
// callcheck: ~UiLayout (the delete-expression: the ROM calls the deleting destructor with flag 1)

void StageResult::releaseLayout()
{
    if (layout != nullptr)
        delete layout;
    layout = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// A stage starts with a clock mode: everything cleared, and for a clock
// mode a new layout from the language pack (count-up or countdown face).
// FS2 0x002b0400
// callcheck: operator_new, UiLayout, ~UiLayout (the new- and delete-expressions; ~UiLayout is the deleting one)

void StageResult::begin(int newMode)
{
    mode = newMode;
    resetPlay();
    memset(attempt, 0, sizeof(attempt) + sizeof(stage));
    memset(&_90, 0, sizeof(_90));
    // releaseLayout, inline.
    if (layout == nullptr) {
        layout = nullptr;
    } else {
        delete layout;
        layout = nullptr;
    }
    if (newMode != StageClock_None) {
        if (newMode == StageClock_Countdown) {
            layout = new UiLayout;
            layout->setData(getLangPackItem(LangItem_ClockCountdown), -1);
        } else if (newMode == StageClock_CountUp) {
            layout = new UiLayout;
            layout->setData(getLangPackItem(LangItem_ClockCountUp), -1);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002b0580

StageResult *getStageResult()
{
    return &gStageResult;
}

// ──────────────────────────────────────────────────────────────────────
// The static destructor (registered by 0x310f60): the layout goes.
// FS2 0x002b0cf0
// callcheck: operator_delete, ~UiLayout (the deleting variant, compiler generated; the delete-expression)

StageResult::~StageResult()
{
    if (layout != nullptr)
        delete layout;
    layout = nullptr;
}

// The MSL std::map / std::vector instances of PackTables' maps, out of line
// in the ROM between getStageResult and the destructor (0x2bcf10's TU calls them too); the host
// <map> / <vector> provide them.
//   FS2 0x002afea0  ListMap::~ListMap (deleting when the flag > 0)
//   FS2 0x002aff10  NameMap::~NameMap (deleting when the flag > 0)
//   FS2 0x002b0590  ListMap::eraseTree (recursive, two levels unrolled)
//   FS2 0x002b07f0  ListMap::Value::~Value (the pair: its EntryList at +4)
//   FS2 0x002b0860  ListMap::freeNode (Heap::free on gGlobalHeap)
//   FS2 0x002b0870  ListMap::getNodeAllocator (this + 4)
//   FS2 0x002b0880  ListMap::destroyValue
//   FS2 0x002b08c0  ListMap::getValueAllocator (returns its argument)
//   FS2 0x002b08d0  NameMap::eraseTree
//   FS2 0x002b0b20  NameMap::Value::~Value (the pair: its string key)
//   FS2 0x002b0b80  NameMap::freeNode
//   FS2 0x002b0b90  NameMap::getNodeAllocator (this + 4)
//   FS2 0x002b0ba0  NameMap::destroyValue
//   FS2 0x002b0bd0  NameMap::getValueAllocator (returns its argument)
//   FS2 0x002b0be0  EntryList::~EntryList (deleting when the flag > 0)
//   FS2 0x002b0c50  EntryList::clear (destroys from the back)
