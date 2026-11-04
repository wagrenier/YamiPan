/* ==========================================================================
 *  game/packtables.cpp
 *
 *  The game's parameter tables, read once at boot from dataEU/packgameEU.pak
 *  (game/packtables.h). The CSV sheets are Shift-JIS, so the column names
 *  below are written in bytes, each with its text in a comment. The ROM's
 *  std::map / std::vector / std::string instances 0x2c0d00..0x2c3d30 are the
 *  host's <map>, <vector> and <string>:
 *    0x2c1430 NameMap::operator[], 0x2c15c0 NameMap::find, 0x2c1b10
 *    ListMap::operator[], 0x2c1670 / 0x2c16d0 clear, 0x2c1c90 ListMap::find,
 *    0x2c1d10 / 0x2c2350 the reader's std::set<int>, 0x2c1e10 EntryList::resize.
 *  PC: a sheet row whose number is out of its table is skipped; the ROM
 *  takes its record as null and writes through it.
 * ======================================================================== */
#include "game/packtables.h"

#include <set>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "game/fileloader.h"
#include "game/gamehooks.h"
#include "game/stringlist.h"

// The pack's items (by id).
enum PackTableItem
{
    PackItem_Characters    = 0,     // three sheets, one per StageKind
    PackItem_Items         = 3,
    PackItem_Doors         = 4,
    PackItem_Motions       = 5,
    PackItem_MotionOffsets = 6,     // two lists
    PackItem_MotionModels  = 8,
    PackItem_MotionBones   = 9,     // kMotionBoneLists lists per group
};

static const char kColNo[] = "No";  /* data 0x30b450 */
static const char kColLabel[] = "\x83\x89\x83\x78\x83\x8b";    // ラベル  /* data 0x30b590 */
static const char kColCtype[] = "ctype";  /* data 0x30b598 */
static const char kColSeType[] = "SEtype";  /* data 0x30b5a0 */
static const char kColWeaponLabel[] = "\x95\x90\x8a\xed\x83\x89\x83\x78\x83\x8b";    // 武器ラベル  /* data 0x30b5a8 */
static const char kColFlashlightLabel[] = "\x89\xf9\x92\x86\x93\x64\x93\x94\x83\x89\x83\x78\x83\x8b";    // 懐中電灯ラベル  /* data 0x30b5b8 */
static const char kColLight[] = "\x8c\x79";    // 軽  /* data 0x30b5c8 */
static const char kColHeavy[] = "\x8f\x64";    // 重  /* data 0x30b5d0 */
static const char kColShort[] = "\x92\x5a";    // 短  /* data 0x30b5d8 */
static const char kColRifle[] = "\x83\x89";    // ラ  /* data 0x30b5e0 */
static const char kColPc[] = "PC";  /* data 0x30b5e8 */
static const char kColNp[] = "NP";  /* data 0x30b5f0 */
static const char kColCorpse[] = "\x8e\x72";    // 屍  /* data 0x30b5f8 */
static const char kColDark[] = "\x88\xc5";    // 闇  /* data 0x30b600 */
static const char kColItemLabel[] = "LABEL";  /* data 0x30b458 */
static const char kColItemKind[] = "\x8e\xed\x95\xca";    // 種別  /* data 0x30b460 */
static const char kColItemEffect[] = "\x83\x41\x83\x43\x83\x65\x83\x80\x8c\xf8\x89\xca";    // アイテム効果  /* data 0x30b468 */
static const char kColItemStart[] = "\x93\xfc\x8e\xe8\x90\x94";    // 入手数  /* data 0x30b478 */
static const char kColItemCarry[] = "\x8f\x8a\x8e\x9d\x8f\xe3\x8c\xc0";    // 所持上限  /* data 0x30b480 */
static const char kColSePickup[] = "\x53\x45\x83\x41\x83\x43\x83\x65\x83\x80\x8e\xe6\x93\xbe";    // SEアイテム取得  /* data 0x30b550 */
static const char kColSeHit[] = "\x53\x45\x8d\x55\x8c\x82\x83\x71\x83\x62\x83\x67";    // SE攻撃ヒット  /* data 0x30b560 */
static const char kColSeLand[] = "\x53\x45\x93\x8a\x82\xb0\x92\x85\x92\x6e";    // SE投げ着地  /* data 0x30b570 */
static const char kColDoorName[] = "Name";  /* data 0x30b880 */
static const char kColDoorKey[] = "\x8c\xae\x91\xae\x90\xab";    // 鍵属性  /* data 0x30b888 */
static const char kColDoorWindow[] = "\x91\x8b\x8f\xf3\x91\xd4";    // 窓状態  /* data 0x30b890 */
static const char kColDoorHp[] = "\x83\x68\x83\x41\x48\x50";    // ドアHP  /* data 0x30b898 */
static const char kColDoorState[] = "\x8f\x89\x8a\xfa\x8f\xf3\x91\xd4";    // 初期状態  /* data 0x30b8a0 */
static const char kColDoorSe[] = "\x53\x45\x83\x89\x83\x78\x83\x8b";    // SEラベル  /* data 0x30b8b0 */
static const char kColDoorLight[] = "light";  /* data 0x30b8d0 */
static const char kColDoorObject[] = "object";  /* data 0x30b8d8 */
static const char kColDoorParticle[] = "particle";  /* data 0x30b8e0 */
static const char kColIndex[] = "\x83\x43\x83\x93\x83\x66\x83\x4e\x83\x58";    // インデクス  /* data 0x30b8f0 */
static const char kColUpperLabel[] = "\x83\x89\x83\x78\x83\x8b\x81\x69\x8f\xe3\x94\xbc\x90\x67\x81\x6a";    // ラベル（上半身）  /* data 0x30b900 */
static const char kColRifleLabel[] = "\x83\x89\x83\x78\x83\x8b\x81\x69\x83\x89\x83\x43\x83\x74\x83\x8b\x83\x5e\x83\x43\x83\x76\x25\x64\x81\x6a";    // ラベル（ライフルタイプ%d）  /* data 0x30b920 */
static const char kColLinkFunction[] = "\x83\x8a\x83\x93\x83\x4e\x83\x74\x83\x40\x83\x93\x83\x4e\x83\x56\x83\x87\x83\x93";    // リンクファンクション  /* data 0x30b940 */
static const char kColShotLabel[] = "S%d";  /* data 0x30b958 */
static const char kColWaitEnd[] = "\x8d\xc4\x90\xb6\x8f\x49\x97\xb9\x91\xd2\x82\xbf";    // 再生終了待ち  /* data 0x30b968 */
static const char kColGroundCheck[] = "\x90\xda\x92\x6e\x94\xbb\x92\xe8";    // 接地判定  /* data 0x30b978 */
static const char kColWallCheck[] = "\x95\xc7\x94\xbb\x92\xe8";    // 壁判定  /* data 0x30b988 */
static const char kColRepelLevel[] = "\x83\x4c\x83\x83\x83\x89\x94\xbd\x94\xad\x4c\x56";    // キャラ反発LV  /* data 0x30b990 */
static const char kColIk[] = "IK";  /* data 0x30b9a0 */
static const char kColDamageKind[] = "\x83\x5f\x83\x81\x81\x5b\x83\x57\x8e\xed\x95\xca";    // ダメージ種別  /* data 0x30b9a8 */
static const char kColDamageStep[] = "\x83\x5f\x83\x81\x81\x5b\x83\x57\x90\x84\x88\xda";    // ダメージ推移  /* data 0x30b9b8 */
static const char kColSameRequest[] = "\x93\xaf\x88\xea\x83\x8a\x83\x4e\x83\x47\x83\x58\x83\x67";    // 同一リクエスト  /* data 0x30b9c8 */
static const char kColLandSeKind[] = "\x90\xda\x92\x6e\x53\x45\x8e\xed\x95\xca";    // 接地SE種別  /* data 0x30b9d8 */
static const char kColHeadSway[] = "\x93\xaa\x90\x55\x82\xe8";    // 頭振り  /* data 0x30b9e8 */
static const char kColUpperSway[] = "\x8f\xe3\x94\xbc\x90\x67\x90\x55\x82\xe8";    // 上半身振り  /* data 0x30b9f0 */
static const char kColCameraMode[] = "\x83\x4a\x83\x81\x83\x89\x83\x82\x81\x5b\x83\x68\x90\xd8\x91\xd6";    // カメラモード切替  /* data 0x30ba00 */
static const char kColFirstPersonZoom[] = "\x8e\xe5\x8a\xcf\x83\x59\x81\x5b\x83\x80";    // 主観ズーム  /* data 0x30ba18 */
static const char kColFlashlightToggle[] = "\x89\xf9\x92\x86\x93\x64\x93\x94\x4f\x4e\x2f\x4f\x46\x46";    // 懐中電灯ON/OFF  /* data 0x30ba28 */
static const char kColFlashlightControl[] = "\x89\xf9\x92\x86\x93\x64\x93\x94\x90\xa7\x8c\xe4";    // 懐中電灯制御  /* data 0x30ba38 */
static const char kColHeadSwayBone[] = "\x93\xaa\x90\x55\x82\xe8\x83\x7b\x81\x5b\x83\x93\x98\x41\x93\xae";    // 頭振りボーン連動  /* data 0x30ba50 */
static const char kColNextMotion[] = "\x91\x4a\x88\xda\x83\x82\x81\x5b\x83\x56\x83\x87\x83\x93";    // 遷移モーション  /* data 0x30ba68 */
static const char kColParam1[] = "parameter 1";  /* data 0x30b490 */
static const char kColParam2[] = "parameter 2";  /* data 0x30b4a0 */
static const char kColParam3[] = "parameter 3";  /* data 0x30b4b0 */
static const char kColParam4[] = "parameter 4";  /* data 0x30b4c0 */
static const char kColParam5[] = "parameter 5";  /* data 0x30b4d0 */
static const char kColParam6[] = "parameter 6";  /* data 0x30b4e0 */
static const char kColParam7[] = "parameter 7";  /* data 0x30b4f0 */
static const char kColParam8[] = "parameter 8";  /* data 0x30b500 */
static const char kColParam9[] = "parameter 9";  /* data 0x30b510 */
static const char kColParam10[] = "parameter 10";  /* data 0x30b520 */
static const char kColParam11[] = "parameter 11";  /* data 0x30b530 */
static const char kColParam12[] = "parameter 12";  /* data 0x30b540 */
static const char kColStartMotion[] = "\x8f\x89\x8a\xfa\x4d";    // 初期M  /* data 0x30b608 */
static const char kColAiType[] = "\x41\x49\xc0\xb2\xcc\xdf";    // AIﾀｲﾌﾟ  /* data 0x30b610 */
static const char kColAiLevel[] = "AILV";  /* data 0x30b618 */
static const char kColSight[] = "\x8e\x8b\x8a\x45";    // 視界  /* data 0x30b620 */
static const char kColHearing[] = "\x89\xb9\x8a\xb4";    // 音感  /* data 0x30b628 */
static const char kColHealth[] = "\x91\xcc\x97\xcd";    // 体力  /* data 0x30b630 */
static const char kColEndurance[] = "\x91\xcf\x8b\x76\x97\xcd";    // 耐久力  /* data 0x30b638 */
static const char kColLeftArm[] = "\x8d\xb6\x98\x72";    // 左腕  /* data 0x30b640 */
static const char kColRightArm[] = "\x89\x45\x98\x72";    // 右腕  /* data 0x30b648 */
static const char kColRightLeg[] = "\x89\x45\x91\xab";    // 右足  /* data 0x30b650 */
static const char kColLeftLeg[] = "\x8d\xb6\x91\xab";    // 左足  /* data 0x30b658 */
static const char kColRecovery[] = "\x89\xf1\x95\x9c";    // 回復  /* data 0x30b660 */
static const char kColLightResist[] = "\x8c\xf5\x91\xcf\x90\xab";    // 光耐性  /* data 0x30b668 */
static const char kColLightDamage[] = "\x8c\xf5\xc0\xde\xd2\xb0\xbc\xde";    // 光ﾀﾞﾒｰｼﾞ  /* data 0x30b670 */
static const char kColLightFlinch[] = "\x8c\xf5\x8b\xaf\x82\xdd";    // 光怯み  /* data 0x30b680 */
static const char kColUnarmedAttack[] = "\x91\x66\x8e\xe8\x8d\x55\x8c\x82";    // 素手攻撃  /* data 0x30b688 */
static const char kColUnarmedMsp[] = "\x91\x66\x8e\xe8\x4d\x53\x50";    // 素手MSP  /* data 0x30b698 */
static const char kColUnarmedMspDrop[] = "\x91\x66\x8e\xe8\x4d\x53\x50\x8c\xb8\x8f\xad\x92\x6c";    // 素手MSP減少値  /* data 0x30b6a0 */
static const char kColWeaponMspScale[] = "\x95\x90\x8a\xed\x4d\x53\x50\x8f\x43\x90\xb3\x92\x6c";    // 武器MSP修正値  /* data 0x30b6b0 */
static const char kColMeleeAttackScale[] = "\x91\xc5\x8c\x82\x8d\x55\x8c\x82\x97\xcd\x8f\x43\x90\xb3\x92\x6c";    // 打撃攻撃力修正値  /* data 0x30b6c0 */
static const char kColShotAttackScale[] = "\x8e\xcb\x8c\x82\x8d\x55\x8c\x82\x97\xcd\x8f\x43\x90\xb3\x92\x6c";    // 射撃攻撃力修正値  /* data 0x30b6e0 */
static const char kColBite[] = "\x82\xa9\x82\xdd\x82\xc2\x82\xab";    // かみつき  /* data 0x30b6f8 */
static const char kColAttackCheck[] = "\x8d\x55\x8c\x82\x94\xbb\x92\xe8";    // 攻撃判定  /* data 0x30b708 */
static const char kColWalkSpeed[] = "\x88\xda\x93\xae\x91\xac\x93\x78";    // 移動速度  /* data 0x30b718 */
static const char kColRunSpeed[] = "\x91\x96\x82\xe8\x91\xac\x93\x78";    // 走り速度  /* data 0x30b728 */
static const char kColTurnSpeed[] = "\x90\xf9\x89\xf1\x91\xac\x93\x78";    // 旋回速度  /* data 0x30b738 */
static const char kColFlashlight[] = "\x89\xf9\x92\x86\x93\x64\x93\x94";    // 懐中電灯  /* data 0x30b748 */
static const char kColReviveTime[] = "\x95\x9c\x8a\x88\x8e\x9e\x8a\xd4";    // 復活時間  /* data 0x30b758 */
static const char kColFlinchResist[] = "\x82\xd0\x82\xe9\x82\xdd\x91\xcf\x90\xab\x92\x6c";    // ひるみ耐性値  /* data 0x30b768 */
static const char kColDecoyResist[] = "\x97\x7a\x93\xae\x91\xcf\x90\xab";    // 陽動耐性  /* data 0x30b778 */
static const char kColControlResist[] = "\x91\x80\x82\xe8\x91\xcf\x90\xab";    // 操り耐性  /* data 0x30b788 */
static const char kColTrapResist[] = "\xe3\xa9\x91\xcf\x90\xab";    // 罠耐性  /* data 0x30b798 */
static const char kColShot[] = "\x8e\xcb\x8c\x82";    // 射撃  /* data 0x30b7a0 */
static const char kColShotInterval[] = "\x8e\xcb\x8c\x82\x8a\xd4\x8a\x75";    // 射撃間隔  /* data 0x30b7a8 */
static const char kColAttackInterval[] = "\x8d\x55\x8c\x82\x8a\xd4\x8a\x75";    // 攻撃間隔  /* data 0x30b7b8 */
static const char kColBiteInterval[] = "\x8a\x9a\x82\xdd\x82\xc2\x82\xab\x8a\xd4\x8a\x75";    // 噛みつき間隔  /* data 0x30b7c8 */
static const char kColReload[] = "\x83\x8a\x83\x8d\x81\x5b\x83\x68";    // リロード  /* data 0x30b7d8 */
static const char kColStepLevel[] = "\x92\x69\x8d\xb7\x4c\x76";    // 段差Lv  /* data 0x30b7e8 */
static const char kColLadder[] = "\x83\x6e\x83\x56\x83\x53";    // ハシゴ  /* data 0x30b7f0 */
static const char kColJumpLevel[] = "\xbc\xde\xac\xdd\xcc\xdf\x4c\x76";    // ｼﾞｬﾝﾌﾟLv  /* data 0x30b7f8 */
static const char kColStepDown[] = "\x92\x69\x8d\xb7\x89\xba\x82\xe8";    // 段差下り  /* data 0x30b808 */
static const char kColBloodSpray[] = "\x8c\x8c\x82\xb5\x82\xd4\x82\xab";    // 血しぶき  /* data 0x30b818 */
static const char kColUnarmedCombo[] = "\x91\x66\x8e\xe8\x83\x52\x83\x93\x83\x7b";    // 素手コンボ  /* data 0x30b828 */
static const char kColWeaponCombo[] = "\x95\x90\x8a\xed\x83\x52\x83\x93\x83\x7b";    // 武器コンボ  /* data 0x30b838 */
static const char kColDrivingSkill[] = "\x89\x5e\x93\x5d\x83\x58\x83\x4c\x83\x8b";    // 運転スキル  /* data 0x30b848 */
static const char kColSnipeLerp[] = "\x91\x5f\x8c\x82\x95\xe2\x8a\xd4\x97\xa6";    // 狙撃補間率  /* data 0x30b858 */
static const char kColSnipeShake[] = "\x91\x5f\x8c\x82\x83\x75\x83\x8c\x97\xa6";    // 狙撃ブレ率  /* data 0x30b868 */

static const char kLineBreaks[] = "\r\n";   /* data 0x30bc30, 0x30b580 */
static const char kComma[]      = ",";      /* data 0x30bc28 */
static const char kDoorSeFormat[] = "DOOR_%02d"; /* data 0x30b8c0 */

constexpr int kNameSize = 0x10;

// The item sheet's kinds, by ItemKind (data 0x30b878, 0x30ba78..). A kind
// that is none of them keeps the row before's.
static const char *const kItemKinds[] = { "LW", "HW", "G3", "G9", "G7", "R5", "R7", "B3", "B9", "B5", "B7", "II", "QI", "KI", "T", "OI" };

// What a motion's S0..S3 can name (MotionDef::shotKind); anything else is
// kMotionShotOther. The ROM's chain of compares in parse. 6 is unused, as
// BM_UPL and BM_UPL2 share 5.
// layout: host -- the ROM has no such table.
struct MotionShotKind
{
    const char *name;
    int         kind;
};
static const MotionShotKind kMotionShotKinds[] = {
    { "SHOUT", 0x0 },
    { "IDLE01", 0x1 },
    { "IDLE02", 0x2 },
    { "PC_DW1", 0x3 },
    { "PC_CLASH", 0x4 },
    { "BM_UPL", 0x5 },
    { "BM_UPL2", 0x5 },
    { "BM_UPM", 0x7 },
    { "BM_UPH", 0x8 },
    { "BM_LFUP", 0x9 },
    { "BM_DWL", 0xa },
    { "BM_DWM", 0xb },
    { "BM_DWH", 0xc },
    { "JUMP", 0xd },
    { "AT_NW1", 0xe },
    { "AT_NW2", 0xf },
    { "AT1", 0x10 },
    { "AT2", 0x11 },
    { "AT3", 0x12 },
    { "AT4", 0x13 },
    { "AT5", 0x14 },
    { "DOWN2", 0x15 },
    { "TRP_O", 0x16 },
    { "THROW", 0x17 },
    { "BITE", 0x18 },
    { "BITE2", 0x19 },
    { "UNTIE", 0x1a },
    { "DMG_L", 0x1b },
    { "DMG_H", 0x1c },
    { "DIE", 0x1d },
    { "GETUP", 0x1e },
    { "SURP", 0x1f },
    { "PANIC", 0x20 },
    { "END", 0x21 },
    { "FALLDW", 0x22 },
    { "JOB01", 0x23 },
    { "JOB02", 0x24 },
    { "JOB03", 0x25 },
    { "JOB04", 0x26 },
};
constexpr int kMotionShotOther = 0x27;

MotionDef gMotionDefs[kMotionDefCount];   /* bss 0x577b20 */
DoorRecord gDoorRecords[kDoorRecords];    /* bss 0x537d80 */
ItemDef gItemDefs[kItemDefCount];         /* bss 0x574c10 */
WeaponDef gWeaponDefs[kWeaponDefCount];   /* bss 0x576010 */
StageObjRecord gStageObjRecords[kStageObjRecordSets][kStageObjRecordCount]; /* bss 0x539b10 */

// A sheet's row, read by column name. A column the sheet lacks reads the
// first (NameMap::operator[] adds it as 0), as in the ROM.
// layout: host -- the port's helper.
struct PackRow
{
    EntryList &cells;
    NameMap   &columns;

    PackEntry  &cell(const char *column) const { return cells[columns[column]]; }
    int         number(const char *column) const { return cell(column).number; }
    const char *text(const char *column) const { return cell(column).text.c_str(); }
};

// ──────────────────────────────────────────────────────────────────────
// Adds text's fields to fields: cut at each delimiter, a run of them cut
// once when collapse is set. The last field is always added.
// FS2 0x002c18a0
// callcheck: operator_new[], FUN_002c2480, strlen, reserve, replace_range, FUN_002c3160, free, operator_delete[] (std::string and vector::push_back)

static void splitFields(std::vector<std::string> *fields, const std::string &text,
                        const char *delimiters, bool collapse)
{
    std::string field;
    const char *p = text.c_str();
    while (*p != '\0') {
        if (strchr(delimiters, *p) == nullptr) {
            field += *p++;
            continue;
        }
        fields->push_back(field);
        field.clear();
        p++;
        if (collapse) {
            while (*p != '\0' && strchr(delimiters, *p) != nullptr)
                p++;
        }
    }
    fields->push_back(field);
}

// ──────────────────────────────────────────────────────────────────────
// One sheet into rows and columns: the first line with a first field names
// the columns, each next one is a row under the number in its first field.
// Once a number came twice, a row taken moves to the next free number. The
// item loses its last byte to the terminator. Answers false for no item.
// FS2 0x002c0ef0
// callcheck: FUN_002c2410, strlen, reserve, replace_range, FUN_002c18a0, free, FUN_002c1830, atoi, FUN_002c1d10, FUN_002c2350, FUN_002c1c90, FUN_002c1b10, FUN_002c1e10, FUN_002c1d90, FUN_002c1430, FUN_002c1ee0, FUN_002c20f0 (std::string, std::set and the maps)

static bool readSheet(ListMap *rows, NameMap *columns, void *item, int size)
{
    if (size == 0)
        return false;

    char *text = (char *)item;
    text[size - 1] = '\0';
    std::vector<std::string> lines;
    splitFields(&lines, text, kLineBreaks, true);

    bool haveHeader = false;
    int repeats = 0;
    size_t columnCount = 0;
    std::set<int> seen;
    for (const std::string &line : lines) {
        std::vector<std::string> fields;
        splitFields(&fields, line, kComma, false);
        if (fields.empty() || fields[0].empty())
            continue;

        if (!haveHeader) {
            int index = 0;
            for (const std::string &name : fields)
                (*columns)[name] = index++;
            haveHeader = true;
            columnCount = fields.size();
            continue;
        }

        int number = atoi(fields[0].c_str());
        if (seen.count(number) == 0)
            seen.insert(number);
        else
            repeats++;
        if (repeats > 0) {
            while (rows->find(number) != rows->end())
                number++;
        }
        EntryList &row = (*rows)[number];
        row.resize(fields.size() > columnCount ? fields.size() : columnCount);
        for (size_t i = 0; i < fields.size(); i++) {
            row[i].number = atoi(fields[i].c_str());
            row[i].text = fields[i];
        }
    }
    return true;
}

// A list item (not a sheet) in lines. PC: the ROM also asks the pack for the
// item's name and drops it (slot 4).
// callcheck: operator_new[], memset, strncpy, split, operator_delete[], FUN_001a5a60

static void readLines(StringList *lines, PackFile *pack, int item)
{
    const char *data = (const char *)pack->getItem(item);
    int size = pack->getItemSize(item);
    char *text = new char[size + 1];
    memset(text, 0, size + 1);
    strncpy(text, data, size);
    lines->split(text, kLineBreaks);
    delete[] text;
}

static float fieldFloat(StringList &fields, int index)
{
    return (float)atof(fields.at(index)->value->m_text);
}

// The list node at index; the ROM's std::list iterator advance in the
// instance 0x2c1730.
// FS2 0x002c1730

StringList::Node *StringList::at(int index)
{
    Node *node = head;
    if (index < count) {
        for (int i = 0; i < index; i++)
            node = node->next;
    }
    return node;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002c0dc0
// callcheck: FUN_002c16b0, FUN_002c1710 (the eight maps' constructors)

PackTables::PackTables()
{
}

// FS2 0x002bcd20
// callcheck: FUN_002c1670, FUN_002c16d0 (the maps' clear)

void PackTables::clear()
{
    characterRows.clear();
    characterColumns.clear();
    itemRows.clear();
    itemColumns.clear();
    doorRows.clear();
    doorColumns.clear();
    motionRows.clear();
    motionColumns.clear();
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002bcf10: the ROM's one function of 0x3df0 bytes, cut here by sheet.
// callcheck: readSheet, clear (the sheets, in the helpers below; the same for all that follows)
// callcheck: FUN_001a5a60, FUN_002c0d00, FUN_002c0d30, FUN_002c0d40, FUN_002c0d50, FUN_002c0d80, FUN_002c0d90, FUN_002c0db0, FUN_002c0e60, FUN_002c1430, FUN_002c1670, FUN_002c16d0 (the std::map iterators and find-or-insert, a StringList's base ctor, string::c_str)
// callcheck: __truncdfsf2, at, atof, atoi, doorNumber, doorText, init, memset, operator_delete[], operator_new[], setLandSound, setPickSound, split, sprintf, sscanf, strcmp, strlen, strncpy, string_cstr, ~string, ~StringList, FUN_001723b0, FUN_001723a0, FUN_00172390, FUN_00172380, FUN_00172370, setKeyKind, setStartState, setWindowState, setHp, setSeIndex (the helpers below; the DoorRecord setters and StageObjRecord::init are plain stores there)

void PackTables::parse(PackFile *pack)
{
    parseCharacters(pack);
    parseItems(pack);
    parseDoors(pack);
    parseMotions(pack);
    parseMotionOffsets(pack);
    parseMotionModels(pack);
    parseMotionBones(pack);
    clear();
}

// The three character sheets, one record table each.

void PackTables::parseCharacters(PackFile *pack)
{
    for (int set = 0; set < kStageObjRecordSets; set++) {
        int item = PackItem_Characters + set;
        readSheet(&characterRows, &characterColumns, pack->getItem(item), pack->getItemSize(item));
        for (auto &entry : characterRows) {
            PackRow row = { entry.second, characterColumns };
            int index = row.number(kColNo) - 1;
            if (index < 0 || index >= kStageObjRecordCount)
                continue;

            StageObjRecord *rec = &gStageObjRecords[set][index];
            rec->startMotion = 1;
            strncpy(rec->name, row.text(kColLabel), kNameSize);
            rec->kind = row.number(kColCtype);
            rec->seType = row.number(kColSeType);
            strncpy(rec->items[0], row.text(kColWeaponLabel), kNameSize);
            strncpy(rec->items[1], row.text(kColFlashlightLabel), kNameSize);
            rec->lightWeapon = row.number(kColLight) & 3;
            rec->heavyWeapon = row.number(kColHeavy) & 3;
            rec->shortGun = row.number(kColShort) & 3;
            rec->rifle = row.number(kColRifle) & 3;
            rec->sides = 0;
            rec->sides |= row.number(kColPc);
            rec->sides |= row.number(kColNp) << 1;
            rec->sides |= row.number(kColCorpse) << 2;
            rec->sides |= row.number(kColDark) << 3;
        rec->startMotion = row.number(kColStartMotion);
        rec->aiType = row.number(kColAiType);
        rec->aiLevel = row.number(kColAiLevel);
        rec->sight = row.number(kColSight);
        rec->hearing = row.number(kColHearing);
        rec->health = row.number(kColHealth);
        rec->endurance = row.number(kColEndurance);
        rec->leftArm = row.number(kColLeftArm);
        rec->rightArm = row.number(kColRightArm);
        rec->rightLeg = row.number(kColRightLeg);
        rec->leftLeg = row.number(kColLeftLeg);
        rec->recovery = row.number(kColRecovery);
        rec->lightResist = row.number(kColLightResist);
        rec->lightDamage = row.number(kColLightDamage);
        rec->lightFlinch = row.number(kColLightFlinch);
        rec->unarmedAttack = row.number(kColUnarmedAttack);
        rec->unarmedMsp = row.number(kColUnarmedMsp);
        rec->unarmedMspDrop = row.number(kColUnarmedMspDrop);
        rec->weaponMspScale = row.number(kColWeaponMspScale);
        rec->meleeAttackScale = row.number(kColMeleeAttackScale);
        rec->shotAttackScale = row.number(kColShotAttackScale);
        rec->bite = row.number(kColBite);
        rec->attackCheck = row.number(kColAttackCheck);
        rec->walkSpeed = row.number(kColWalkSpeed);
        rec->runSpeed = row.number(kColRunSpeed);
        rec->turnSpeed = row.number(kColTurnSpeed);
        rec->flashlight = row.number(kColFlashlight);
        rec->reviveTime = row.number(kColReviveTime);
        rec->flinchResist = row.number(kColFlinchResist);
        rec->decoyResist = row.number(kColDecoyResist);
        rec->controlResist = row.number(kColControlResist);
        rec->trapResist = row.number(kColTrapResist);
        rec->shot = row.number(kColShot);
        rec->shotInterval = row.number(kColShotInterval);
        rec->attackInterval = row.number(kColAttackInterval);
        rec->biteInterval = row.number(kColBiteInterval);
        rec->reload = row.number(kColReload);
        rec->stepLevel = row.number(kColStepLevel);
        rec->ladder = row.number(kColLadder);
        rec->jumpLevel = row.number(kColJumpLevel);
        rec->stepDown = row.number(kColStepDown);
        rec->bloodSpray = row.number(kColBloodSpray);
        rec->unarmedCombo = row.number(kColUnarmedCombo);
        rec->weaponCombo = row.number(kColWeaponCombo);
        rec->drivingSkill = row.number(kColDrivingSkill);
        rec->snipeLerp = row.number(kColSnipeLerp);
        rec->snipeShake = row.number(kColSnipeShake);
        }
        characterRows.clear();
        characterColumns.clear();
    }
}

// The item sheet: every row an ItemDef, a weapon's row a WeaponDef too. A
// weapon naming another item in parameter 7 takes two of its parameters.

void PackTables::parseItems(PackFile *pack)
{
    readSheet(&itemRows, &itemColumns, pack->getItem(PackItem_Items), pack->getItemSize(PackItem_Items));

    int kind = 0;
    int weaponCount = 0;
    for (auto &entry : itemRows) {
        PackRow row = { entry.second, itemColumns };
        int index = row.number(kColNo) - 1;
        if (index < 0 || index >= kItemDefCount)
            continue;

        ItemDef *item = &gItemDefs[index];
        strncpy(item->name, row.text(kColItemLabel), kNameSize);
        const char *kindName = row.text(kColItemKind);
        for (int i = 0; i < (int)(sizeof(kItemKinds) / sizeof(kItemKinds[0])); i++) {
            if (strcmp(kindName, kItemKinds[i]) == 0) {
                kind = i;
                break;
            }
        }
        item->kind = kind;
        item->effect = row.number(kColItemEffect);
        item->startCount = (u_char)row.number(kColItemStart);
        item->stackKind = (u_char)row.number(kColItemCarry);
        item->setPickSound(row.text(kColSePickup));
        item->isBlade = (u_char)row.number(kColSeHit);
        item->setLandSound(row.text(kColSeLand));
        item->maxCount = (short)row.number(kColNo);

        if (kind >= ItemKind_ItemFirst)
            continue;

        if (weaponCount >= kWeaponDefCount) {
            weaponCount++;
            continue;
        }
        WeaponDef *weapon = &gWeaponDefs[weaponCount++];
        strncpy(weapon->name, row.text(kColItemLabel), kNameSize);
        weapon->param1 = row.number(kColParam1);
        weapon->param2 = row.number(kColParam2);
        weapon->range = row.number(kColParam3);
        weapon->param4 = row.number(kColParam4);
        weapon->param5 = row.number(kColParam5);
        weapon->magazine = row.number(kColParam6);
        strncpy(weapon->ammo, row.text(kColParam7), kNameSize);
        weapon->param8 = row.number(kColParam8);
        weapon->param9 = row.number(kColParam9);
        weapon->param10 = row.number(kColParam10);
        weapon->param11 = row.number(kColParam11);
        weapon->param12 = row.number(kColParam12);
        if (weapon->ammo[0] == '\0') {
            weapon->ammoParam1 = 0;
            continue;
        }
        for (auto &other : itemRows) {
            PackRow ammo = { other.second, itemColumns };
            if (ammo.cell(kColItemLabel).text == weapon->ammo) {
                weapon->ammoParam1 = ammo.number(kColParam1);
                weapon->ammoParam12 = ammo.number(kColParam12);
                break;
            }
        }
    }
}

// The door sheet. Its columns are looked up with find here: a missing one
// reads 0 / "".

// FS2 0x002bce70
// callcheck: FUN_002c15c0, string_cstr, ~string (std::map::find, the key's std::string)
static int doorNumber(NameMap &columns, EntryList &cells, const char *column)
{
    auto found = columns.find(column);
    return found == columns.end() ? 0 : cells[found->second].number;
}

// FS2 0x002bcd80
// callcheck: FUN_002c15c0, string_copy, string_cstr, ~string (std::map::find, the key's std::string)
static std::string doorText(NameMap &columns, EntryList &cells, const char *column)
{
    auto found = columns.find(column);
    return found == columns.end() ? std::string() : cells[found->second].text;
}

void PackTables::parseDoors(PackFile *pack)
{
    readSheet(&doorRows, &doorColumns, pack->getItem(PackItem_Doors), pack->getItemSize(PackItem_Doors));

    for (auto &entry : doorRows) {
        PackRow row = { entry.second, doorColumns };
        int index = row.number(kColNo) - 1;
        if (index < 0 || index >= kDoorRecords)
            continue;

        DoorRecord *door = &gDoorRecords[index];
        strncpy(door->name, row.text(kColDoorName), kNameSize);
        int key = doorNumber(doorColumns, entry.second, kColDoorKey);
        int window = doorNumber(doorColumns, entry.second, kColDoorWindow);
        int hp = doorNumber(doorColumns, entry.second, kColDoorHp);
        int state = doorNumber(doorColumns, entry.second, kColDoorState);
        std::string se = doorText(doorColumns, entry.second, kColDoorSe);
        int seIndex;
        if (sscanf(se.c_str(), kDoorSeFormat, &seIndex) == 1) {
            door->keyKind = key;
            door->startState = state;
            door->windowState = window;
            door->hp = (short)hp;
            door->seIndex = (short)seIndex;
        }
        std::string light = doorText(doorColumns, entry.second, kColDoorLight);
        strncpy(door->light, light.c_str(), kNameSize);
        door->hasObject = doorNumber(doorColumns, entry.second, kColDoorObject) != 0;
        door->value = doorNumber(doorColumns, entry.second, kColDoorParticle);
    }
}

// A MotionDef's text pointer: null for an empty text.
static const char *textOrNull(const char *text)
{
    return strlen(text) == 0 ? nullptr : text;
}

static int findShotKind(const char *name)
{
    for (const MotionShotKind &kind : kMotionShotKinds) {
        if (strcmp(name, kind.name) == 0)
            return kind.kind;
    }
    return kMotionShotOther;
}

// The motion sheet, in two passes: 遷移モーション names a motion by label.

void PackTables::parseMotions(PackFile *pack)
{
    readSheet(&motionRows, &motionColumns, pack->getItem(PackItem_Motions), pack->getItemSize(PackItem_Motions));

    char column[0x48];
    for (auto &entry : motionRows) {
        PackRow row = { entry.second, motionColumns };
        int index = row.number(kColIndex) - 1;
        if (index < 0 || index >= kMotionDefCount)
            continue;

        MotionDef *motion = &gMotionDefs[index];
        strncpy(motion->label, row.text(kColLabel), kNameSize);
        strncpy(motion->upperLabel, row.text(kColUpperLabel), kNameSize);
        for (int i = 0; i < kMotionRifleLabels; i++) {
            sprintf(column, kColRifleLabel, i);
            strncpy(motion->rifleLabel[i], row.text(column), kNameSize);
        }
        strncpy(motion->linkFunction, row.text(kColLinkFunction), kNameSize);
        for (int i = 0; i < kMotionShots; i++) {
            sprintf(column, kColShotLabel, i);
            strncpy(motion->shot[i], row.text(column), kNameSize);
        }

        motion->labelText = textOrNull(motion->label);
        motion->upperLabelText = textOrNull(motion->upperLabel);
        for (int i = 0; i < kMotionRifleLabels; i++)
            motion->rifleLabelText[i] = textOrNull(motion->rifleLabel[i]);
        motion->linkFunctionText = textOrNull(motion->linkFunction);
        for (int i = 0; i < kMotionShots; i++) {
            motion->shotText[i] = textOrNull(motion->shot[i]);
            motion->shotKind[i] = motion->shotText[i] == nullptr ? -1 : findShotKind(motion->shotText[i]);
        }

        motion->waitForEnd = row.number(kColWaitEnd) != 0;
        motion->groundCheck = row.number(kColGroundCheck);
        motion->wallCheck = row.number(kColWallCheck) != 0;
        motion->repelLevel = row.number(kColRepelLevel);
        motion->ik = row.number(kColIk) != 0;
        motion->damageKind = row.number(kColDamageKind);
        motion->damageStep = row.number(kColDamageStep);
        motion->sameRequest = row.number(kColSameRequest) != 0;
        motion->landSeKind = row.number(kColLandSeKind);
        motion->headSway = row.number(kColHeadSway) == 1;
        motion->upperSway = row.number(kColUpperSway) != 0;
        motion->cameraModeSwitch = row.number(kColCameraMode) != 0;
        motion->firstPersonZoom = row.number(kColFirstPersonZoom) != 0;
        motion->flashlightToggle = row.number(kColFlashlightToggle) != 0;
        motion->headSway2 = row.number(kColHeadSway) == 2;
        motion->flashlightControl = row.number(kColFlashlightControl) != 0;
        motion->headSwayBoneLink = row.number(kColHeadSwayBone) != 0;
    }

    for (auto &entry : motionRows) {
        PackRow row = { entry.second, motionColumns };
        int index = row.number(kColIndex) - 1;
        if (index < 0 || index >= kMotionDefCount)
            continue;

        const char *next = row.text(kColNextMotion);
        gMotionDefs[index].nextMotion = -1;
        if (strlen(next) == 0)
            continue;
        for (int i = 0; i < kMotionDefCount; i++) {
            const char *label = gMotionDefs[i].labelText;
            if (label != nullptr && strcmp(label, next) == 0) {
                gMotionDefs[index].nextMotion = i;
                break;
            }
        }
    }
}

// Items 6 and 7, lines of "label,x,y,z,x,y,z,flag" four times over: every
// motion with that label takes them.

void PackTables::parseMotionOffsets(PackFile *pack)
{
    for (int set = 0; set < kMotionOffsetSets; set++) {
        StringList lines;
        readLines(&lines, pack, PackItem_MotionOffsets + set);
        for (StringList::Node *line = lines.head; line != nullptr; line = line->next) {
            StringList fields;
            fields.split(line->value->m_text, kComma);
            for (int i = 0; i < kMotionDefCount; i++) {
                MotionDef *motion = &gMotionDefs[i];
                if (motion->labelText == nullptr
                    || strcmp(motion->labelText, fields.at(0)->value->m_text) != 0)
                    continue;
                for (int group = 0; group < 4; group++) {
                    int first = group * 6 + 1;
                    sceVu0FVECTOR &a = motion->cameraFocus[set][group];
                    a[0] = fieldFloat(fields, first);
                    a[1] = fieldFloat(fields, first + 1);
                    a[2] = fieldFloat(fields, first + 2);
                    a[3] = 0.0f;
                    sceVu0FVECTOR &b = motion->cameraEye[set][group];
                    b[0] = fieldFloat(fields, first + 3);
                    b[1] = fieldFloat(fields, first + 4);
                    b[2] = fieldFloat(fields, first + 5);
                    b[3] = 0.0f;
                    motion->cameraShift[set] = atoi(fields.at(first + 6)->value->m_text) != 0;
                }
            }
        }
    }
}

// Item 8: lines "tag,15 numbers".

void PackTables::parseMotionModels(PackFile *pack)
{
    StringList lines;
    readLines(&lines, pack, PackItem_MotionModels);
    for (StringList::Node *line = lines.head; line != nullptr; line = line->next) {
        StringList fields;
        fields.split(line->value->m_text, kComma);
        for (int i = 0; i < kMotionModelCount; i++) {
            MotionModel *model = &gMotionModels[i];
            if (model->tag == nullptr || strcmp(model->tag, fields.at(0)->value->m_text) != 0)
                continue;
            model->limits[0] = fieldFloat(fields, 1);
            model->limits[1] = fieldFloat(fields, 2);
            model->limits[2] = fieldFloat(fields, 3);
            model->a[0] = fieldFloat(fields, 4);
            model->a[1] = fieldFloat(fields, 5);
            model->a[2] = fieldFloat(fields, 6);
            model->a[3] = 1.0f;
            model->b[0] = fieldFloat(fields, 7);
            model->b[1] = fieldFloat(fields, 8);
            model->b[2] = fieldFloat(fields, 9);
            model->b[3] = 1.0f;
            model->c[0] = fieldFloat(fields, 10);
            model->c[1] = fieldFloat(fields, 11);
            model->c[2] = fieldFloat(fields, 12);
            model->c[3] = 0.0f;
            model->d[0] = fieldFloat(fields, 13);
            model->d[1] = fieldFloat(fields, 14);
            model->d[2] = fieldFloat(fields, 15);
            model->d[3] = 0.0f;
            break;
        }
    }
}

// Items 9 to 0x30, ten lists per group: lines "bone,a,b,c,d".

void PackTables::parseMotionBones(PackFile *pack)
{
    for (int list = 0; list < kMotionBoneLists; list++) {
        for (int group = 0; group < kMotionBoneGroups; group++) {
            const MotionBoneList &bones = gMotionBoneSets[group][list];
            StringList lines;
            readLines(&lines, pack, PackItem_MotionBones + group * kMotionBoneLists + list);
            for (StringList::Node *line = lines.head; line != nullptr; line = line->next) {
                StringList fields;
                fields.split(line->value->m_text, kComma);
                for (int i = 0; i < bones.count; i++) {
                    MotionBone *bone = &bones.bones[i];
                    if (strcmp(bone->name, fields.at(0)->value->m_text) != 0)
                        continue;
                    bone->a = fieldFloat(fields, 1);
                    bone->b = fieldFloat(fields, 2);
                    bone->c = fieldFloat(fields, 3);
                    bone->d = fieldFloat(fields, 4);
                    break;
                }
            }
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// The tables the lists overwrite, with the ROM's values (datadump.py).
MotionModel gMotionModels[kMotionModelCount] = {  /* data 0x2f8000 */
    { "_li_", { 1.2566371f, 1.2566371f, 1.2566371f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 50.0f, 80.0f, 400.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },  // 0x2f8000
    { "_lh_", { 1.2566371f, 1.2566371f, 1.2566371f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 50.0f, 80.0f, 400.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },  // 0x2f8050
    { "_th_", { 1.2566371f, 1.2566371f, 1.2566371f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 50.0f, 80.0f, 400.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },  // 0x2f80a0
    { "_mh_", { 1.2566371f, 1.2566371f, 1.2566371f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 50.0f, 80.0f, 400.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },  // 0x2f80f0
    { "_sh_", { 1.2566371f, 1.2566371f, 1.2566371f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 50.0f, 80.0f, 400.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },  // 0x2f8140
    { "_ch_", { 1.2566371f, 1.2566371f, 1.2566371f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 50.0f, 80.0f, 400.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },  // 0x2f8190
    { "_ms_", { 1.2566371f, 1.2566371f, 1.2566371f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 50.0f, 80.0f, 400.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },  // 0x2f81e0
    { "_ss_", { 1.2566371f, 1.2566371f, 1.2566371f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 50.0f, 80.0f, 400.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },  // 0x2f8230
    { "_zy_", { 1.2566371f, 1.2566371f, 1.2566371f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 50.0f, 80.0f, 400.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },  // 0x2f8280
    { "_ky_", { 1.2566371f, 1.2566371f, 1.2566371f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 50.0f, 80.0f, 400.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },  // 0x2f82d0
    { "_oy_", { 1.2566371f, 1.2566371f, 1.2566371f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 50.0f, 80.0f, 400.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },  // 0x2f8320
    { "_ry_", { 1.2566371f, 1.2566371f, 1.2566371f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 50.0f, 80.0f, 400.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },  // 0x2f8370
    { "_sy_", { 1.2566371f, 1.2566371f, 1.2566371f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 50.0f, 80.0f, 400.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },  // 0x2f83c0
    { "bta_", { 1.2566371f, 1.2566371f, 1.2566371f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 50.0f, 80.0f, 400.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },  // 0x2f8410
    { "kkb_", { 1.2566371f, 1.2566371f, 1.2566371f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 50.0f, 80.0f, 400.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },  // 0x2f8460
    { "tks_", { 1.2566371f, 1.2566371f, 1.2566371f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 50.0f, 80.0f, 400.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },  // 0x2f84b0
    { "cat_", { 1.2566371f, 1.2566371f, 1.2566371f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 50.0f, 80.0f, 400.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },  // 0x2f8500
    { "crw_", { 1.2566371f, 1.2566371f, 1.2566371f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 50.0f, 80.0f, 400.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },  // 0x2f8550
    { "kmb_", { 1.2566371f, 1.2566371f, 1.2566371f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 11.0f, 11.0f, 0.0f, 1.0f }, { 50.0f, 80.0f, 400.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },  // 0x2f85a0
    { nullptr, { 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f, 0.0f } },  // 0x2f85f0
};

static MotionBone kBones2f8720[18] = {  // 0x2f8720
    { "bn_pelvis", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_spine0", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_spine1", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_clavicle", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_upperarm", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_foretwist0", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_hand", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_thigh", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_calf", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_foot", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_clavicle", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_upperarm", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_foretwist0", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_hand", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_thigh", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_calf", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_foot", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f8960[18] = {  // 0x2f8960
    { "bn_pelvis", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_spine0", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_spine1", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_clavicle", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_upperarm", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_foretwist0", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_hand", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_thigh", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_calf", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_foot", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_clavicle", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_upperarm", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_foretwist0", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_hand", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_thigh", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_calf", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_foot", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f8ba0[18] = {  // 0x2f8ba0
    { "bn_pelvis", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_spine0", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_spine1", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_clavicle", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_upperarm", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_foretwist0", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_hand", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_thigh", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_calf", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_foot", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_clavicle", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_upperarm", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_foretwist0", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_hand", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_thigh", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_calf", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_foot", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f8de0[18] = {  // 0x2f8de0
    { "bn_pelvis", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_spine0", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_spine1", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_clavicle", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_upperarm", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_foretwist0", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_hand", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_thigh", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_calf", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_foot", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_clavicle", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_upperarm", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_foretwist0", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_hand", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_thigh", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_calf", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_foot", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f9020[18] = {  // 0x2f9020
    { "bn_pelvis", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_spine0", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_spine1", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_clavicle", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_upperarm", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_foretwist0", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_hand", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_thigh", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_calf", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_foot", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_clavicle", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_upperarm", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_foretwist0", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_hand", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_thigh", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_calf", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_foot", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f9260[18] = {  // 0x2f9260
    { "bn_pelvis", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_spine0", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_spine1", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_clavicle", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_upperarm", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_foretwist0", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_hand", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_thigh", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_calf", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_foot", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_clavicle", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_upperarm", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_foretwist0", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_hand", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_thigh", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_calf", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_foot", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f94a0[4] = {  // 0x2f94a0
    { "bn_pelvis", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_thigh", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_calf", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f9520[8] = {  // 0x2f9520
    { "bn_pelvis", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_spine0", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_spine1", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_spine2", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_thigh", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_calf", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_foot", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f9620[19] = {  // 0x2f9620
    { "bn_pelvis", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_spine0", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_spine1", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_spine2", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_clavicle", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_upperarm", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_foretwist0", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_hand", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_clavicle", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_upperarm", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_foretwist0", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_hand", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_thigh", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_calf", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_foot", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_toe0", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_toe1", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_toe2", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f9880[1] = {  // 0x2f9880
    { "bn_pelvis", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f9900[4] = {  // 0x2f9900
    { "bn_r_hand", "bn_r_upperarm", 50.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_hand", "bn_l_upperarm", 50.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_foot", "bn_r_thigh", 50.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_foot", "bn_l_thigh", 50.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f9980[4] = {  // 0x2f9980
    { "bn_r_hand", "bn_r_upperarm", 50.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_hand", "bn_l_upperarm", 50.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_foot", "bn_r_thigh", 50.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_foot", "bn_l_thigh", 50.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f9a00[4] = {  // 0x2f9a00
    { "bn_r_hand", "bn_r_upperarm", 50.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_hand", "bn_l_upperarm", 50.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_foot", "bn_r_thigh", 50.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_foot", "bn_l_thigh", 50.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f9a80[2] = {  // 0x2f9a80
    { "bn_r_hand", "bn_r_upperarm", 50.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_hand", "bn_l_upperarm", 50.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f9ac0[2] = {  // 0x2f9ac0
    { "bn_r_hand", "bn_r_upperarm", 50.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 50.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f9b00[1] = {  // 0x2f9b00
    { "bn_head", nullptr, 50.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f9b20[1] = {  // 0x2f9b20
    { "bn_head", nullptr, 50.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f9b40[1] = {  // 0x2f9b40
    { "bn_head", nullptr, 50.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f9b60[2] = {  // 0x2f9b60
    { "bn_r_hand", nullptr, 50.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 50.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f9ba0[3] = {  // 0x2f9ba0
    { "bn_r_hand", nullptr, 50.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 50.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_pelvis", nullptr, 50.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f9f20[3] = {  // 0x2f9f20
    { "bn_base", nullptr, 35.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_pelvis", nullptr, 35.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f9f80[3] = {  // 0x2f9f80
    { "bn_base", nullptr, 35.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_pelvis", nullptr, 35.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f9fe0[3] = {  // 0x2f9fe0
    { "bn_base", nullptr, 35.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_pelvis", nullptr, 35.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2fa040[3] = {  // 0x2fa040
    { "bn_base", nullptr, 35.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_pelvis", nullptr, 35.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2fa0a0[3] = {  // 0x2fa0a0
    { "bn_base", nullptr, 35.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_pelvis", nullptr, 35.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2fa100[3] = {  // 0x2fa100
    { "bn_base", nullptr, 35.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_pelvis", nullptr, 35.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2fa160[3] = {  // 0x2fa160
    { "bn_base", nullptr, 35.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_pelvis", nullptr, 35.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2fa1c0[3] = {  // 0x2fa1c0
    { "bn_base", nullptr, 35.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_pelvis", nullptr, 35.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2fa220[11] = {  // 0x2fa220
    { "bn_pelvis", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_spine0", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_upperarm", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_r_upperarm", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_thigh", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_calf", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_foot", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_toe0", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_toe1", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_l_toe2", nullptr, 15.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2fa380[3] = {  // 0x2fa380
    { "bn_base", nullptr, 35.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_pelvis", nullptr, 35.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f9c60[2] = {  // 0x2f9c60
    { "bn_pelvis", nullptr, 35.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f9ca0[2] = {  // 0x2f9ca0
    { "bn_pelvis", nullptr, 35.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f9ce0[2] = {  // 0x2f9ce0
    { "bn_pelvis", nullptr, 35.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f9d20[2] = {  // 0x2f9d20
    { "bn_pelvis", nullptr, 35.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_d_head", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f9d60[2] = {  // 0x2f9d60
    { "bn_pelvis", nullptr, 35.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f9da0[2] = {  // 0x2f9da0
    { "bn_pelvis", nullptr, 35.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f9de0[2] = {  // 0x2f9de0
    { "bn_pelvis", nullptr, 35.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f9e20[2] = {  // 0x2f9e20
    { "bn_pelvis", nullptr, 35.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f9e60[2] = {  // 0x2f9e60
    { "bn_pelvis", nullptr, 35.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
    { "bn_head", nullptr, 20.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};
static MotionBone kBones2f9ea0[1] = {  // 0x2f9ea0
    { "bn_pelvis", nullptr, 35.0f, 0, 0.0f, 0.0f, 0.0f, 0 },
};

// By group (the ROM's order of tables: 0x2f98a0, 0x2f9c00, 0x2fa3e0, 0x2f9ec0)
// and list.
MotionBoneList gMotionBoneSets[kMotionBoneGroups][kMotionBoneLists] = {
    { { kBones2f8720, 18 }, { kBones2f8960, 18 }, { kBones2f8ba0, 18 }, { kBones2f8de0, 18 }, { kBones2f9020, 18 }, { kBones2f9260, 18 }, { kBones2f94a0, 4 }, { kBones2f9520, 8 }, { kBones2f9620, 19 }, { kBones2f9880, 1 } },
    { { kBones2f9900, 4 }, { kBones2f9980, 4 }, { kBones2f9a00, 4 }, { kBones2f9a80, 2 }, { kBones2f9ac0, 2 }, { kBones2f9b00, 1 }, { kBones2f9b20, 1 }, { kBones2f9b40, 1 }, { kBones2f9b60, 2 }, { kBones2f9ba0, 3 } },
    { { kBones2f9f20, 3 }, { kBones2f9f80, 3 }, { kBones2f9fe0, 3 }, { kBones2fa040, 3 }, { kBones2fa0a0, 3 }, { kBones2fa100, 3 }, { kBones2fa160, 3 }, { kBones2fa1c0, 3 }, { kBones2fa220, 11 }, { kBones2fa380, 3 } },
    { { kBones2f9c60, 2 }, { kBones2f9ca0, 2 }, { kBones2f9ce0, 2 }, { kBones2f9d20, 2 }, { kBones2f9d60, 2 }, { kBones2f9da0, 2 }, { kBones2f9de0, 2 }, { kBones2f9e20, 2 }, { kBones2f9e60, 2 }, { kBones2f9ea0, 1 } },
};
