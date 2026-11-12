/* ==========================================================================
 *  game/stringlist.cpp
 *
 *  The small text classes String and StringList (game/stringlist.h),
 *  0x148db0..0x149510. Everything that reads text line by line or word by
 *  word goes through them: the stage scripts (StageFiles::parseScript).
 * ======================================================================== */
#include "game/stringlist.h"

#include <string.h>

// FS2 0x00149040
// callcheck: strlen, operator_new[], strcpy

String::String(const char *text)
{
    m_text = new char[strlen(text) + 1];
    m_tokens = nullptr;
    strcpy(m_text, text);
}

// FS2 0x00148db0
// callcheck: operator_delete[], operator_delete (the deleting form)

String::~String()
{
    delete[] m_text;
    delete[] m_tokens;
}

// FS2 0x00148f60
// callcheck: operator_delete[], strlen, operator_new[], strcpy

String &String::assign(const char *text)
{
    delete[] m_text;
    if (text != nullptr) {
        m_text = new char[strlen(text) + 1];
        strcpy(m_text, text);
    }
    return *this;
}

// FS2 0x00148fd0
// callcheck: operator_delete[], strlen, operator_new[], strcpy

String &String::copyFrom(const String &other)
{
    delete[] m_text;
    m_text = new char[strlen(other.m_text) + 1];
    strcpy(m_text, other.m_text);
    return *this;
}

// FS2 0x00148ec0
// callcheck: strlen, strlen, operator_new[], strcpy, strcat, operator_delete[]

String &String::append(const char *text)
{
    char *joined = new char[strlen(m_text) + strlen(text) + 1];
    strcpy(joined, m_text);
    strcat(joined, text);
    delete[] m_text;
    m_text = joined;
    return *this;
}

// FS2 0x00148e40
// callcheck: operator_delete[], strlen, operator_new[], strcpy, strtok

char *String::firstToken(const char *delimiters)
{
    delete[] m_tokens;
    m_tokens = new char[strlen(m_text) + 1];
    strcpy(m_tokens, m_text);
    return strtok(m_tokens, delimiters);
}

// FS2 0x00148e30
// callcheck: strtok

char *String::nextToken(const char *delimiters)
{
    return strtok(nullptr, delimiters);
}

// FS2 0x00149100
// callcheck: String, clear, firstToken, assign, pushBack, nextToken, operator_delete[], operator_delete[], operator_delete[], operator_delete[] (the two Strings' inline dtors)

int StringList::split(const char *text, const char *delimiters)
{
    String source(text);
    String word;
    clear();
    for (char *token = source.firstToken(delimiters); token != nullptr;
         token = source.nextToken(delimiters)) {
        word.assign(token);
        pushBack(word);
    }
    return count;
}

// FS2 0x00149230
// callcheck: split (the virtual slot)

int StringList::splitString(const String &text, const char *delimiters)
{
    return split(text.m_text, delimiters);
}

// FS2 0x00149250
// callcheck: clear, operator_delete (the deleting form)

StringList::~StringList()
{
    clear();
}

// FS2 0x001492b0
// callcheck: pushFront, tail, operator_new, operator_new, copyFrom (new String, new Node)

void StringList::pushBack(const String &text)
{
    if (head == nullptr) {
        pushFront(text);
        return;
    }
    Node *last = tail();
    String *copy = new String;
    copy->copyFrom(text);
    Node *node = new Node(copy);
    node->prev = last;
    last->next = node;
    count++;
}

// FS2 0x00149380
// callcheck: operator_delete (the node; the String's deleting dtor is the virtual slot)

void StringList::clear()
{
    Node *node = head;
    while (node != nullptr) {
        Node *next = node->next;
        delete node->value;
        delete node;
        node = next;
    }
    head = nullptr;
    count = 0;
}

// FS2 0x00149400
// callcheck: operator_new, operator_new, copyFrom (new String, new Node)

void StringList::pushFront(const String &text)
{
    Node *first = head;
    String *copy = new String;
    copy->copyFrom(text);
    head = new Node(copy);
    if (first != nullptr) {
        head->next = first;
        first->prev = head;
    }
    count++;
}

// FS2 0x001494c0

StringList::Node *StringList::tail()
{
    Node *last = head;
    for (Node *node = head; node != nullptr; node = node->next)
        last = node;
    return last;
}
