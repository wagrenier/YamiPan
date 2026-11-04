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
 *    called, its nodes also chained in insertion order, the keys owned.
 *  The classes' real names are unknown (no RTTI strings, no symbols).
 * ======================================================================== */
#ifndef _GAME_STAGELIST_H
#define _GAME_STAGELIST_H

#include <stddef.h>
#include <string.h>

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

    // FS2 0x001a59f0, 0x001a5980, 0x001a5910, 0x001a58a0, 0x00186b10: the instances'
    // constructors. CollisionBody's +0xd4 list (dtor, clear) and its +0x14 block
    // (dtor, reset) are inline in the bullet's dtor and wait for that class's TU.
    // FS2 0x00184260, 0x001843c0, 0x00184430, 0x001845f0
    StageList() : head(NULL), count(0) {}
    // FS2 0x001a5a00, 0x001a5990, 0x001a5920, 0x001a58b0, 0x00186b20, 0x00186b80: the destructors.
    ~StageList() { clear(); }

    // FS2 0x001a5e60, 0x001a5f80, 0x001a60a0, 0x001a6210, 0x00186c50, 0x00186be0
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
    // The instance 0x1842c0's are 0x1844a0 (head), 0x184530 (tail), 0x184560 (node).
    // FS2 0x001a5db0, 0x001a5ed0, 0x001a5ff0, 0x001a6140, 0x001a5d00, 0x001842c0, 0x0017e1b0, 0x001844a0, 0x00184530, 0x00184560
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
// EE 0x14, the fields below. Values stay the owner's.
template <class T>
class StringMap
{
public:
    // layout: host (above)
    struct Node
    {
        T     value;            // FS2 +0x00
        char *key;              // FS2 +0x04: new[]'d copy
        Node *left;             // FS2 +0x08: strcmp < 0
        Node *right;            // FS2 +0x0c
        Node *next;             // FS2 +0x10: insertion order
    };

    // FS2 0x001a4c20, 0x001a4ac0, 0x001a4960
    StringMap() : root(NULL), first(NULL), last(NULL), count(0) {}

    // The nodes' destructors (0x1a55c0, 0x1a51f0, 0x1a4e20) free the keys.
    // FS2 0x001a4c40, 0x001a4ae0, 0x001a4980
    ~StringMap()
    {
        for (Node *node = first; node != NULL;) {
            Node *next = node->next;
            delete[] node->key;
            delete node;
            node = next;
        }
    }

    // The node of key; null without one or when its value is null.
    // FS2 0x00189840, 0x001a5250
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
        node->key = new char[strlen(key) + 1];
        strcpy(node->key, key);
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

    // Evens the tree out, once the map is full (bones, motions).
    void balance() { root = balanceFrom(root); }

    Node *root;                 // FS2 +0x0
    Node *first;                // FS2 +0x4
    Node *last;                 // FS2 +0x8
    int   count;                // FS2 +0xc

private:
    // FS2 0x00189ad0, 0x001898c0
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
    // FS2 0x001895c0, 0x00189700
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

#endif /* _GAME_STAGELIST_H */
