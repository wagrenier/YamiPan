/* ==========================================================================
 *  game/scenepairs.h
 *
 *  The scene's action lists (0x171300..0x171a30), StageScene +0x14. The name
 *  ScenePairTable came from the five {0, 0} pairs its ctor makes: they are
 *  five doubly linked lists. Each frame's four action steps walk all five
 *  and call one of the four side slots of every entry that is on.
 *
 *  The list is a template instance in the ROM, the only one seen; written
 *  for its one type.
 * ======================================================================== */
#ifndef YAMIPAN_GAME_SCENEPAIRS_H
#define YAMIPAN_GAME_SCENEPAIRS_H

class StageSideBase;

// Which list an entry joins: the order the frame steps them in.
enum ScenePairListId
{
    ScenePairList_Items      = 0,   // items, bullets
    ScenePairList_Objects    = 1,   // map objects, an event's objects
    ScenePairList_Lights     = 2,
    ScenePairList_Characters = 3,   // StageCharacter::setup
    ScenePairList_Count      = 5,   // nothing joins the fifth
};

// layout: host -- EE 0xc.
struct ScenePairNode
{
    explicit ScenePairNode(StageSideBase **value);                       // FS2 0x00171a10

    StageSideBase **value;      // FS2 +0x00: a box of its own, on the heap
    ScenePairNode  *next;       // FS2 +0x04
    ScenePairNode  *prev;       // FS2 +0x08
};

// layout: host -- EE 8.
class ScenePairList
{
public:
    ScenePairList();                                                     // FS2 0x001716c0
    ~ScenePairList();                                                    // FS2 0x001716d0: deleting

    ScenePairNode *find(StageSideBase *entry) const;                     // FS2 0x00171730
    void pushBack(StageSideBase *entry);                                 // FS2 0x00171770
    void pushFront(StageSideBase *entry);                                // FS2 0x001718e0
    void erase(ScenePairNode *node);                                     // FS2 0x00171820
    void clear();                                                        // FS2 0x00171870
    ScenePairNode *tail() const;                                         // FS2 0x00171970

    ScenePairNode *head;        // FS2 +0x00
    int            count;       // FS2 +0x04

private:
    static void unlink(ScenePairNode *node);                             // FS2 0x001719a0
};

// layout: host -- EE 0x2c (StageScene's ctor asks for that; nothing touches
// the last word).
struct ScenePairTable
{
    ScenePairTable();                                                    // FS2 0x00171680
    ~ScenePairTable();                                                   // FS2 0x00171620: deleting

    // entry joins list; every action step reaches it.
    void add(StageSideBase *entry, int list);                            // FS2 0x001715f0
    // entry leaves whichever of the five lists holds it.
    void remove(StageSideBase *entry);                                   // FS2 0x00171580

    // The frame's action steps, named by the perf bars: one side slot of
    // every entry with m_viewActive set.
    void actionStart();                                                  // FS2 0x001714e0: sideHook0
    void actionControl();                                                // FS2 0x00171440: sideHook1
    void actionMovement();                                               // FS2 0x001713a0: sideHook2
    void actionTerminate();                                              // FS2 0x00171300: sideHook3

    ScenePairList lists[ScenePairList_Count]; // FS2 +0x00
};

#endif
