/* ==========================================================================
 *  game/uilayout.h
 *
 *  The 2D layout player (game/uilayout.cpp, 0x243390..0x246230): a UiLayout
 *  plays one S2D file (the .d2b files under 2dEU/ and the pack items, magic
 *  "S2D\0"): its sequences run small scripts that start and stop other
 *  sequences, animate actors through anim sets, create widgets, play
 *  sounds, read the pad, set variables, open a nested layout and end the
 *  layout with a result (m_result / m_done, which the modal runner in
 *  game/screen.cpp waits for).
 *
 *  A frame is advance() (sequences step their animations), update() (each
 *  sequence's frame commands until nothing starts or stops any more) and
 *  draw() (sequences in priority order through the UiDrawer). A layout can
 *  hold one nested layout (UiSubLayout, its base) loaded by a script command
 *  from a file or the stage pack; it runs and draws on top of its parent.
 *
 *  The S2D file is game/s2ddata.h, the sequences and their actors / anims /
 *  widgets game/uisequence.h, the drawer game/uidrawer.h; the layout's game
 *  hooks (members declared here) are written in game/uigame.cpp. The file
 *  and class names are the port's (no source string in the range).
 * ======================================================================== */
#ifndef _GAME_UILAYOUT_H
#define _GAME_UILAYOUT_H

#include <stddef.h>

#include "game/s2ddata.h"
#include "game/uidrawer.h"
#include "game/uigame.h"
#include "game/uisequence.h"
#include "sdk/libvifpk.h"
#include "sdk/scetypes.h"

class FileJob;                  // game/gamehooks.h

// ──────────────────────────────────────────────────────────────────────
// The layout.

// UiSubLayout::m_state.
enum SubLayoutState
{
    SubLayout_None = 0,
    SubLayout_Loading = 1,      // the FileJob runs; advance opens its data
    SubLayout_Open = 2,         // the child runs and draws
    SubLayout_Closing = 3,      // closed on the next advance (nothing here sets it)
};

// The nested layout a layout can hold: UiLayout's base (+0x00..+0x1c).
// layout: host -- EE 0x1c.
class UiSubLayout
{
public:
    // Inline in the ROM (UiLayout::UiLayout).
    UiSubLayout()
        : m_state(SubLayout_None), m_lastResult(-1), m_child(nullptr), m_job(nullptr),
          m_arg(0), m_childDir(nullptr), m_18(0)
    {
    }
    ~UiSubLayout();                                                      // FS2 0x002438b0: (obj, flag)

    void load(const char *dir, const char *path, int arg);               // FS2 0x00243390
    bool open(void *data);                                               // FS2 0x00243630
    void close();                                                        // FS2 0x00243770
    void advance();                                                      // FS2 0x00243520
    void update();                                                       // FS2 0x002434c0

    int         m_state;        // FS2 +0x00: SubLayoutState
    int         m_lastResult;   // FS2 +0x04: the last child's result
    UiLayout   *m_child;        // FS2 +0x08
    FileJob    *m_job;          // FS2 +0x0c: while loading
    int         m_arg;          // FS2 +0x10: the child's setData arg
    const char *m_childDir;     // FS2 +0x14: the child's base dir (nothing here sets it)
    int         m_18;           // FS2 +0x18: only cleared
};

// The sound slots a layout's scripts play into.
constexpr int kUiLayoutSoundSlots = 4;

// A 2D layout playing an S2D file. The overlay layouts (game/screen.cpp)
// and the modal runner build them on pack items.
// layout: host -- EE 0x98.
class UiLayout : public UiSubLayout
{
public:
    UiLayout();                                                          // FS2 0x00246160
    ~UiLayout();                                                         // FS2 0x00246010: deleting (obj, 1)

    bool setData(void *data, int arg);                                   // FS2 0x00245e80
    void advance();                                                      // FS2 0x00243b90
    void update();                                                       // FS2 0x00243c10
    int  draw(sceVif1Packet *packet, void *texList);                     // FS2 0x002439f0: answers the sequence count
    // Replaces the new[] copy at +0x60 (null clears it); runArchiveScreen
    // passes "2dEU/".
    void setBaseDir(const char *dir);                                    // FS2 0x00245f10
    void clear();                                                        // FS2 0x00245f80

    int         startSequence(int id, int value);                        // FS2 0x00244d90
    int         stopSequence(int id);                                    // FS2 0x00243dc0
    int         removeSequence(int id);                                  // FS2 0x00245c80: -1 all
    UiSequence *findSequence(int id);                                    // FS2 0x00245e00
    int         runCommands(UiSequence *seq, int trigger);               // FS2 0x00243f40
    bool        testCondition(UiSequence *seq, const S2dCond *cond);     // FS2 0x00245060
    bool        compare(int a, int b, const S2dCond *cond);              // FS2 0x00245c00

    // The game hooks (game/uigame.cpp).
    void playSound(int slot, const char *name);                          // FS2 0x0025ff40
    void stopSound(int slot);                                            // FS2 0x00260100: -1 does nothing
    void readGameValue(int kind, u_int packed);                          // FS2 0x002601f0
    void writeGameValue(int kind, u_int packed);                         // FS2 0x002602a0
    bool testGameValue(int kind, u_int packed);                          // FS2 0x002603d0
    bool testGameState(const char *name, const char *arg, UiSequence *seq); // FS2 0x002604a0

    S2dData     *m_data;        // FS2 +0x1c
    UiDrawer    *m_drawer;      // FS2 +0x20
    int          m_seqCapacity; // FS2 +0x24
    int          m_seqCount;    // FS2 +0x28
    UiSequence **m_seqs;        // FS2 +0x2c: gGlobalHeap, grown by 16
    u_char       m_drawMode;    // FS2 +0x30: UiDrawer::setMode's mode (0 / 1)
    int          m_arg;         // FS2 +0x34: setData's arg
    int          m_result;      // FS2 +0x38: the answer, once m_done
    u_char       m_done;        // FS2 +0x3c: the layout has ended
    UiVec4       m_color;       // FS2 +0x40
    UiVec4       m_offset;      // FS2 +0x50
    char        *m_baseDir;     // FS2 +0x60: new[]
    const char  *m_soundName[kUiLayoutSoundSlots];   // FS2 +0x64
    long long    m_soundHandle[kUiLayoutSoundSlots]; // FS2 +0x78: -1 none

private:
    // Priority order, lowest first (an exchange sort). Inline in the ROM
    // everywhere the array changes.
    void sortSequences()
    {
        if (m_seqs == nullptr)
            return;
        for (int i = 0; i < m_seqCount - 1; i++) {
            for (int j = i + 1; j < m_seqCount; j++) {
                UiSequence *a = m_seqs[i];
                UiSequence *b = m_seqs[j];
                if (b->m_priority < a->m_priority) {
                    m_seqs[i] = b;
                    m_seqs[j] = a;
                }
            }
        }
    }
};

#endif /* _GAME_UILAYOUT_H */
