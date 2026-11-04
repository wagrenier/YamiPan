/* ==========================================================================
 *  game/menupack.h
 *
 *  The resident menu pack (game/menupack.cpp, 0x2c3d30..0x2c42a0):
 *  dataEU/packmenuEU.pak with the SePlayer the menus play their sounds
 *  on, built from its seMenu_SE.seh. A stage unloads both for the memory.
 *  The file also holds MessageTable, the stage's script text lookup
 *  (StageMission owns one): by position only, no caller in common.
 *
 *  The file name is the port's (no source string).
 * ======================================================================== */
#ifndef _GAME_MENUPACK_H
#define _GAME_MENUPACK_H

#include <stddef.h>

#include "game/s2ddata.h"   /* S2dRef */
#include "sdk/scetypes.h"

class SePlayer;             // game/seplayer.h

// The items the game asks the menu pack for by id.
enum MenuPackItem
{
    MenuPackItem_SaveIcon = 0,  // icon1.ico (Memcard::startSave)
    MenuPackItem_IconSys  = 1,  // an icon.sys template (Memcard::buildIconSys)
};

// Loads the pack and builds the SePlayer, unless the pack is loaded.
void loadMenuPack();                                                     // FS2 0x002c3e10
// Deletes what loadMenuPack made (the pack, its SePlayer); a stage needs
// the memory.
void unloadMenuPack();                                                   // FS2 0x002c3db0
// The pack's item / its size; null / 0 while it is not loaded.
void *getMenuPackItem(int id);                                           // FS2 0x002c3d70
int   getMenuPackItemSize(int id);                                       // FS2 0x002c3d30
// The sound player outside a stage: null while the pack is unloaded.
SePlayer *getSePlayer();                                                 // FS2 0x002c4140: sbss 0x31aea0

// ──────────────────────────────────────────────────────────────────────
// MessageTable

constexpr u_int kMessageFileMagic = 0x6d621a00;

// A pair of a message file: the key a script names and its text.
struct MessageRecord
{
    S2dRef<const char> key;     // FS2 +0x0
    S2dRef<const char> text;    // FS2 +0x4
};
static_assert(sizeof(MessageRecord) == 8, "MessageRecord: EE 8");
static_assert(offsetof(MessageRecord, text) == 4, "MessageRecord::text");

// The stage's message file, relocated in place once.
struct MessageFile
{
    u_int  magic;               // FS2 +0x00: kMessageFileMagic
    u_char _04[4];              // FS2 +0x04
    u_int  count;               // FS2 +0x08
    u_int  relocated;           // FS2 +0x0c
    MessageRecord records[1];   // FS2 +0x10: count
};
static_assert(offsetof(MessageFile, count) == 0x08, "MessageFile::count");
static_assert(offsetof(MessageFile, relocated) == 0x0c, "MessageFile::relocated");
static_assert(offsetof(MessageFile, records) == 0x10, "MessageFile::records");

// layout: host -- EE 4 (the file).
class MessageTable
{
public:
    MessageTable();                                                      // FS2 0x002c4290
    // Takes a message file, relocating it once; false null or not one
    // (the table is then empty).
    bool setFile(void *file);                                            // FS2 0x002c41f0
    // The text of key; null none.
    const char *find(const char *key) const;                             // FS2 0x002c4150

private:
    MessageFile *m_file;        // FS2 +0x0
};

#endif /* _GAME_MENUPACK_H */
