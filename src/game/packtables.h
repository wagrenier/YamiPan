/* ==========================================================================
 *  game/packtables.h
 *
 *  The game's parameter tables (game/packtables.cpp, 0x2bcd20..0x2c3d30):
 *  dataEU/packgameEU.pak is a pack of CSV sheets and small lists; once at
 *  boot PackTables::parse reads them into the tables other files use:
 *  gStageObjRecords, gItemDefs, gWeaponDefs, gDoorRecords (their records
 *  are in game/gamehooks.h), gMotionDefs, gMotionModels and gMotionBoneSets.
 *  The sheets are freed again: only those tables stay.
 * ======================================================================== */
#ifndef _GAME_PACKTABLES_H
#define _GAME_PACKTABLES_H

#include <map>
#include <string>
#include <vector>

#include "sdk/libvu0.h"
#include "sdk/scetypes.h"

class PackFile;

// A sheet's cell. layout: host -- std::string; EE 0x10.
struct PackEntry
{
    int         number;         // FS2 +0x0: atoi of the text
    std::string text;           // FS2 +0x4: MSL string, EE 0xc
};
typedef std::vector<PackEntry> EntryList;
// The rows by the number in their first column.
typedef std::map<int, EntryList> ListMap;
// The columns by the name in the header row.
typedef std::map<std::string, int> NameMap;

// layout: host -- EE 0x80: eight MSL maps of 0x10, ListMap at +0x00 / +0x20 /
// +0x40 / +0x60, NameMap at +0x10 / +0x30 / +0x50 / +0x70 (0x2c16b0 /
// 0x2c1710); the destructor is implicit (inline in loadGamePackTables).
class PackTables
{
public:
    PackTables();                                                        // FS2 0x002c0dc0
    // Reads the pack's items into the tables; the maps end empty.
    void parse(PackFile *pack);                                          // FS2 0x002bcf10

    ListMap characterRows;      // FS2 +0x00
    NameMap characterColumns;   // FS2 +0x10
    ListMap itemRows;           // FS2 +0x20
    NameMap itemColumns;        // FS2 +0x30
    ListMap doorRows;           // FS2 +0x40
    NameMap doorColumns;        // FS2 +0x50
    ListMap motionRows;         // FS2 +0x60
    NameMap motionColumns;      // FS2 +0x70

private:
    void parseCharacters(PackFile *pack);
    void parseItems(PackFile *pack);
    void parseDoors(PackFile *pack);
    void parseMotions(PackFile *pack);
    void parseMotionOffsets(PackFile *pack);
    void parseMotionModels(PackFile *pack);
    void parseMotionBones(PackFile *pack);
    void clear();                                                        // FS2 0x002bcd20
};

// A motion (bss 0x577b20, 0x100 x 0x210; item 5 and, for the two offset
// sets, items 6 and 7). The field names say what the sheet's column says;
// the readers (0x1de000..0x1e4000) are not written yet.
// layout: host -- +0xa0..+0xc4 are pointers into the record itself.
constexpr int kMotionDefCount    = 0x100;
constexpr int kMotionRifleLabels = 3;
constexpr int kMotionShots       = 4;
constexpr int kMotionOffsetSets  = 2;
struct MotionDef
{
    char          label[0x10];       // FS2 +0x000: ラベル
    char          upperLabel[0x10];  // FS2 +0x010: ラベル（上半身）
    char          rifleLabel[kMotionRifleLabels][0x10]; // FS2 +0x020: ラベル（ライフルタイプ%d）
    char          linkFunction[0x10]; // FS2 +0x050: リンクファンクション
    char          shot[kMotionShots][0x10]; // FS2 +0x060: S%d
    // Each of these is the text above, null when it is empty.
    const char   *labelText;         // FS2 +0x0a0
    const char   *upperLabelText;    // FS2 +0x0a4
    const char   *rifleLabelText[kMotionRifleLabels]; // FS2 +0x0a8
    const char   *linkFunctionText;  // FS2 +0x0b4
    const char   *shotText[kMotionShots]; // FS2 +0x0b8
    int           shotKind[kMotionShots]; // FS2 +0x0c8: kMotionShotKinds, -1 without a text
    u_char        waitForEnd;        // FS2 +0x0d8: 再生終了待ち
    int           groundCheck;       // FS2 +0x0dc: 接地判定
    u_char        wallCheck;         // FS2 +0x0e0: 壁判定
    int           repelLevel;        // FS2 +0x0e4: キャラ反発LV
    u_char        ik;                // FS2 +0x0e8: IK
    int           nextMotion;        // FS2 +0x0ec: 遷移モーション as an index, -1 none
    int           damageKind;        // FS2 +0x0f0: ダメージ種別
    int           damageStep;        // FS2 +0x0f4: ダメージ推移
    u_char        sameRequest;       // FS2 +0x0f8: 同一リクエスト
    int           landSeKind;        // FS2 +0x0fc: 接地SE種別
    u_char        headSway;          // FS2 +0x100: 頭振り == 1
    u_char        upperSway;         // FS2 +0x101: 上半身振り
    u_char        cameraModeSwitch;  // FS2 +0x102: カメラモード切替
    u_char        firstPersonZoom;   // FS2 +0x103: 主観ズーム
    u_char        flashlightToggle;  // FS2 +0x104: 懐中電灯ON/OFF
    u_char        flashlightControl; // FS2 +0x105: 懐中電灯制御
    u_char        headSway2;         // FS2 +0x106: 頭振り == 2
    u_char        headSwayBoneLink;  // FS2 +0x107: 頭振りボーン連動
    u_char        cameraShift[kMotionOffsetSets]; // FS2 +0x108: the last group's flag in items 6, 7
    // Items 6 and 7: four groups of two vectors each (w 0).
    sceVu0FVECTOR cameraFocus[kMotionOffsetSets][4]; // FS2 +0x110
    sceVu0FVECTOR cameraEye[kMotionOffsetSets][4]; // FS2 +0x190
};
extern MotionDef gMotionDefs[kMotionDefCount];   /* bss 0x577b20 */

// A model family's motion limits (data 0x2f8000, 0x14 x 0x50), found by a
// name tag; item 8 overwrites them. What the numbers mean is unread (the
// readers are 0x17d970 and 0x1ecda0). layout: host -- EE 0x50.
constexpr int kMotionModelCount = 0x14;
struct MotionModel
{
    const char   *tag;          // FS2 +0x00: _li_, _lh_, _th_, ...
    float         limits[3];    // FS2 +0x04
    sceVu0FVECTOR a;            // FS2 +0x10: w 1
    sceVu0FVECTOR b;            // FS2 +0x20: w 1
    sceVu0FVECTOR c;            // FS2 +0x30: w 0
    sceVu0FVECTOR d;            // FS2 +0x40: w 0
};
extern MotionModel gMotionModels[kMotionModelCount];   /* data 0x2f8000 */

// A bone's motion entry (EE 0x20, 0x2f8720..0x2fa3e0), by the bone's name;
// items 9.. overwrite the four numbers. The readers are 0x1f0170..0x1f3d20.
// layout: host -- EE 0x20.
struct MotionBone
{
    const char *name;           // FS2 +0x00: bn_pelvis, ...
    const char *name2;          // FS2 +0x04: a second bone in some lists; null none
    float       a;              // FS2 +0x08
    int         _0c;            // FS2 +0x0c: 0 in every entry
    float       b;              // FS2 +0x10
    float       c;              // FS2 +0x14
    float       d;              // FS2 +0x18
    int         _1c;            // FS2 +0x1c: 0 in every entry
};
// Four groups (0x2f98a0, 0x2f9c00, 0x2fa3e0, 0x2f9ec0) of ten lists.
constexpr int kMotionBoneGroups = 4;
constexpr int kMotionBoneLists  = 10;
// layout: host -- the ROM keeps two parallel tables.
struct MotionBoneList
{
    MotionBone *bones;          // the group's pointer table
    int         count;          // the group's count table (+0x30)
};
extern MotionBoneList gMotionBoneSets[kMotionBoneGroups][kMotionBoneLists];

#endif /* _GAME_PACKTABLES_H */
