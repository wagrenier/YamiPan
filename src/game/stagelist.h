/* ==========================================================================
 *  game/stagelist.h
 *
 *  The two template containers of the stage's files (StageFiles and what
 *  reads it), one source for the many instances CodeWarrior emits at the
 *  end of game/stagescene.cpp, 0x1a4960..0x1a6790, and in the runs that use
 *  them (StringMap::find 0x189840):
 *  - StageList<T>: a doubly linked list with no tail pointer (pushBack walks
 *    to the end);
 *  - StringMap<T>: a binary tree on strcmp, unbalanced until balance is
 *    called, its nodes also chained in insertion order, the keys owned;
 *  - LiteralMap<T>: the same with keys that stay the caller's (literals or a
 *    file's own bytes): the three maps of PlayerMotion, emitted after it,
 *    0x1eee30..0x1efee4, and 0x1d1280, 0x281730.. for the third; StageModel's
 *    BoneMap and MotionMap, emitted after it, 0x188d60..0x189cd4.
 *  The classes' real names are unknown (no RTTI strings, no symbols).
 * ======================================================================== */
#ifndef _GAME_STAGELIST_H
#define _GAME_STAGELIST_H

#include <stddef.h>
#include <string.h>

// The instance on StageObject *, what a character carries (+0x22c) and the
// objects hung on a light or a block, emitted after game/stagecharacter.cpp:
//   FS2 0x0017e140  StageList<StageObject *>::StageList
//   FS2 0x0017e150  StageList<StageObject *>::~StageList, deleting
//   FS2 0x0017e1b0  StageList<StageObject *>::pushBack
//   FS2 0x0017e260  StageList<StageObject *>::erase
//   FS2 0x0017e2b0  StageList<StageObject *>::clear
//   FS2 0x0017e320  StageList<StageObject *>::pushFront, pushBack's empty case
//   FS2 0x0017e3b0  StageList<StageObject *>::getTail, pushBack's walk
//   FS2 0x0017e3e0  StageList<StageObject *>::Node::Node
//   FS2 0x0017e400  StageList<StageObject *>::Node::unlink, erase's tail and delete
// layout: host -- EE 8: +0 head, +4 count. An EE node is 0xc: +0 a new'd
// 4-byte box holding the value, +4 next, +8 prev; the host keeps the value
// in the node. Elements stay the owner's: clear deletes only the nodes.
template <class T>
class StageList
{
public:
    // layout: host (above)
    struct Node
    {
        T     value;            // FS2 +0x0 (through the box)
        Node *next;             // FS2 +0x4
        Node *prev;             // FS2 +0x8
    };

    // FS2 0x001a59f0, 0x001a5980, 0x001a5910, 0x001a58a0, 0x00186b10, 0x001f4bb0, 0x001f4c20: the instances'
    // constructors. CollisionBody's +0xd4 list (dtor, clear).
    // FS2 0x00184260, 0x001843c0
    StageList() : head(NULL), count(0) {}
    // FS2 0x001a5a00, 0x001a5990, 0x001a5920, 0x001a58b0, 0x00186b20, 0x00186b80: the destructors.
    ~StageList() { clear(); }

    // FS2 0x001a5e60, 0x001a5f80, 0x001a60a0, 0x001a6210, 0x00186c50, 0x00186be0, 0x001f4fd0
    void clear()
    {
        for (Node *node = head; node != NULL;) {
            Node *next = node->next;
            delete node;
            node = next;
        }
        head = NULL;
        count = 0;
    }

    // The ROM's empty case is a push at the head (0x1a65a0, 0x1a64e0,
    // 0x1a6420, 0x1a6390), its tail search a function of its own (0x1a6630,
    // 0x1a6570, 0x1a64b0), its node a two-argument constructor (0x1a6750,
    // 0x1a6730, 0x1a6710, 0x1a66f0).
    // The instance 0x1842c0's are 0x1844a0 (head), 0x184530 (tail), 0x184560 (node);
    // 0x1f4d30's 0x1f5100, 0x1f51e0, 0x1f5230; 0x1f4f20's 0x1f5040, 0x1f50d0, 0x1f5210.
    // FS2 0x001a5db0, 0x001a5ed0, 0x001a5ff0, 0x001a6140, 0x001a5d00, 0x001842c0, 0x0017e1b0, 0x001844a0, 0x00184530, 0x00184560, 0x001f4d30, 0x001f4f20, 0x001f5040, 0x001f50d0, 0x001f5100, 0x001f51e0, 0x001f5210, 0x001f5230
    void pushBack(const T &value)
    {
        Node *node = new Node;
        node->value = value;
        node->next = NULL;
        node->prev = NULL;
        count++;
        if (head == NULL) {
            head = node;
            return;
        }
        Node *tail = head;
        while (tail->next != NULL)
            tail = tail->next;
        node->prev = tail;
        tail->next = node;
    }

    // The index-th node; the first when the index is out of range.
    // FS2 0x001f4c30, 0x001f4e20
    Node *at(int index) const
    {
        Node *node = head;
        if (index < count) {
            for (int i = 0; i < index; i++)
                node = node->next;
        }
        return node;
    }

    // Unlinks and frees node.
    // FS2 0x00184370, 0x0017e260, 0x00184580 (the instance's unlink)
    void erase(Node *node)
    {
        if (node == head)
            head = head->next;
        if (node->prev != NULL)
            node->prev->next = node->next;
        if (node->next != NULL)
            node->next->prev = node->prev;
        delete node;
        count--;
    }

    Node *head;                 // FS2 +0x0
    int   count;                // FS2 +0x4

private:
    StageList(const StageList &);
    StageList &operator=(const StageList &);
};

// layout: host -- EE 0x10: +0 root, +4 first, +8 last, +0xc count. A node is
// EE 0x14, the fields below. Values stay the owner's. CopiesKeys is the one
// difference between the ROM's two templates: its node's constructor and
// destructor. The tree is the same code.
template <class T, bool CopiesKeys = true>
class StringMap
{
public:
    // layout: host (above)
    struct Node
    {
        T     value;            // FS2 +0x00
        const char *key;        // FS2 +0x04: new[]'d copy, or the caller's (LiteralMap)
        Node *left;             // FS2 +0x08: strcmp < 0
        Node *right;            // FS2 +0x0c
        Node *next;             // FS2 +0x10: insertion order
    };

    // FS2 0x001a4c20, 0x001a4ac0, 0x001a4960
    // LiteralMap's: FS2 0x001eee30, FS2 0x001eeee0, FS2 0x001ef040
    StringMap() : root(NULL), first(NULL), last(NULL), count(0) {}

    // The nodes' destructors (0x1a55c0, 0x1a51f0, 0x1a4e20) free the keys;
    // LiteralMap's free nothing: FS2 0x001ef1a0, FS2 0x001ef210, FS2 0x001ef540,
    // FS2 0x00189000, FS2 0x00189330.
    // The ROM's also zero root, first and last, not count.
    // FS2 0x001a4c40, 0x001a4ae0, 0x001a4980
    // LiteralMap's: FS2 0x001eee50, FS2 0x001eef00, FS2 0x001ef060,
    // FS2 0x00188d60, FS2 0x00188ea0
    ~StringMap()
    {
        for (Node *node = first; node != NULL;) {
            Node *next = node->next;
            if (CopiesKeys)
                delete[] node->key;
            delete node;
            node = next;
        }
    }

    // The node of key; null without one or when its value is null.
    // FS2 0x00189840, 0x001a5250
    // LiteralMap's, handed the root: FS2 0x001ef260, FS2 0x001ef590, FS2 0x001d1280,
    // FS2 0x00189050, FS2 0x00177270
    Node *find(const char *key) const
    {
        Node *node = root;
        while (node != NULL) {
            int order = strcmp(key, node->key);
            if (order == 0)
                return node->value != T() ? node : NULL;
            node = order < 0 ? node->left : node->right;
        }
        return NULL;
    }

    // The new node, chained last; null (nothing added) when key is taken.
    // The ROM's search recurses, with one level inlined (0x1a52d0, 0x1a5620).
    // FS2 0x001a4b70, 0x001a4cd0, 0x001a5180, 0x001a5550
    // LiteralMap's: FS2 0x001eef90, FS2 0x001ef0f0, FS2 0x00281730; their
    // searches FS2 0x001ef2e0, FS2 0x001ef610, FS2 0x00281800; their nodes'
    // constructors, value then key, FS2 0x001ef1f0, FS2 0x001ef520, FS2 0x002817e0.
    // StageModel's: FS2 0x00188df0, FS2 0x00188f30; searches FS2 0x001890d0,
    // FS2 0x00189380; nodes' constructors FS2 0x00188fe0, FS2 0x00189310.
    Node *insert(const char *key, const T &value)
    {
        Node **link = &root;
        while (*link != NULL) {
            int order = strcmp(key, (*link)->key);
            if (order == 0)
                return NULL;
            link = order < 0 ? &(*link)->left : &(*link)->right;
        }
        Node *node = new Node;
        node->value = value;
        node->key = copyKey(key);
        node->left = NULL;
        node->right = NULL;
        node->next = NULL;
        *link = node;
        if (first == NULL)
            first = node;
        else
            last->next = node;
        last = node;
        count++;
        return node;
    }

    // The node added index-th; null past the end.
    // FS2 0x001a4d80
    Node *at(int index) const
    {
        Node *node = first;
        for (int i = 0; node != NULL && i != index; i++)
            node = node->next;
        return node;
    }

    // Evens the tree out, once the map is full (bones, motions; of LiteralMap's
    // the conditions and the links, not the specials).
    void balance() { root = balanceFrom(root); }

    Node *root;                 // FS2 +0x0
    Node *first;                // FS2 +0x4
    Node *last;                 // FS2 +0x8
    int   count;                // FS2 +0xc

private:
    static const char *copyKey(const char *key)
    {
        if (!CopiesKeys)
            return key;
        char *copy = new char[strlen(key) + 1];
        strcpy(copy, key);
        return copy;
    }

    // LiteralMap's, two levels inlined: FS2 0x001efad0, FS2 0x001efce0,
    // FS2 0x001898c0, FS2 0x00189ad0
    static int height(const Node *node)
    {
        if (node == NULL)
            return 0;
        int right = height(node->right);
        int left = height(node->left);
        return (right > left ? right : left) + 1;
    }
    static int lean(const Node *node) { return height(node->right) - height(node->left); }

    // Rotations from the leaves up, until no node leans by more than one.
    // LiteralMap's: FS2 0x001ef850, FS2 0x001ef990, FS2 0x00189700, FS2 0x001895c0
    static Node *balanceFrom(Node *node)
    {
        Node *top;
        int after;
        do {
            int before;
            do {
                if (node == NULL)
                    return NULL;
                if (node->right != NULL)
                    node->right = balanceFrom(node->right);
                if (node->left != NULL)
                    node->left = balanceFrom(node->left);
                before = lean(node);
                if (before < 0) {
                    top = node->left;
                    node->left = top->right;
                    top->right = node;
                } else if (before > 0) {
                    top = node->right;
                    node->right = top->left;
                    top->left = node;
                } else {
                    return node;
                }
                node = top;
            } while (before != -1 && before != 1);
            after = lean(top);
        } while (after < -1 || after > 1);
        return top;
    }

    StringMap(const StringMap &);
    StringMap &operator=(const StringMap &);
};

template <class T>
using LiteralMap = StringMap<T, false>;

#endif /* _GAME_STAGELIST_H */
