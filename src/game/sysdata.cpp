// game/sysdata.cpp -- the system data block: save data, play record,
// options, flag table, language (game/sysdata.h).

#include "game/sysdata.h"

#include <string.h>

#include "game/gamehooks.h"
#include "game/gamesystem.h"
#include "game/gfxtarget.h"
#include "game/heap.h"
#include "game/memcard.h"
#include "game/progress.h"
#include "game/stageresult.h"
#include "sdk/scetypes.h"

// ButtonMap's default button per action (setDefaults stores them, the
// switch in defaultButton answers them).
constexpr int kButtonMapDefaults[kButtonMapActions] = { 6, 5, 4, 7, 3, 1, 2, 0 };
// defaultButton's answer for an action out of range.
constexpr int kButtonMapFallback = 6;

// SaveData::pickVariant3ac draws one of four with these weights (data
// 0x2fabf0; they sum to 1).
constexpr int kVariant3acCount = 4;
static const float sVariant3acWeights[kVariant3acCount] = { 0.2f, 0.25f, 0.25f, 0.3f };

// The bit a cleared (kind, variant) sets in a stage's flag byte (data
// 0x307350).
static const int sClearBits[kRecordKinds][kRecordVariants] = {
    { 0x01, 0x02 },
    { 0x04, 0x08 },
    { 0x10, 0x20 },
};

// The flag table entries FlagTable::countListed counts (data 0x2fac00).
static const u_char sListedFlags[] = {
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
    13, 14, 15, 16, 17, 18, 19,
    23, 24, 25, 26, 27, 28, 29, 30, 31, 32,
};
static int sListedFlagCount = sizeof(sListedFlags);     /* sdata 0x31aacc: 29 */

// GameOptions::brightness -> value (data 0x2fac20).
static const u_char sBrightnessValues[] = {
    28, 36, 44, 52, 64, 72, 80, 88, 96, 104, 112, 120, 128, 136, 144, 152, 160, 168, 176,
};

static const char *const sLanguageCodes[Language_Count] = {    /* data 0x2fac40 */
    "J", "US", "KR", "CN", "Ct", "EN", "DE", "FR", "ES", "IT",
};

SystemData gSystemData;                 /* bss 0x598b30 */
static int sLanguage = Language_EN;     /* sdata 0x31aad0 */

// ──────────────────────────────────────────────────────────────────────
// The play record: a stage number in 0..29 or 80..94 and a record kind 0..2.
// FS2 0x0023af30

bool StageSelect::isValid() const
{
    if ((stage < 0 || stage >= kStageCount) && (stage < kStageExtraFirst || stage > kStageExtraLast))
        return false;
    if (kind < 0)
        return false;
    return kind < kRecordKinds;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0023af80

void StageSelect::setDefaults()
{
    stage = 1;
    kind = 1;
    variant = 0;
    exitKind = StageExit_None;
    _10 = 0;
}

// ──────────────────────────────────────────────────────────────────────
// 1 when both blocks hold the same bytes.
// FS2 0x0023afa0

int SaveData::equals(const SaveData *other) const
{
    const u_char *a = (const u_char *)this;
    const u_char *b = (const u_char *)other;
    for (int i = 0; i < (int)sizeof(SaveData); i++) {
        if (b[i] != a[i])
            return 0;
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Sets the byte +0x468 once: true only for the call that set it.
// FS2 0x0023afe0

bool SaveData::markUnlock468()
{
    bool first = unlock468 == 0;
    if (first)
        unlock468 = 1;
    return first;
}

// ──────────────────────────────────────────────────────────────────────
// Whether a stage's variant-1 records count: stages 1..28 but 27.
// FS2 0x0023b000

bool SaveData::stageHasVariant1(int stage) const
{
    if (stage < 0)
        return false;
    if (stage >= kStageCount)
        return false;
    if (stage == 0)
        return false;
    if (stage == 27)
        return false;
    return stage != 29;
}

// ──────────────────────────────────────────────────────────────────────
// 1 when every stage 1..29 has a changed kind-1 or kind-2 record in
// variant 0 and, where variant 1 counts, in variant 1 too.
// FS2 0x0023b040

int SaveData::allRecordsSet() const
{
    for (int stage = 1; stage < kStageCount; stage++) {
        if (getRecord(1, 0, stage) == stageDefaultRecord(1, 0, stage) &&
            getRecord(2, 0, stage) == stageDefaultRecord(2, 0, stage))
            return 0;
        if (stageHasVariant1(stage)) {
            if (getRecord(1, 1, stage) == stageDefaultRecord(1, 1, stage) &&
                getRecord(2, 1, stage) == stageDefaultRecord(2, 1, stage))
                return 0;
        }
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0023b1b0

bool SaveData::isRecordSet(int kind, int variant, int stage) const
{
    u_int fresh = stageDefaultRecord(kind, variant, stage);
    return getRecord(kind, variant, stage) != fresh;
}

// ──────────────────────────────────────────────────────────────────────
// A fresh save: every record its default, no flags, no medals.
// FS2 0x0023b280

void SaveData::setDefaults()
{
    memset(this, 0, sizeof(SaveData));

    for (int kind = 0; kind < kRecordKinds; kind++) {
        for (int variant = 0; variant < kRecordVariants; variant++) {
            for (int stage = 0; stage < kStageCount; stage++)
                record[kind][variant][stage] = stageDefaultRecord(kind, variant, stage);
        }
    }

    for (int i = 0; i < kStageFlagCount; i++)
        stageFlags[i] = 0;

    playTime = 0;
    _3a8 = 0;
    variant3ac = 0;
    _3b0 = 0;

    for (int kind = 0; kind < kRecordKinds; kind++) {
        for (int variant = 0; variant < kRecordVariants; variant++) {
            for (int stage = 0; stage < kStageCount; stage++)
                medals[kind][variant][stage] = 0;
        }
    }

    unlock468 = memcardGetState();

    for (int i = 0; i < 16; i++) {
        _46c[i] = 0;
        _4ac[i] = 0;
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0023b510

int SaveData::clearBit(int kind, int variant) const
{
    return sClearBits[kind][variant];
}

// ──────────────────────────────────────────────────────────────────────
// Stages: any of the mask's bits, or all of them with kStageFlagTestAll in
// the mask. Past the stages: whether the flag is set at all.
// FS2 0x0023b530

bool SaveData::testStageFlags(int stage, u_int mask) const
{
    u_int flags = stageFlags[stage];
    if (stage < kStageCount) {
        if (mask & kStageFlagTestAll) {
            u_int bits = mask & ~kStageFlagTestAll;
            return (flags & bits) == bits;
        }
        return (flags & mask) != 0;
    }
    return flags != 0;
}

// ──────────────────────────────────────────────────────────────────────
// Stores variant 0..3 as given, or a weighted random one for anything else;
// returns what it stored.
// FS2 0x0023b580

int SaveData::pickVariant3ac(int variant)
{
    if (variant < 0 || variant >= kVariant3acCount) {
        float r = randUnit();
        float sum = 0.0f;
        variant = kVariant3acCount - 1;
        for (int i = 0; i < kVariant3acCount; i++) {
            sum += sVariant3acWeights[i];
            if (r < sum) {
                variant = i;
                break;
            }
        }
    }
    variant3ac = variant;
    return variant;
}

// ──────────────────────────────────────────────────────────────────────
// After a stage: its clear bit, the better record (when the result counts)
// and the medal bits for the play record's stage, kind and variant.
// FS2 0x0023b630

void SystemData::recordStageResult()
{
    u_char *stageFlag = &save.stageFlags[select.stage];
    int bit = save.clearBit(select.kind, select.variant);
    if (bit & ~*stageFlag) {
        *stageFlag |= bit;
        if (save.stageFlags[kStageFlagUnlockA] != 0 && save.stageFlags[kStageFlagUnlockB] != 0)
            save.markUnlock468();
    }

    if (getStageResult()->mode == StageClock_None)
        return;
    if (select.stage >= kStageCount)
        return;

    u_int value;
    if (select.stage == kStageHighestWins)
        value = getStageResult()->clock2.value;
    else
        value = getStageResult()->clock.value;

    int stage = select.stage;
    int kind = select.kind;
    int variant = select.variant;
    if (stage < kStageCount) {
        u_int best = save.getRecord(kind, variant, stage);
        bool better = stage == kStageHighestWins ? value > best : value < best;
        if (better)
            save.record[kind][variant][stage] = value;
    }

    u_char medal = (u_char)getStageResult()->getPerfectBits();
    if (select.stage < kStageCount)
        save.medals[select.kind][select.variant][select.stage] |= medal;
}

// ──────────────────────────────────────────────────────────────────────
// How many of the listed entries are set.
// FS2 0x0023ba20

int FlagTable::countListed() const
{
    int count = 0;
    for (int i = 0; i < sListedFlagCount; i++) {
        if (value[sListedFlags[i]] != 0)
            count++;
    }
    return count;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0023ba70

void FlagTable::set(int index, int v)
{
    value[index] = v;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0023ba80

int FlagTable::get(int index) const
{
    return value[index];
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0023ba90

void FlagTable::clear()
{
    for (int i = 0; i < kFlagTableSize; i++)
        value[i] = 0;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0023bad0

FlagTable *getFlagTable()
{
    return &gSystemData.flags;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0023baf0

GameOptions *getGameOptions()
{
    return &gSystemData.options;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0023bb10

StageSelect *getStageSelect()
{
    return &gSystemData.select;
}

// ──────────────────────────────────────────────────────────────────────
// Same block as getSaveData; which getter is meant for what is a guess.
// FS2 0x0023bb30

SystemData *getSystemData()
{
    return &gSystemData;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0023bb40

SaveData *getSaveData()
{
    return &gSystemData.save;
}

// ──────────────────────────────────────────────────────────────────────
// The boot's reset of the whole block (gameInit, before
// getGameOptions()->setDefaults()).
// FS2 0x0023bb50
// callcheck: clear (FlagTable::clear inline in the ROM)

void systemDataInit()
{
    gSystemData.save.setDefaults();
    gSystemData.select.setDefaults();
    gSystemData.options.init();
    gSystemData.flags.clear();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0023bbd0

void setLanguage(int language)
{
    sLanguage = language;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0023bbe0

int getLanguage()
{
    return sLanguage;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0023bbf0

const char *getFileSuffix(int alternate)
{
    if (alternate == 0)
        return "";
    return "_e";
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0023bc20

const char *getRegionCode()
{
    return "EU";
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0023bc30

const char *getLanguageCode(int language)
{
    return sLanguageCodes[language];
}

// ──────────────────────────────────────────────────────────────────────
// Hands the sound output mode to the sound layer (mono for anything not
// stereo or surround).
// FS2 0x0023bc50

void GameOptions::applySound()
{
    if (soundMode == SoundOutput_Stereo)
        gSoundConfig->setOutputMode(SoundOutput_Stereo);
    else if (soundMode == SoundOutput_Surround)
        gSoundConfig->setOutputMode(SoundOutput_Surround);
    else
        gSoundConfig->setOutputMode(SoundOutput_Mono);
}

// ──────────────────────────────────────────────────────────────────────
// Points port 0's pad at this button map and moves the picture.
// PC: the display offset moves the GS DISPLAY registers; the host renderer
// owns the display size and should read screenX / screenY itself (any
// aspect ratio), so gfxSetDisplayOffset becomes a host hook there.
// FS2 0x0023bcb0

void GameOptions::applyPadAndScreen()
{
    gGameSystem->m_pad0.setButtonMap(&buttonMap);
    gfxSetDisplayOffset(gfxGetCurrentDisplay(), screenX, screenY);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0023bd00

void ButtonMap::setDefaults()
{
    for (int i = 0; i < kButtonMapActions; i++)
        button[i] = kButtonMapDefaults[i];
}

// ──────────────────────────────────────────────────────────────────────
// 1 when every option and the button map match.
// FS2 0x0023bd40

int GameOptions::equals(const GameOptions *other) const
{
    if (soundMode != other->soundMode)
        return 0;
    if (_08 != other->_08)
        return 0;
    if (screenX != other->screenX)
        return 0;
    if (screenY != other->screenY)
        return 0;
    if (_10 != other->_10)
        return 0;
    if (invertY != other->invertY)
        return 0;
    if (invertX != other->invertX)
        return 0;
    if (brightness != other->brightness)
        return 0;
    if (subtitles != other->subtitles)
        return 0;
    if (wideScreen != other->wideScreen)
        return 0;
    if (mapMode != other->mapMode)
        return 0;
    if (hints != other->hints)
        return 0;
    if (_4c != other->_4c)
        return 0;
    if (autoSave != other->autoSave)
        return 0;
    if (_04 != other->_04)
        return 0;
    for (int i = 0; i < kButtonMapActions; i++) {
        if (buttonMap.button[i] != other->buttonMap.button[i])
            return 0;
    }
    return 1;
}

// ──────────────────────────────────────────────────────────────────────
// Every option's default (autoSave is left as it is).
// FS2 0x0023be70
// callcheck: setDefaults (ButtonMap::setDefaults inline in the ROM)

void GameOptions::setDefaults()
{
    soundMode = SoundOutput_Stereo;
    _04 = 1;
    _08 = 1;
    screenX = 0;
    screenY = 0;
    _10 = 2;
    invertY = 1;
    invertX = 0;
    brightness = 9;
    subtitles = 1;
    wideScreen = 0;
    buttonMap.setDefaults();
    mapMode = MapMode_Always;
    hints = 1;
    _4c = 1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0023bef0

void GameOptions::init()
{
    setDefaults();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0023bf00

int ButtonMap::defaultButton(int action)
{
    if ((u_int)action >= kButtonMapActions)
        return kButtonMapFallback;
    return kButtonMapDefaults[action];
}

// ──────────────────────────────────────────────────────────────────────
// Gives an action a button; the action that had it gets the old one.
// PC: for action -1 the ROM still reads button[-1] (in the options' map the
// word before it, GameOptions::wideScreen) and hands that to the holder; the port
// leaves the holder's button as it is.
// FS2 0x0023bf80

void ButtonMap::assign(int action, int newButton)
{
    int holder;
    for (holder = 0; holder < kButtonMapActions; holder++) {
        if (newButton == button[holder])
            break;
    }
    if (holder == kButtonMapActions)
        holder = -1;

    int old = action != -1 ? button[action] : (holder != -1 ? button[holder] : 0);
    if (action != -1)
        button[action] = newButton;
    if (holder != -1)
        button[holder] = old;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x0023bff0

u_char GameOptions::getBrightnessValue() const
{
    return sBrightnessValues[brightness];
}
