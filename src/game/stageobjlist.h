/* ==========================================================================
 *  game/stageobjlist.h
 *
 *  The stage's object registry (game/stageobjlist.cpp, 0x193900..0x1953f0):
 *  gStageScene->objects, made by the stage scene (0x19c180) and deleted by
 *  its teardown (0x19bbe0). Every stage object is filed in one of nine
 *  groups, under its name (StageObject::m_name) in the group's name map and
 *  in the group's list in creation order. A name may be shared: the map
 *  holds an array per name, and the lookups take an index into it.
 *
 *  The two containers are the game's own (no MSL): StageObjArray, a
 *  zero-terminated pointer array on the global heap, and StageNameMap, a
 *  binary tree by strcmp whose nodes also chain in insertion order;
 *  rebalance makes it an AVL-like tree once the stage is loaded (0x17fb90).
 *  The key a node holds is the object's own name buffer.
 * ======================================================================== */
#ifndef _GAME_STAGEOBJLIST_H
#define _GAME_STAGEOBJLIST_H

#include "sdk/scetypes.h"

class StageObject;              // game/gamehooks.h

// The nine groups. Who fills each one is known (the add* callers); what
// the groups mean is read only where those callers say it.
enum StageObjGroup
{
    StageObjGroup_0          = 0,   // add0 from 0x177d20
    StageObjGroup_1          = 1,   // add1 from 0x1760c0 ("Root"); doors are here (getDoorState)
    StageObjGroup_Characters = 2,   // addCharacter from 0x17c700 (bones head / bn_l_foot, BLOOD); numbered
    StageObjGroup_3          = 3,   // add3 from 0x183bf0 ("bn_pic")
    StageObjGroup_Bullets    = 4,   // addBullet from the bullet class 0x1803e0 ("bullet"); removable
    StageObjGroup_5          = 5,   // add5 from 0x1861b0; removable
    StageObjGroup_6          = 6,   // add6 from 0x17ef10
    StageObjGroup_7          = 7,   // add7 from 0x17ecb0
    StageObjGroup_8          = 8,   // add8 from 0x269bd0
    StageObjGroup_Count      = 9,
};

// A growable array of object pointers on gGlobalHeap: a block of capacity
// + 2 words, data[0] unused (0), the items at data[1..count] and a 0 after
// the last. The walks stop at that 0, not at count.
// layout: host -- EE 8 bytes: +0 data, +4 count (u_short), +6 capacity.
class StageObjArray
{
public:
    StageObjArray();                                                     // FS2 0x001950a0
    ~StageObjArray();                                                    // FS2 0x001948a0

    void clear();                                                        // FS2 0x00195080
    void reserve(u_short capacity);                                      // FS2 0x00195120
    int  push(StageObject *const &obj);                                  // FS2 0x00194fd0
    // The slot holding obj; null none.
    StageObject **find(StageObject *const &obj);                         // FS2 0x00194ee0
    // Removes the item in slot; answers slot (now the next item), null
    // when it was the last.
    StageObject **erase(StageObject **slot);                             // FS2 0x00194f40

    // The first item's slot, null when empty; next answers null past the
    // last (inlined everywhere in the ROM).
    StageObject **begin() { return m_count == 0 ? nullptr : &m_data[1]; }
    static StageObject **next(StageObject **slot) { return slot[1] == nullptr ? nullptr : slot + 1; }
    // The index-th item; a negative index or one past the end answers the
    // last; null when empty (inlined in every lookup).
    StageObject *at(int index) const
    {
        if (index < 0 || index >= m_count)
            return m_data[m_count];
        return m_data[index + 1];
    }

    StageObject **m_data;       // FS2 +0x0
    u_short       m_count;      // FS2 +0x4
    u_short       m_capacity;   // FS2 +0x6

    static const u_short kInitialCapacity = 14;
};

// A node of a name map: a binary tree by strcmp (lower keys left), and a
// chain in insertion order for the walks.
// layout: host -- EE 0x14 bytes.
struct StageNameNode
{
    StageNameNode(const char *key, StageObjArray *const &value);         // FS2 0x00194a70

    // The node for key under node; a node without an array counts as
    // missing. Called with a map's root (null: empty).
    static StageNameNode *find(StageNameNode *node, const char *key);    // FS2 0x00194ae0
    // A new leaf for key under this node; null when key is already there.
    StageNameNode *insert(const char *key, StageObjArray *const &value); // FS2 0x00194b60
    // The subtree rebalanced; answers its new root.
    static StageNameNode *balance(StageNameNode *node);                  // FS2 0x00194da0
    static u_int height(StageNameNode *node);                            // FS2 0x001951e0

    StageObjArray *value;       // FS2 +0x00: the objects called key
    const char    *key;         // FS2 +0x04: the first object's m_name
    StageNameNode *left;        // FS2 +0x08
    StageNameNode *right;       // FS2 +0x0c
    StageNameNode *next;        // FS2 +0x10: insertion order
};

// layout: host -- EE 0x10 bytes.
class StageNameMap
{
public:
    StageNameMap();                                                      // FS2 0x00194910
    ~StageNameMap();                                                     // FS2 0x00194930

    // A node for key holding value; null when key is already there.
    StageNameNode *insert(const char *key, StageObjArray *const &value); // FS2 0x001949c0

    StageNameNode *m_root;      // FS2 +0x0
    StageNameNode *m_first;     // FS2 +0x4
    StageNameNode *m_last;      // FS2 +0x8
    int            m_count;     // FS2 +0xc
};

// The registry (gStageScene +0x10).
// layout: host -- EE 0xdc bytes: +0x00 the maps (0x10 each), +0x90 the
// lists (8 each), +0xd8 the last character id.
class StageObjectList
{
public:
    StageObjectList();                                                   // FS2 0x00194800
    // Deletes every object in the lists (the ROM's deleting dtor through
    // vtable +0x10) and every name array.
    ~StageObjectList();                                                  // FS2 0x00194630

    // File obj under its name and at the end of its group's list.
    void add0(StageObject *obj);                                         // FS2 0x00194590
    void add1(StageObject *obj);                                         // FS2 0x00194430
    // Also numbers obj (StageObject::m_id) from 1.
    void addCharacter(StageObject *obj);                                 // FS2 0x001944d0
    void add3(StageObject *obj);                                         // FS2 0x00194390
    void addBullet(StageObject *obj);                                    // FS2 0x001942f0
    void add5(StageObject *obj);                                         // FS2 0x00194250
    void add6(StageObject *obj);                                         // FS2 0x001941b0
    void add7(StageObject *obj);                                         // FS2 0x00194110
    void add8(StageObject *obj);                                         // FS2 0x00194070

    // Take obj out of its name array and its group's list (the name node
    // stays, maybe empty).
    void removeBullet(StageObject *obj);                                 // FS2 0x00193ef0
    void remove5(StageObject *obj);                                      // FS2 0x00193f80

    // The index-th object called name in the group (negative or past the
    // end: the last of them); null none.
    StageObject *find0(const char *name, int index);                     // FS2 0x00193e60
    StageObject *find1(const char *name, int index);                     // FS2 0x00193ce0
    StageObject *findCharacter(const char *name, int index);             // FS2 0x00193dd0
    StageObject *find5(const char *name, int index);                     // FS2 0x00193c50
    StageObject *find6(const char *name, int index);                     // FS2 0x00193bc0
    StageObject *find7(const char *name, int index);                     // FS2 0x00193b30
    StageObject *find8(const char *name, int index);                     // FS2 0x00193aa0
    // The same over every group in order; the first found.
    StageObject *findAny(const char *name, int index);                   // FS2 0x001939c0
    // Every object called name in the group; null none.
    StageObjArray *findAll3(const char *name);                           // FS2 0x00193990
    StageObjArray *findAll5(const char *name);                           // FS2 0x00193960
    StageObjArray *findAll6(const char *name);                           // FS2 0x00193930
    StageObjArray *findAll7(const char *name);                           // FS2 0x00193900
    // The character addCharacter numbered id; null none.
    StageObject *findCharacterById(int id);                              // FS2 0x00193d70

    // Rebalances every name map (once, after the stage is loaded).
    void rebalance();                                                    // FS2 0x00194010

    StageNameMap  m_names[StageObjGroup_Count];   // FS2 +0x00
    StageObjArray m_lists[StageObjGroup_Count];   // FS2 +0x90
    int           m_lastId;                       // FS2 +0xd8

private:
    // The bodies every add* / remove* / find* inlines.
    void add(StageObjGroup group, StageObject *obj);
    void remove(StageObjGroup group, StageObject *obj);
    StageObject *lookup(StageObjGroup group, const char *name, int index);
    StageObjArray *lookupAll(StageObjGroup group, const char *name);
};

#endif /* _GAME_STAGEOBJLIST_H */
