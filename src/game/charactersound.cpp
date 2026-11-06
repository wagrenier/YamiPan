/* ==========================================================================
 *  game/charactersound.cpp (FS2 0x2a25b0..0x2a29f0, 0x2a31b0..0x2a49c0): the
 *  cues a character's motions and hits ask for, by who the character is.
 *
 *  The cue's name comes from the sheet: a PC and an NPC have one name each
 *  (the ROM's strings are all NPC_..., the PC's is the same string one byte
 *  on), a shibito's has its sheet's SEtype in it (E%02d_...). Sound kind 7
 *  has names here and getSoundKind never answers it: the ROM's, kept.
 * ======================================================================== */
#include "game/gamehooks.h"
#include "game/stageitem.h"

#include <stdio.h>

constexpr int kVoiceNameSize = 0x20;    // the ROM's buffers

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
    if (m_44c == 0)
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
