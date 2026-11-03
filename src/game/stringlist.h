/* ==========================================================================
 *  game/stringlist.h
 *
 *  The small text classes of the run unassigned@00148db0 (0x148db0..0x149520;
 *  their TU is not written yet, this is what game/stagescene.cpp calls):
 *  String, an owned 0-terminated text, and StringList, a list of them cut
 *  from a text by split or splitLines. Both names are the port's.
 * ======================================================================== */
#ifndef _GAME_STRINGLIST_H
#define _GAME_STRINGLIST_H

// An owned text. layout: host -- EE 0xc: the vptr (0x311200) at +8.
class String
{
public:
    explicit String(const char *text);                                   // FS2 0x00149040
    virtual ~String();                                                   // FS2 0x00148db0: slot 0, deleting

    // This text = other's.
    String &copyFrom(const String &other);                               // FS2 0x00148fd0
    String &append(const char *text);                                    // FS2 0x00148ec0

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
        String *value;          // FS2 +0x0
        Node   *next;           // FS2 +0x4
        Node   *prev;           // FS2 +0x8
    };

    // Inline in the ROM: the list base's 0x1a5a60, then the vptr.
    StringList() : head(0), count(0) {}
    virtual ~StringList();                                               // FS2 0x00149250: slot 0, deleting

    // Replaces the list with the words of text; answers the count.
    virtual int  splitString(const String &text, const char *delimiters); // FS2 0x00149230: slot 1
    virtual int  split(const char *text, const char *delimiters);        // FS2 0x00149100: slot 2

    void clear();                                                        // FS2 0x00149380
    // Appends a copy.
    void pushBack(const String &text);                                   // FS2 0x001492b0
    // The last node; null when empty.
    Node *tail();                                                        // FS2 0x001494c0

    Node *head;                 // FS2 +0x0
    int   count;                // FS2 +0x4
};

#endif /* _GAME_STRINGLIST_H */
