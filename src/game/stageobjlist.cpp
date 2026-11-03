// game/stageobjlist.cpp -- the stage's object registry (game/stageobjlist.h).

#include "game/stageobjlist.h"

#include <string.h>

#include "game/gamehooks.h"
#include "game/heap.h"
#include "sdk/scetypes.h"

// The arrays' blocks are 64-byte aligned on the global heap.
static const u_int kObjArrayAlign = 0x40;

// ──────────────────────────────────────────────────────────────────────
// The lookups. Each group's has the same body in the ROM (lookup /
// lookupAll inlined); they differ only in the map they read.
// callcheck: find (StageNameNode::find, in the inlined lookup)
// FS2 0x00193900

StageObjArray *StageObjectList::findAll7(const char *name)
{
    return lookupAll(StageObjGroup_7, name);
}

// callcheck: find (StageNameNode::find, in the inlined lookup)
// FS2 0x00193930

StageObjArray *StageObjectList::findAll6(const char *name)
{
    return lookupAll(StageObjGroup_6, name);
}

// callcheck: find (StageNameNode::find, in the inlined lookup)
// FS2 0x00193960

StageObjArray *StageObjectList::findAll5(const char *name)
{
    return lookupAll(StageObjGroup_5, name);
}

// callcheck: find (StageNameNode::find, in the inlined lookup)
// FS2 0x00193990

StageObjArray *StageObjectList::findAll3(const char *name)
{
    return lookupAll(StageObjGroup_3, name);
}

// ──────────────────────────────────────────────────────────────────────
// callcheck: find (StageNameNode::find, in the inlined lookup)
// FS2 0x001939c0

StageObject *StageObjectList::findAny(const char *name, int index)
{
    for (int group = 0; group < StageObjGroup_Count; group++) {
        StageObject *obj = lookup((StageObjGroup)group, name, index);
        if (obj != nullptr)
            return obj;
    }
    return nullptr;
}

// callcheck: find (StageNameNode::find, in the inlined lookup)
// FS2 0x00193aa0

StageObject *StageObjectList::find8(const char *name, int index)
{
    return lookup(StageObjGroup_8, name, index);
}

// callcheck: find (StageNameNode::find, in the inlined lookup)
// FS2 0x00193b30

StageObject *StageObjectList::find7(const char *name, int index)
{
    return lookup(StageObjGroup_7, name, index);
}

// callcheck: find (StageNameNode::find, in the inlined lookup)
// FS2 0x00193bc0

StageObject *StageObjectList::find6(const char *name, int index)
{
    return lookup(StageObjGroup_6, name, index);
}

// callcheck: find (StageNameNode::find, in the inlined lookup)
// FS2 0x00193c50

StageObject *StageObjectList::find5(const char *name, int index)
{
    return lookup(StageObjGroup_5, name, index);
}

// callcheck: find (StageNameNode::find, in the inlined lookup)
// FS2 0x00193ce0

StageObject *StageObjectList::find1(const char *name, int index)
{
    return lookup(StageObjGroup_1, name, index);
}

// ──────────────────────────────────────────────────────────────────────
// Walks the characters' list in creation order.
// FS2 0x00193d70

StageObject *StageObjectList::findCharacterById(int id)
{
    StageObjArray &list = m_lists[StageObjGroup_Characters];
    for (StageObject **slot = list.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
        if ((*slot)->m_id == id)
            return *slot;
    }
    return nullptr;
}

// callcheck: find (StageNameNode::find, in the inlined lookup)
// FS2 0x00193dd0

StageObject *StageObjectList::findCharacter(const char *name, int index)
{
    return lookup(StageObjGroup_Characters, name, index);
}

// callcheck: find (StageNameNode::find, in the inlined lookup)
// FS2 0x00193e60

StageObject *StageObjectList::find0(const char *name, int index)
{
    return lookup(StageObjGroup_0, name, index);
}

// ──────────────────────────────────────────────────────────────────────
// callcheck: erase, find (the inlined remove)
// FS2 0x00193ef0

void StageObjectList::removeBullet(StageObject *obj)
{
    remove(StageObjGroup_Bullets, obj);
}

// callcheck: erase, find (the inlined remove)
// FS2 0x00193f80

void StageObjectList::remove5(StageObject *obj)
{
    remove(StageObjGroup_5, obj);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00194010

void StageObjectList::rebalance()
{
    for (int group = 0; group < StageObjGroup_Count; group++) {
        if (m_names[group].m_root != nullptr)
            m_names[group].m_root = StageNameNode::balance(m_names[group].m_root);
    }
}

// ──────────────────────────────────────────────────────────────────────
// The registrations: the same body per group (add inlined).
// callcheck: StageObjArray, find, insert, operator_new, push (the inlined add)
// FS2 0x00194070

void StageObjectList::add8(StageObject *obj)
{
    add(StageObjGroup_8, obj);
}

// callcheck: StageObjArray, find, insert, operator_new, push (the inlined add)
// FS2 0x00194110

void StageObjectList::add7(StageObject *obj)
{
    add(StageObjGroup_7, obj);
}

// callcheck: StageObjArray, find, insert, operator_new, push (the inlined add)
// FS2 0x001941b0

void StageObjectList::add6(StageObject *obj)
{
    add(StageObjGroup_6, obj);
}

// callcheck: StageObjArray, find, insert, operator_new, push (the inlined add)
// FS2 0x00194250

void StageObjectList::add5(StageObject *obj)
{
    add(StageObjGroup_5, obj);
}

// callcheck: StageObjArray, find, insert, operator_new, push (the inlined add)
// FS2 0x001942f0

void StageObjectList::addBullet(StageObject *obj)
{
    add(StageObjGroup_Bullets, obj);
}

// callcheck: StageObjArray, find, insert, operator_new, push (the inlined add)
// FS2 0x00194390

void StageObjectList::add3(StageObject *obj)
{
    add(StageObjGroup_3, obj);
}

// callcheck: StageObjArray, find, insert, operator_new, push (the inlined add)
// FS2 0x00194430

void StageObjectList::add1(StageObject *obj)
{
    add(StageObjGroup_1, obj);
}

// callcheck: StageObjArray, find, insert, operator_new, push (the inlined add)
// FS2 0x001944d0

void StageObjectList::addCharacter(StageObject *obj)
{
    add(StageObjGroup_Characters, obj);
    obj->m_id = ++m_lastId;
}

// callcheck: StageObjArray, find, insert, operator_new, push (the inlined add)
// FS2 0x00194590

void StageObjectList::add0(StageObject *obj)
{
    add(StageObjGroup_0, obj);
}

// ──────────────────────────────────────────────────────────────────────
// Per group: the name arrays (along the insertion chain), then the
// objects. The ROM reads a node's array through the node even when it is
// null (never: add always sets one). The maps and lists then destroy
// themselves (__destroy_arr with 0x194930 / 0x1947a0 in the ROM).
// callcheck: __destroy_arr, operator_delete, ~StageObjArray (the delete-expression and the member arrays' implicit dtors)
// FS2 0x00194630

StageObjectList::~StageObjectList()
{
    for (int group = 0; group < StageObjGroup_Count; group++) {
        for (StageNameNode *node = m_names[group].m_first; node != nullptr; node = node->next) {
            delete node->value;
        }
        StageObjArray &list = m_lists[group];
        for (StageObject **slot = list.begin(); slot != nullptr; slot = StageObjArray::next(slot)) {
            if (*slot != nullptr)
                (*slot)->destroy();
        }
    }
}

// callcheck: __construct_array (the member arrays' implicit ctors)
// FS2 0x00194800

StageObjectList::StageObjectList()
    : m_lastId(0)
{
}

// The array copies of the StageObjArray ctor / dtor that __construct_array
// and __destroy_arr receive; the host's member arrays need neither.
//   FS2 0x001947a0  StageObjArray::dtorForArray (0x1948a0, then delete)
//   FS2 0x00194870  StageObjArray::ctorForArray (0x1950a0)

// ──────────────────────────────────────────────────────────────────────
// callcheck: operator_delete (the deleting dtor's delete, implicit)
// FS2 0x001948a0

StageObjArray::~StageObjArray()
{
    clear();
    gGlobalHeap.free(m_data);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00194910

StageNameMap::StageNameMap()
    : m_root(nullptr), m_first(nullptr), m_last(nullptr), m_count(0)
{
}

// Leaves m_count as it was (the ROM zeroes the three pointers only).
// callcheck: operator_delete, ~StageNameNode (the delete-expression)
// FS2 0x00194930

StageNameMap::~StageNameMap()
{
    StageNameNode *node = m_first;
    while (node != nullptr) {
        StageNameNode *next = node->next;
        delete node;                    // FS2 0x00194a90: the trivial node dtor
        node = next;
    }
    m_root = nullptr;
    m_first = nullptr;
    m_last = nullptr;
}

// callcheck: operator_new (the new-expression)
// FS2 0x001949c0

StageNameNode *StageNameMap::insert(const char *key, StageObjArray *const &value)
{
    if (m_root == nullptr) {
        m_root = new StageNameNode(key, value);
        m_count++;
        m_first = m_root;
        m_last = m_root;
        return m_root;
    }
    StageNameNode *node = m_root->insert(key, value);
    if (node == nullptr)
        return nullptr;
    m_count++;
    m_last->next = node;
    m_last = node;
    return node;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00194a70

StageNameNode::StageNameNode(const char *key, StageObjArray *const &value)
    : value(value), key(key), left(nullptr), right(nullptr), next(nullptr)
{
}

// FS2 0x00194ae0

StageNameNode *StageNameNode::find(StageNameNode *node, const char *key)
{
    while (node != nullptr) {
        int cmp = strcmp(key, node->key);
        if (cmp == 0)
            return node->value == nullptr ? nullptr : node;
        node = cmp < 0 ? node->left : node->right;
    }
    return nullptr;
}

// The ROM unrolls the first level of the recursion.
// callcheck: operator_new (the new-expression)
// FS2 0x00194b60

StageNameNode *StageNameNode::insert(const char *key, StageObjArray *const &value)
{
    int cmp = strcmp(key, this->key);
    if (cmp == 0)
        return nullptr;
    StageNameNode *&child = cmp < 0 ? left : right;
    if (child == nullptr) {
        child = new StageNameNode(key, value);
        return child;
    }
    return child->insert(key, value);
}

// height itself never takes null; every caller tests first.
static inline int subtreeHeight(StageNameNode *node)
{
    return node == nullptr ? 0 : (int)StageNameNode::height(node);
}

// Children first, then a rotation toward the taller side whenever the
// heights differ at all (a difference of one included), repeated from the
// new top until that top differs by at most one.
// callcheck: height (behind subtreeHeight's null test)
// FS2 0x00194da0

StageNameNode *StageNameNode::balance(StageNameNode *node)
{
    for (;;) {
        if (node == nullptr)
            return nullptr;
        if (node->right != nullptr)
            node->right = balance(node->right);
        if (node->left != nullptr)
            node->left = balance(node->left);

        int diff = subtreeHeight(node->right) - subtreeHeight(node->left);
        StageNameNode *top;
        if (diff < 0) {
            top = node->left;
            node->left = top->right;
            top->right = node;
        } else if (diff == 0) {
            return node;
        } else {
            top = node->right;
            node->right = top->left;
            top->left = node;
        }
        node = top;

        if (diff == -1 || diff == 1) {
            int topDiff = subtreeHeight(top->right) - subtreeHeight(top->left);
            if (topDiff >= -1 && topDiff <= 1)
                return top;
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00194ee0

StageObject **StageObjArray::find(StageObject *const &obj)
{
    for (StageObject **slot = begin(); slot != nullptr; slot = next(slot)) {
        if (*slot == obj)
            return slot;
    }
    return nullptr;
}

// FS2 0x00194f40

StageObject **StageObjArray::erase(StageObject **slot)
{
    if (m_count == 0)
        return nullptr;
    for (StageObject **from = next(slot); from != nullptr; from = next(from))
        from[-1] = *from;
    m_data[m_count] = nullptr;
    m_count--;
    return *slot == nullptr ? nullptr : slot;
}

// Doubles the block (capacity + 2 words) when full.
// FS2 0x00194fd0

int StageObjArray::push(StageObject *const &obj)
{
    if (m_count >= m_capacity)
        reserve((u_short)((m_capacity + 2) * 2 - 2));
    m_count++;
    m_data[m_count] = obj;
    m_data[m_count + 1] = nullptr;
    return 1;
}

// FS2 0x00195080

void StageObjArray::clear()
{
    m_count = 0;
    m_data[1] = nullptr;
}

// FS2 0x001950a0

StageObjArray::StageObjArray()
    : m_count(0), m_capacity(kInitialCapacity)
{
    m_data = (StageObject **)gGlobalHeap.alloc((m_capacity + 2) * sizeof(StageObject *), kObjArrayAlign);
    m_data[0] = nullptr;
    m_data[1] = nullptr;
}

// FS2 0x00195120

void StageObjArray::reserve(u_short capacity)
{
    if (capacity <= m_capacity)
        return;
    u_short count = m_count;
    StageObject **old = m_data;
    m_capacity = capacity;
    m_data = (StageObject **)gGlobalHeap.alloc((m_capacity + 2) * sizeof(StageObject *), kObjArrayAlign);
    for (int i = 0; i < count + 2; i++)
        m_data[i] = old[i];
    gGlobalHeap.free(old);
}

// ──────────────────────────────────────────────────────────────────────
// 1 + the taller child's height; an empty child is 0. The ROM unrolls two
// levels of the recursion.
// FS2 0x001951e0

u_int StageNameNode::height(StageNameNode *node)
{
    u_int right = node->right == nullptr ? 0 : height(node->right);
    u_int left = node->left == nullptr ? 0 : height(node->left);
    return (right > left ? right : left) + 1;
}

// ──────────────────────────────────────────────────────────────────────
// The bodies the ROM inlines into each group's function.

// A new name gets a new array.
void StageObjectList::add(StageObjGroup group, StageObject *obj)
{
    StageNameNode *node = StageNameNode::find(m_names[group].m_root, obj->m_name);
    if (node == nullptr) {
        StageObjArray *array = new StageObjArray;
        array->push(obj);
        m_names[group].insert(obj->m_name, array);
    } else {
        node->value->push(obj);
    }
    m_lists[group].push(obj);
}

void StageObjectList::remove(StageObjGroup group, StageObject *obj)
{
    StageNameNode *node = StageNameNode::find(m_names[group].m_root, obj->m_name);
    if (node == nullptr)
        return;
    StageObject **slot = node->value->find(obj);
    if (slot != nullptr)
        node->value->erase(slot);
    slot = m_lists[group].find(obj);
    if (slot != nullptr)
        m_lists[group].erase(slot);
}

StageObject *StageObjectList::lookup(StageObjGroup group, const char *name, int index)
{
    StageNameNode *node = StageNameNode::find(m_names[group].m_root, name);
    if (node == nullptr)
        return nullptr;
    return node->value->at(index);
}

StageObjArray *StageObjectList::lookupAll(StageObjGroup group, const char *name)
{
    StageNameNode *node = StageNameNode::find(m_names[group].m_root, name);
    return node == nullptr ? nullptr : node->value;
}
