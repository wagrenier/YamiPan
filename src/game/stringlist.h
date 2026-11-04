/* ==========================================================================
 *  game/stringlist.h
 *
 *  The small text classes (game/stringlist.cpp, 0x148db0..0x149510):
 *  String, an owned 0-terminated text, and StringList, a list of them cut
 *  from a text by split or splitLines. Both names are the port's.
 * ======================================================================== */
#ifndef _GAME_STRINGLIST_H
#define _GAME_STRINGLIST_H

// An owned text. layout: host -- EE 0xc: the vptr (0x311200) at +8.
class String
{
public:
    // Inline in the ROM: no text yet.
    String() : m_text(nullptr), m_tokens(nullptr) {}
    explicit String(const char *text);                                   // FS2 0x00149040
    virtual ~String();                                                   // FS2 0x00148db0: slot 0, deleting

    // This text = text; a null text leaves m_text dangling, as the ROM does
    // (no caller passes one).
    String &assign(const char *text);                                    // FS2 0x00148f60
    // This text = other's.
    String &copyFrom(const String &other);                               // FS2 0x00148fd0
    String &append(const char *text);                                    // FS2 0x00148ec0
    // strtok over a private copy of the text; nextToken goes on with the
    // same (strtok's own static) position, whatever object it is called on.
    char *firstToken(const char *delimiters);                            // FS2 0x00148e40
    char *nextToken(const char *delimiters);                             // FS2 0x00148e30

    char *m_text;               // FS2 +0x0: new[]'d
    char *m_tokens;             // FS2 +0x4: new[]'d copy strtok cuts (firstToken)
};

// A list of Strings. layout: host -- EE 0xc: +0 head, +4 count, the vptr
// (0x311210) at +8. An EE node is 0xc: the fields of Node.
class StringList
{
public:
    struct Node
    {
        // FS2 0x001494f0
        explicit Node(String *text) : value(text), next(nullptr), prev(nullptr) {}

        String *value;          // FS2 +0x0
        Node   *next;           // FS2 +0x4
        Node   *prev;           // FS2 +0x8
    };

    // Inline in the ROM: the list base's 0x1a5a60, then the vptr.
    StringList() : head(0), count(0) {}
    virtual ~StringList();                                               // FS2 0x00149250 (0x1490a0: the vtable's wrapper)

    // Replaces the list with the words of text; answers the count.
    virtual int  splitString(const String &text, const char *delimiters); // FS2 0x00149230: slot 1
    virtual int  split(const char *text, const char *delimiters);        // FS2 0x00149100: slot 2

    void clear();                                                        // FS2 0x00149380
    // Appends a copy.
    void pushBack(const String &text);                                   // FS2 0x001492b0
    // Puts a copy first.
    void pushFront(const String &text);                                  // FS2 0x00149400
    // The last node; null when empty.
    Node *tail();                                                        // FS2 0x001494c0
    // Node index; the head when there is no such node.
    Node *at(int index);                                                 // FS2 0x002c1730

    Node *head;                 // FS2 +0x0
    int   count;                // FS2 +0x4
};

#endif /* _GAME_STRINGLIST_H */
