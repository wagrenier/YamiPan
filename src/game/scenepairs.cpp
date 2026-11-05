/* ==========================================================================
 *  game/scenepairs.cpp
 *
 *  The scene's action lists (game/scenepairs.h), 0x171300..0x171a30. The file
 *  name is the port's.
 *
 *  An action step reads an entry's next link after its hook returns, as the
 *  ROM does: a hook may add to the list it is walked by. Only StageBullet and
 *  StageLight ever remove themselves, from their dtors; every other entry
 *  stays until the scene's table goes, after the scene's objects, and the
 *  table's dtor does not look into its entries.
 * ======================================================================== */
#include "game/gamehooks.h"
#include "game/scenepairs.h"

// FS2 0x00171a10

ScenePairNode::ScenePairNode(StageSideBase **value)
    : value(value), next(nullptr), prev(nullptr)
{
}

// FS2 0x001716c0

ScenePairList::ScenePairList() : head(nullptr), count(0)
{
}

// FS2 0x001716d0
// callcheck: clear, operator_delete

ScenePairList::~ScenePairList()
{
    clear();
}

// FS2 0x00171730

ScenePairNode *ScenePairList::find(StageSideBase *entry) const
{
    for (ScenePairNode *node = head; node != nullptr; node = node->next) {
        if (*node->value == entry)
            return node;
    }
    return nullptr;
}

// FS2 0x00171770
// callcheck: pushFront, tail, operator_new, operator_new, ScenePairNode

void ScenePairList::pushBack(StageSideBase *entry)
{
    if (head == nullptr) {
        pushFront(entry);
        return;
    }
    ScenePairNode *last = tail();
    ScenePairNode *node = new ScenePairNode(new StageSideBase *(entry));
    node->prev = last;
    last->next = node;
    count++;
}

// FS2 0x001718e0
// callcheck: operator_new, operator_new, ScenePairNode

void ScenePairList::pushFront(StageSideBase *entry)
{
    ScenePairNode *old = head;
    head = new ScenePairNode(new StageSideBase *(entry));
    if (old != nullptr) {
        head->next = old;
        old->prev = head;
    }
    count++;
}

// FS2 0x00171820
// callcheck: unlink

void ScenePairList::erase(ScenePairNode *node)
{
    if (node == head)
        head = head->next;
    unlink(node);
    count--;
}

// FS2 0x00171870
// callcheck: operator_delete, operator_delete

void ScenePairList::clear()
{
    ScenePairNode *node = head;
    while (node != nullptr) {
        ScenePairNode *next = node->next;
        delete node->value;
        delete node;
        node = next;
    }
    head = nullptr;
    count = 0;
}

// FS2 0x00171970

ScenePairNode *ScenePairList::tail() const
{
    ScenePairNode *last = head;
    for (ScenePairNode *node = head; node != nullptr; node = node->next)
        last = node;
    return last;
}

// FS2 0x001719a0
// callcheck: operator_delete, operator_delete

void ScenePairList::unlink(ScenePairNode *node)
{
    if (node->prev != nullptr)
        node->prev->next = node->next;
    if (node->next != nullptr)
        node->next->prev = node->prev;
    delete node->value;
    delete node;
}

// The five lists: the array's ctor.
// FS2 0x00171680
// callcheck: __construct_array

ScenePairTable::ScenePairTable()
{
}

// FS2 0x00171620
// callcheck: __destroy_arr, operator_delete

ScenePairTable::~ScenePairTable()
{
}

// FS2 0x001715f0
// callcheck: pushBack

void ScenePairTable::add(StageSideBase *entry, int list)
{
    lists[list].pushBack(entry);
}

// FS2 0x00171580
// callcheck: find, erase

void ScenePairTable::remove(StageSideBase *entry)
{
    for (int i = 0; i < ScenePairList_Count; i++) {
        ScenePairNode *node = lists[i].find(entry);
        if (node != nullptr)
            lists[i].erase(node);
    }
}

// FS2 0x001714e0

void ScenePairTable::actionStart()
{
    for (int i = 0; i < ScenePairList_Count; i++) {
        for (ScenePairNode *node = lists[i].head; node != nullptr; node = node->next) {
            StageSideBase *entry = *node->value;
            if (entry->m_viewActive)
                entry->sideHook0();
        }
    }
}

// FS2 0x00171440

void ScenePairTable::actionControl()
{
    for (int i = 0; i < ScenePairList_Count; i++) {
        for (ScenePairNode *node = lists[i].head; node != nullptr; node = node->next) {
            StageSideBase *entry = *node->value;
            if (entry->m_viewActive)
                entry->sideHook1();
        }
    }
}

// FS2 0x001713a0

void ScenePairTable::actionMovement()
{
    for (int i = 0; i < ScenePairList_Count; i++) {
        for (ScenePairNode *node = lists[i].head; node != nullptr; node = node->next) {
            StageSideBase *entry = *node->value;
            if (entry->m_viewActive)
                entry->sideHook2();
        }
    }
}

// FS2 0x00171300

void ScenePairTable::actionTerminate()
{
    for (int i = 0; i < ScenePairList_Count; i++) {
        for (ScenePairNode *node = lists[i].head; node != nullptr; node = node->next) {
            StageSideBase *entry = *node->value;
            if (entry->m_viewActive)
                entry->sideHook3();
        }
    }
}
