/* ==========================================================================
 *  game/charactersound.cpp (FS2 0x2a2070..0x2a2e30, 0x2a31b0..0x2a49c0,
 *  0x2a4a40..0x2a6420, 0x2a6830..0x2a6980): the cues a character's motions and
 *  hits ask for, by who the character is, and what it sounds like each frame.
 *
 *  The cue's name comes from the sheet: a PC and an NPC have one name each
 *  (the ROM's strings are all NPC_..., the PC's is the same string one byte
 *  on), a shibito's has its sheet's SEtype in it (E%02d_...). Sound kind 7
 *  has names here and getSoundKind never answers it: the ROM's, kept.
 * ======================================================================== */
#include "game/gamehooks.h"
#include "game/gamerate.h"
#include "game/characterbrain.h"
#include "game/characterground.h"
#include "game/modelpose.h"
#include "game/playermotion.h"
#include "game/stageitem.h"
#include "game/stagecollision.h"
#include "game/stagesound.h"
#include "game/sysdata.h"

#include <stdio.h>
#include <string.h>

constexpr int kVoiceNameSize = 0x20;    // the ROM's buffers

enum { kSoundNone = 0, kSoundSource = 1, kSoundCue = 2 };   // m_soundState
constexpr int kMoveSoundMotionBase = 5;  // m_moveSoundState above the kinds' own states
static const float kSoundAtOrigin[4] = { 0.0f, 0.0f, 0.0f, 1.0f };  /* data 0x309de0: at its parent */

// A PC's or an NPC's own cue, by the sheet's kind; anyone else has none.
// Inline in the ROM, each with its own getStageSelect.
static void sayPartyVoice(StageCharacter *character, const char *npcName)
{
    const int kind = stageObjRecord(character->m_itemDef)->kind;
    if (kind == CharacterKind_1)
        character->playVoiceHeard(npcName);
    else if (kind == CharacterKind_0)
        character->playVoiceHeard(npcName + 1);
}

// "%s_DMG_%c" for the character's sound kind. Inline three times in the ROM
// (0x2a27b4, 0x2a3840, 0x2a44c8); its table's rows 0 and 1 are the same PC.
static void formatDamageVoice(StageCharacter *character, char *out, bool light)
{
    static const char *const kOwners[] = {  /* data 0x30a058.. */
        "PC", "PC", "NPC", nullptr, "UMI", "SRT", "YRT", "RYO", "TKS", "BOSS1",
    };
    char enemy[kVoiceNameSize];             // PC: the ROM's 4 bytes end at the frame's top
    const int kind = character->getSoundKind();
    const char *owner = kOwners[kind];
    if (kind == SoundKind_Enemy) {
        sprintf(enemy, "E%02d", stageObjRecord(character->m_itemDef)->seType);
        owner = enemy;
    }
    sprintf(out, "%s_DMG_%c", owner, light ? 'L' : 'H');
}

// What a PC or NPC says on a swing, by what it holds.
static const char *partyAttackVoice(StageWeaponItem *weapon, int second, bool altered)
{
    if (weapon == nullptr)
        return altered ? "NPC_AT_NW2" : "NPC_AT_NW1";
    const ItemDef &def = gItemDefs[weapon->m_itemDef];
    if (def.kind == ItemKind_MeleeLight)
        return second != 0 ? "NPC_AT4" : "NPC_AT3";
    if (def.isFirearm())
        return "NPC_AT5";
    return second != 0 ? "NPC_AT2" : "NPC_AT1";
}

// The cue of an attack, in buffer when it has to be built; null none.
// A shibito's attack takes its damage name: the ROM's format string is the same.
// FS2 0x002a25b0
// callcheck: getSoundKind, getSoundKind, getSoundKind, getStageSelect, sprintf, sprintf, sprintf (partyAttackVoice, formatDamageVoice)

const char *StageCharacter::getAttackVoice(char *buffer, int second, bool altered)
{
    const int kind = getSoundKind();
    switch (kind) {
    case SoundKind_Pc:
    case SoundKind_Npc: {
        const char *name = partyAttackVoice(static_cast<StageWeaponItem *>(m_weapons[0]), second, altered);
        return kind == SoundKind_Pc ? name + 1 : name;
    }
    case SoundKind_Enemy:
    case SoundKind_Umi:
    case SoundKind_Ryo:
        formatDamageVoice(this, buffer, second != 0);
        return buffer;
    case SoundKind_Srt:
        return "SRT_AT";
    case SoundKind_Yrt:
        return "YRT_AT";
    case SoundKind_Tks:
        sprintf(buffer, "TKS_AT_NW%c", '2' - (second & 0xff));
        return buffer;
    case SoundKind_Boss1:
        return "BOSS1_AT1";
    default:
        return nullptr;
    }
}

// The ROM's call passes 800 in a float register that playNamedHeard2 never reads.
// FS2 0x002a31b0
// callcheck: FUN_002a6980, playNamedHeard2 (getSoundKind)

void StageCharacter::playVanishSound()
{
    const int kind = getSoundKind();
    const char *name = kind == SoundKind_Yrt ? "YRT_VANISH" : kind == SoundKind_Srt ? "SRT_VANISH" : nullptr;
    playNamedHeard2(name, m_xform.position, nullptr);
}

// The ROM hands getHitSound a second argument it never reads.
// FS2 0x002a3220
// callcheck: getHitSound, playNamedHeard2

void StageCharacter::playHitSound(StageWeaponItem *weapon)
{
    const char *name = weapon == nullptr ? "BT_HT_L_AT" : weapon->getHitSound();
    if (name != nullptr)
        playNamedHeard2(name, m_xform.position, nullptr);
}

// FS2 0x002a3280
// callcheck: getSwingSound, playNamedHeard2

void StageCharacter::playSwingSound(bool light)
{
    StageWeaponItem *weapon = static_cast<StageWeaponItem *>(m_weapons[0]);
    const char *name = weapon == nullptr ? nullptr : weapon->getSwingSound(light);
    if (name != nullptr)
        playNamedHeard2(name, m_xform.position, nullptr);
}

// FS2 0x002a32e0
// callcheck: getStageSelect, playVoiceHeard, playVoiceHeard (sayPartyVoice)

void StageCharacter::playClashVoice()
{
    sayPartyVoice(this, "NPC_CLASH");
}

// FS2 0x002a33e0
// callcheck: getStageSelect, playVoiceHeard (sayPartyVoice)

void StageCharacter::playEndVoice()
{
    sayPartyVoice(this, "NPC_END");
}

// FS2 0x002a34e0
// callcheck: getStageSelect, playVoiceHeard (sayPartyVoice)

void StageCharacter::playSurpriseVoice()
{
    sayPartyVoice(this, "NPC_SURP");
}

// FS2 0x002a35e0
// callcheck: getStageSelect, playVoiceHeard (sayPartyVoice)

void StageCharacter::playTrapVoice()
{
    sayPartyVoice(this, "NPC_TRP_O");
}

// FS2 0x002a36e0
// callcheck: getSoundKind, getStageSelect, getSoundKind, getSoundKind, getSoundKind, getStageSelect, sprintf, sprintf, playVoiceHeard (sayPartyVoice, formatDamageVoice; only the row of kind 3 of its two switches can run)

void StageCharacter::playTrapHitVoice()
{
    const int kind = getSoundKind();
    if (kind == SoundKind_Enemy) {
        char name[kVoiceNameSize];
        formatDamageVoice(this, name, false);
        playVoiceHeard(name);
    } else if (kind == SoundKind_Npc || kind == SoundKind_Pc) {
        sayPartyVoice(this, "NPC_TRP_I");
    }
}

// FS2 0x002a39f0
// callcheck: getStageSelect, playVoiceHeard (sayPartyVoice)

void StageCharacter::playUntieVoice()
{
    sayPartyVoice(this, "NPC_UNTIE");
}

// FS2 0x002a3af0
// callcheck: getStageSelect, playVoiceHeard (sayPartyVoice)

void StageCharacter::playBittenVoice()
{
    sayPartyVoice(this, "NPC_BITE");
}

// FS2 0x002a3bf0
// callcheck: getStageSelect, playVoiceHeard (sayPartyVoice)

void StageCharacter::playJumpVoice()
{
    sayPartyVoice(this, "NPC_JUMP");
}

// FS2 0x002a3cf0
// callcheck: getStageSelect, playVoiceHeard (sayPartyVoice)

void StageCharacter::playBreathDown(int level)
{
    static const char *const kBreaths[] = {     /* data 0x2fb6e0 */
        "NPC_BM_DWL", "NPC_BM_DWM", "NPC_BM_DWH",
    };
    sayPartyVoice(this, kBreaths[level]);
}

// Two of the kinds vanish rather than die, and those are heard.
// FS2 0x002a2e30
// callcheck: getSoundKind, getStageSelect, sprintf, playVoiceHeard, playVoice

void StageCharacter::playDieVoice()
{
    char buffer[kVoiceNameSize];
    const char *stream = nullptr;
    const char *voice = nullptr;
    switch (getSoundKind()) {
    case SoundKind_Yrt:
        voice = "YRT_VANISH";
        break;
    case SoundKind_Srt:
        voice = "SRT_VANISH";
        break;
    case SoundKind_Enemy:
        sprintf(buffer, "SE%02d_DIE", stageObjRecord(m_itemDef)->seType);
        stream = buffer;
        break;
    case SoundKind_Npc:
        stream = "SNPC_DIE";
        break;
    case SoundKind_Pc:
        stream = "SPC_DIE";
        break;
    default:
        break;
    }
    if (stream != nullptr)
        playVoice(stream);
    else if (voice != nullptr)
        playVoiceHeard(voice);
}

// FS2 0x002a3e20
// callcheck: getStageSelect, playVoiceHeard (sayPartyVoice)

void StageCharacter::playBreathUp(int level)
{
    static const char *const kBreaths[] = {     /* data 0x2fb6cc */
        "NPC_BM_UPL1", "NPC_BM_UPL2", "NPC_BM_UPM", "NPC_BM_UPH", "NPC_BM_LFUP",
    };
    sayPartyVoice(this, kBreaths[level]);
}

// FS2 0x002a3f50
// callcheck: getStageSelect, playVoiceHeard (sayPartyVoice)

void StageCharacter::playHardDownVoice()
{
    sayPartyVoice(this, "NPC_DW2");
}

// FS2 0x002a4050
// callcheck: getStageSelect, playVoiceHeard (sayPartyVoice)

void StageCharacter::playDownVoice()
{
    sayPartyVoice(this, "NPC_DW1");
}

// FS2 0x002a4150
// callcheck: getStageSelect, playVoiceHeard (sayPartyVoice)

void StageCharacter::playThrowVoice()
{
    sayPartyVoice(this, "NPC_THROW");
}

// FS2 0x002a4250
// callcheck: getSoundKind, playVoiceHeard, getStageSelect, sprintf

void StageCharacter::playBiteVoice()
{
    if (getSoundKind() != SoundKind_Enemy)
        return;
    char name[kVoiceNameSize];
    sprintf(name, "E%02d_BITE", stageObjRecord(m_itemDef)->seType);
    playVoiceHeard(name);
}

// FS2 0x002a4340
// callcheck: getSoundKind, getStageSelect, sprintf, playVoiceHeard

void StageCharacter::playQuestionVoice()
{
    char buffer[kVoiceNameSize];
    const int kind = getSoundKind();
    if (kind == SoundKind_Ryo) {
        playVoiceHeard("RYO_Q");
    } else if (kind == SoundKind_Enemy) {
        sprintf(buffer, "E%02d_Q", stageObjRecord(m_itemDef)->seType);
        playVoiceHeard(buffer);
    }
}

// FS2 0x002a4450
// callcheck: getSoundKind, getSoundKind, getSoundKind, getStageSelect, sprintf, sprintf, playVoiceHeard (formatDamageVoice)

void StageCharacter::playDamageVoice(bool light)
{
    const int kind = getSoundKind();
    switch (kind) {
    case SoundKind_Pc:
    case SoundKind_Npc: {
        const char *name = light ? "NPC_DMG_L" : "NPC_DMG_H";
        playVoiceHeard(kind == SoundKind_Pc ? name + 1 : name);
        break;
    }
    case SoundKind_Enemy:
    case SoundKind_Umi:
    case SoundKind_Ryo: {
        char name[kVoiceNameSize];
        formatDamageVoice(this, name, light);
        playVoiceHeard(name);
        break;
    }
    case SoundKind_Yrt:
        playVoiceHeard("YRT_DAMAGE");
        break;
    default:
        break;
    }
}

// FS2 0x002a4680
// callcheck: FUN_002a25b0, playVoiceHeard (getAttackVoice)

void StageCharacter::playAttackVoice(int second, bool altered)
{
    char buffer[kVoiceNameSize];
    const char *name = getAttackVoice(buffer, second, altered);
    if (name != nullptr)
        playVoiceHeard(name);
}

// FS2 0x002a46d0
// callcheck: getSoundKind, getStageSelect, sprintf, playVoiceHeard

void StageCharacter::sayFound()
{
    if (m_voiceOn == 0)
        return;
    char buffer[kVoiceNameSize];
    const int kind = getSoundKind();
    if (kind == SoundKind_Ryo) {
        playVoiceHeard("RYO_FIND");
    } else if (kind == SoundKind_Enemy) {
        sprintf(buffer, "E%02d_FIND", stageObjRecord(m_itemDef)->seType);
        playVoiceHeard(buffer);
    }
}

// Not through playVoiceHeard: it plays over whatever is being said, and it
// is the one here that makes a noise. The ROM passes loudness to playNamed
// too, which does not read it, and its a1 is not read at all.
// FS2 0x002a4800
// callcheck: getSoundKind, getSoundKind, getStageSelect, sprintf, playNamed, isPlayer, addNoise, addNoise

void StageCharacter::playShout(float loudness, int, const float *pos)
{
    char buffer[kVoiceNameSize];
    const char *name = nullptr;
    const int kind = getSoundKind();
    if (kind == SoundKind_Npc) {
        name = "NPC_SHOUT";
    } else if (kind == SoundKind_Pc) {
        name = "PC_SHOUT";
    } else if (kind == SoundKind_Ryo) {
        name = "RYO_CALL";
    } else if (kind == SoundKind_Enemy) {
        sprintf(buffer, "E%02d_CALL", stageObjRecord(m_itemDef)->seType);
        name = buffer;
    }
    if (name == nullptr)
        return;
    playNamed(name, pos, nullptr);
    gStageScene->noises->addNoise(loudness, this, isPlayer() ? SceneNoise_Any : SceneNoise_Ally);
}

// One name per surface; only the families differ.
// FS2 0x002a29f0
// callcheck: playNamedHeard2

void StageCharacter::playFallDownSound(int surface)
{
    const char *name;
    switch (surface) {
    case 0: case 2: case 3: case 4:
    case 10: case 11: case 12: case 13: case 14: case 15:
    case 0x15: case 0x16: case 0x17: case 0x18:
    case 0x1e:
        name = "FALLDW1";
        break;
    case 1:
        name = "FALLDW2";
        break;
    case 5: case 6: case 7: case 8: case 9:
    case 0x10: case 0x11: case 0x12: case 0x13: case 0x14:
    case 0x19: case 0x1a: case 0x1b: case 0x1c:
        name = "FALLDW3";
        break;
    case 0x1d: case 0x1f:
        name = "FALLDW4";
        break;
    default:
        return;
    }
    playNamedHeard2(name, m_xform.position, nullptr);
}

// FS2 0x002a2b10
// callcheck: FUN_002cef50, playEntryHeard (getSurfaceWallSound)

void StageCharacter::playBumpSound(int surface)
{
    if (m_weapons[0] == nullptr)
        return;
    if (const SeEntry *entry = getSurfaceWallSound(surface))
        playEntryHeard(entry, m_xform.position, nullptr);
}

// The model's name has an m five letters in: taken to be a man (the name is a
// reading: only playJobSound asks, JOB00..04 for it and JOB05..09 else).
// FS2 0x00215320

static bool hasMaleModel(StageCharacter *character)
{
    return character->m_model->pose->name[4] == 'm';
}

// FS2 0x002a2b60
// callcheck: FUN_00215320, sprintf, playVoiceHeard (hasMaleModel)

void StageCharacter::playJobSound(int job)
{
    char name[kVoiceNameSize];
    if (!hasMaleModel(this))
        job += 5;
    sprintf(name, "JOB%02d", job);
    playVoiceHeard(name);
}

// FS2 0x002a2bc0
// callcheck: getSoundKind, getSoundKind, playVoice

void StageCharacter::playIdleVoice(int which)
{
    const int kind = getSoundKind();
    const char *name = nullptr;
    if (which == 0) {
        if (kind == SoundKind_Npc)
            name = "SNPC_IDLE01";
        else if (kind == SoundKind_Pc)
            name = "SPC_IDLE01";
    } else {
        if (kind == SoundKind_Npc)
            name = "SNPC_IDLE02";
        else if (kind == SoundKind_Pc)
            name = "SPC_IDLE02";
    }
    if (name != nullptr)
        playVoice(name);
}

// FS2 0x002a2c80
// callcheck: getSoundKind, playVoice

void StageCharacter::playPanicVoice()
{
    const int kind = getSoundKind();
    if (kind == SoundKind_Npc)
        playVoice("SNPC_SCREAM");
    else if (kind == SoundKind_Pc)
        playVoice("SPC_SCREAM");
}

// FS2 0x002a2d00
// callcheck: getSoundKind, getStageSelect, sprintf, playVoice

void StageCharacter::playGetUpVoice()
{
    char buffer[kVoiceNameSize];
    const char *name = nullptr;
    switch (getSoundKind()) {
    case SoundKind_Enemy:
        sprintf(buffer, "SE%02d_GETUP", stageObjRecord(m_itemDef)->seType);
        name = buffer;
        break;
    case SoundKind_Npc:
        name = "SNPC_GETUP";
        break;
    case SoundKind_Pc:
        name = "SPC_GETUP";
        break;
    default:
        break;
    }
    if (name != nullptr)
        playVoice(name);
}

// A motion's own sound; a resumable one stops when the motion changes (it
// borrows the move sound's slot, by the motion id + 5, for updateSounds).
// The ROM's call passes 800 in a float register that playNamedHeard2 never reads.
// FS2 0x002a2070
// callcheck: playNamedHeard2, stopSound

void StageCharacter::playMotionSound(const char *name, const float *pos)
{
    StageSoundSource *source = playNamedHeard2(name, pos, nullptr);
    if (source == nullptr || !source->m_entry->resumable)
        return;
    if (m_moveSoundState != 0)
        stopSound(m_moveSound);
    m_moveSoundState = m_motion->motionId + kMoveSoundMotionBase;
    m_moveSound = source;
}

// FS2 0x00215af0
// callcheck: getSaveData, strcmp, getStageSelect, getStageSelect (stageObjRecord; getSaveData's +0x4bc is getStageSelect's record)

bool StageCharacter::hasAlteredVoice()
{
    switch (getStageSelect()->stage) {
    case 0x1c:
    case 0x18:
        return m_name[2] == 'E' && strcmp(&m_name[5], "_TKA") == 0;
    case 0x11:
    case 0xe:
        return stageObjRecord(m_itemDef)->kind == CharacterKind_0;
    case 5:
        return stageObjRecord(m_itemDef)->kind == CharacterKind_1;
    default:
        return false;
    }
}

// The party's view looks through this character.
// FS2 0x00212300
// callcheck: isRunning, isRunning, getCompanion, getKind, getKind (getPlayerAltFlag)

static bool isViewedThrough(StageCharacter *character)
{
    if (gStageScene->getPlayerAltFlag() == 0)
        return false;
    StageObject *companion = gStageScene->party->getCompanion();
    if (companion != nullptr && companion->getKind() != StageObject_Character)
        companion = nullptr;
    return companion == character;
}

// The breath that fits: a winded PC or NPC, or, seen through, anyone; out
// when it has to be built; null none.
// FS2 0x002a2280
// callcheck: getLifeState, FUN_00212300, getSoundKind, getSoundKind, getSoundKind, getStageSelect, sprintf, sprintf (isViewedThrough; the ROM asks getSoundKind again where the host keeps it, the last of them with rows the three kinds reaching it cannot run)

static const char *getBreathName(StageCharacter *character, char *out, int winded)
{
    static const char *const kBreaths[] = {         /* data 0x31ac54: 0 has no entry */
        nullptr, "NPC_BRH_L", "NPC_BRH_H",
    };
    static const char *const kViewedBreaths[] = {   /* data 0x2fb710 */
        "NPC_BRH_J", "NPC_BRH_L_J", "NPC_BRH_H_J",
    };
    if (character->getLifeState() != 2)
        return nullptr;
    const int kind = character->getSoundKind();
    if (!isViewedThrough(character)) {
        if (kind == SoundKind_Tks)
            return "TKS_BRH_L";
        if ((kind == SoundKind_Npc || kind == SoundKind_Pc) && winded != 0)
            return kBreaths[winded] + (kind == SoundKind_Pc ? 1 : 0);
        return nullptr;
    }
    switch (kind) {
    case SoundKind_Pc:
        return kViewedBreaths[winded] + 1;
    case SoundKind_Npc:
        return kViewedBreaths[winded];
    case SoundKind_Enemy:
    case SoundKind_Umi:
    case SoundKind_Ryo: {
        char pace;
        switch (character->getBrainPace()) {
        case 1: case 6:
            pace = 'N';
            break;
        case 2: case 3: case 4:
            pace = 'C';
            break;
        case 5:
            pace = 'B';
            break;
        default:
            return nullptr;
        }
        char enemy[kVoiceNameSize];                 // PC: the ROM's 4 bytes end at the frame's top
        const char *owner = kind == SoundKind_Umi ? "UMI" : "RYO";
        if (kind == SoundKind_Enemy) {
            sprintf(enemy, "E%02d", stageObjRecord(character->m_itemDef)->seType);
            owner = enemy;
        }
        sprintf(out, "%s_BRH_%c_J", owner, pace);
        return out;
    }
    case SoundKind_Tks:
        return "TKS_BRH_J";
    case SoundKind_Boss1:
        return "BOSS1_BT_J";
    default:
        return nullptr;
    }
}

// What the boss's motion sounds like: 1 stands, 2 and 3 its paces, 4 strains;
// a motion on neither list keeps what plays.
static int boss1MoveState(int motion, int current)
{
    if (isHoldableMotion(motion))
        return 1;
    if (isMovingMotion(motion)) {
        const bool heavy = motion == 0x1a || motion == 0x19 || motion == 0x16 || motion == 10 || motion == 9
                           || motion == 0x2d || motion == 0x24;
        return heavy ? 3 : 2;
    }
    return motion == 0xe4 ? 4 : current;
}

// Sheet kind 0xb, the boss.
static void updateBoss1Sound(StageCharacter *character)
{
    static const char *const kSounds[] = {
        nullptr, "BOSS1_STAY", "BOSS1_MVL", "BOSS1_MVH", "BOSS1_STR",
    };
    const int state = boss1MoveState(character->m_motion->motionId, character->m_moveSoundState);
    if (character->m_moveSoundState == state)
        return;
    character->m_moveSoundState = state;
    if (character->m_moveSound != nullptr)
        character->stopSound(character->m_moveSound);
    if (state >= 1 && state <= 4)
        character->m_moveSound = character->playNamedHeard2(kSounds[state], kSoundAtOrigin, character);
    else if (state == 0)
        character->m_moveSound = nullptr;
}

// Sheet kind 0xd goes on a surface: the state is that surface.
static void updateSurfaceMoveSound(StageCharacter *character)
{
    int state = 0;
    if (isMovingMotion(character->m_motion->motionId)) {
        const StageCollisionFace *floor = character->m_ground->floorFace;
        state = floor != nullptr ? floor->surface : character->m_moveSoundState;
    }
    if (character->m_moveSoundState == state)
        return;
    character->m_moveSoundState = state;
    StageSound *sound = gStageScene->sound;
    character->m_moveSound = sound->findSource(character->m_moveSound);
    const SeEntry *entry = state != 0 ? getSurfaceMoveSound(state) : nullptr;
    if (entry != nullptr) {
        if (character->m_moveSound == nullptr
            || (const char *)character->m_moveSound->m_entry->name != (const char *)entry->name) {
            character->m_moveSound = character->playEntryHeard(entry, kSoundAtOrigin, character);
        }
        return;
    }
    if (character->m_moveSound != nullptr)
        character->stopSound(character->m_moveSound);
    character->m_moveSound = nullptr;
}

// Sheet kinds 3 and 7 are heard while they move.
static void updateGhostMoveSound(StageCharacter *character, int sheetKind)
{
    const int state = isMovingMotion(character->m_motion->motionId) ? 2 : 0;
    if (character->m_moveSoundState == state)
        return;
    character->m_moveSoundState = state;
    if (character->m_moveSound != nullptr)
        character->stopSound(character->m_moveSound);
    if (state == 2) {
        const char *name = sheetKind == CharacterKind_Srt ? "SRT_MOVE" : "YRT_MOVE";
        character->m_moveSound = character->playNamedHeard2(name, kSoundAtOrigin, character);
    } else {
        character->m_moveSound = nullptr;
    }
}

// FS2 0x002a4a40
// callcheck: isCuePlaying, findSource, getSoundKind, getStageSelect, sprintf, strcmp, stopSound, FUN_002a2280, findSource, strcmp, stopSound, find, playNamedHeard2, stopSound, getStageSelect, stopSound, playNamedHeard2, playNamedHeard2, playNamedHeard2, playNamedHeard2, findSource, FUN_002cede0, playEntryHeard, stopSound, stopSound, getStageSelect, playNamedHeard2, getLifeState, getSoundKind, isPlayer, isPartner, getStageSelect, sprintf, getStageSelect, sprintf, getStageSelect, sprintf, randNext, randNext, randNext, playVoice (updateBoss1Sound, updateSurfaceMoveSound, updateGhostMoveSound, one call each of what the ROM repeats per branch; its motion lists are isHoldableMotion and isMovingMotion, inline)

void StageCharacter::updateSounds()
{
    constexpr int kSecondsWalking = 20;     // up to 10 more
    constexpr int kSecondsAlert = 10;       // up to 10 more
    constexpr int kFramesPerSecond = 25;
    StageSound *sound = gStageScene->sound;

    // A voice that ended lets go; a bite's stops with the bite.
    if (m_soundState == kSoundCue) {
        if (!sound->isCuePlaying(static_cast<const SeEntry *>(m_soundHandle)->name, this)) {
            m_soundPriority = 0x7fffffff;
            m_soundState = kSoundNone;
        }
    } else if (m_soundState == kSoundSource) {
        StageSoundSource *source = sound->findSource((StageSoundSource *)m_soundHandle);
        m_soundHandle = source;
        if (source == nullptr) {
            m_soundPriority = 0x7fffffff;
            m_soundState = kSoundNone;
        } else if (source->m_entry->resumable && getSoundKind() == SoundKind_Enemy) {
            char bite[kVoiceNameSize];
            sprintf(bite, "E%02d_BITE", stageObjRecord(m_itemDef)->seType);
            const int id = m_motion->motionId;
            if (strcmp(source->m_entry->name, bite) == 0 && id != 0x9f && id != 0x9e)
                stopSound(source);
        }
    }

    // The breath, while no voice plays, at the head.
    char breathName[kVoiceNameSize];
    const char *breath = m_soundState == kSoundNone ? getBreathName(this, breathName, m_winded) : nullptr;
    if (m_breathSound != nullptr) {
        StageSoundSource *source = sound->findSource(m_breathSound);
        if (source == nullptr) {
            m_breathSound = nullptr;
        } else if (breath != nullptr && strcmp(source->m_entry->name, breath) == 0) {
            breath = nullptr;
        } else {
            stopSound(source);
            m_breathSound = nullptr;
        }
    }
    if (breath != nullptr) {
        static const char kHead[] = "bn_head";  /* data 0x30a630 */
        m_breathSound = playNamedHeard2(breath, kSoundAtOrigin, m_model->boneMap.find(kHead)->value);
    }

    // A motion's own sound ends with the motion.
    if (m_moveSoundState > 4 && m_motion->motionId + kMoveSoundMotionBase != m_moveSoundState) {
        m_moveSoundState = 0;
        stopSound(m_moveSound);
    }

    const int sheetKind = stageObjRecord(m_itemDef)->kind;
    if (sheetKind == CharacterKind_NoShadow)
        updateBoss1Sound(this);
    else if (sheetKind == CharacterKind_HideD)
        updateSurfaceMoveSound(this);
    else if (sheetKind == CharacterKind_Yrt || sheetKind == CharacterKind_Srt)
        updateGhostMoveSound(this, sheetKind);

    // What it says by itself, every so often while it lives.
    if (m_soundState != kSoundNone || getLifeState() != 2 || m_voiceOn == 0)
        return;
    if (m_voiceDelay != 0) {
        if (palTicks() != 0)   // PC: game_rate, the delay counts 25 Hz frames (kFramesPerSecond; game/gamerate.h)
            m_voiceDelay--;
        return;
    }
    char buffer[kVoiceNameSize];
    const char *name = nullptr;
    const int pace = getBrainPace();
    const int soundKind = getSoundKind();
    if ((soundKind == SoundKind_Ryo || soundKind == SoundKind_Enemy) && !isPlayer() && !isPartner()) {
        const char *format = nullptr;
        switch (pace) {
        case 1: case 6:
            format = "SE%02d_WND";
            break;
        case 2: case 3: case 4:
            format = "SE%02d_CAU";
            break;
        case 5:
            format = "SE%02d_ATK";
            break;
        default:
            break;
        }
        if (format != nullptr) {
            sprintf(buffer, format, stageObjRecord(m_itemDef)->seType);
            name = buffer;
        }
    }
    int seconds = 30;
    switch (pace) {
    case 1: case 6:
        seconds = randNext() % 11 + kSecondsWalking;
        break;
    case 2: case 3: case 4: case 5:
        seconds = randNext() % 11 + kSecondsAlert;
        break;
    default:
        break;
    }
    if (name != nullptr)
        playVoice(name);
    m_voiceDelay = seconds * kFramesPerSecond;
}

// FS2 0x002a6380
// callcheck: stopCue, findSource, stopSource

void StageCharacter::stopBodySounds()
{
    StageSound *sound = gStageScene->sound;
    if (m_soundState == kSoundCue) {
        sound->stopCue(this, static_cast<const SeEntry *>(m_soundHandle));
    } else if (m_soundState == kSoundSource) {
        if (StageSoundSource *source = sound->findSource((StageSoundSource *)m_soundHandle))
            sound->stopSource(source);
    } else {
        return;
    }
    m_soundState = kSoundNone;
    m_soundPriority = 0x7fffffff;
    m_soundHandle = nullptr;
}

// FS2 0x002a6830
// callcheck: stopCue, findSource, stopSource, stopCuesOf, stopSound, stopSound (stopBodySounds, inline in the ROM)

void StageCharacter::stopSounds()
{
    stopBodySounds();
    gStageScene->sound->stopCuesOf(this);
    m_moveSoundState = 0;
    if (m_moveSound != nullptr)
        stopSound(m_moveSound);
    m_moveSound = nullptr;
    if (m_breathSound != nullptr)
        stopSound(m_breathSound);
    m_breathSound = nullptr;
}

// FS2 0x002a6910
// callcheck: randNext

void StageCharacter::initSounds()
{
    m_soundState = kSoundNone;
    m_soundPriority = 0x7fffffff;
    m_soundHandle = nullptr;
    m_breathSound = nullptr;
    m_moveSoundState = 0;
    m_moveSound = nullptr;
    m_voiceDelay = (randNext() % 30) * 10;
    m_voiceOn = 1;
}
