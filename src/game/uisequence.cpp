// game/uisequence.cpp -- a 2D layout's running sequence: its actors, their
// anims, the widget base and the merged priority walk that advances and
// draws them (game/uisequence.h).

#include "game/uisequence.h"

#include <string.h>

#include "game/heap.h"
#include "game/s2ddata.h"
#include "game/uidrawer.h"
#include "game/uilayout.h"
#include "sdk/scetypes.h"

// The actor and widget arrays grow by this many entries.
constexpr int kActorArrayGrow = 16;
constexpr int kWidgetArrayGrow = 16;

// ──────────────────────────────────────────────────────────────────────
// Private helpers, inline in the ROM.

inline void UiSequence::sortActors()
{
    if (m_actors == nullptr)
        return;
    for (int i = 0; i < m_actorCount - 1; i++) {
        for (int j = i + 1; j < m_actorCount; j++) {
            UiActor *a = m_actors[i];
            UiActor *b = m_actors[j];
            if (b->m_priority < a->m_priority) {
                m_actors[i] = b;
                m_actors[j] = a;
            }
        }
    }
}

inline void UiSequence::sortWidgets()
{
    if (m_widgets == nullptr)
        return;
    for (int i = 0; i < m_widgetCount - 1; i++) {
        for (int j = i + 1; j < m_widgetCount; j++) {
            UiWidget *a = m_widgets[i];
            UiWidget *b = m_widgets[j];
            if (b->m_priority < a->m_priority) {
                m_widgets[i] = b;
                m_widgets[j] = a;
            }
        }
    }
}

inline UiActor *UiSequence::addActor()
{
    if (m_actorCapacity <= m_actorCount) {
        if (m_actors == nullptr)
            m_actors = (UiActor **)gGlobalHeap.alloc(kActorArrayGrow * sizeof(UiActor *), 16);
        else
            m_actors = (UiActor **)gGlobalHeap.realloc(m_actors, (m_actorCapacity + kActorArrayGrow) * sizeof(UiActor *));
        for (int i = m_actorCapacity; i < m_actorCapacity + kActorArrayGrow; i++)
            m_actors[i] = nullptr;
        m_actorCapacity += kActorArrayGrow;
    }
    UiActor *actor = new UiActor(m_data);
    m_actors[m_actorCount] = actor;
    m_actorCount++;
    return actor;
}

// ──────────────────────────────────────────────────────────────────────
// The anim playing the object named by string nameString, or null.
// FS2 0x00246230
// callcheck: findAnim (inline in the ROM)

UiAnim *UiSequence::findAnimByName(int nameString)
{
    const char *name = m_data->getString(nameString);
    if (name == nullptr)
        return nullptr;
    int obj = m_data->findObject(name);
    if (obj < 0)
        return nullptr;
    return findAnim(obj);
}

// ──────────────────────────────────────────────────────────────────────
// The first anim of the sequence's actors playing object objIndex, or null.
// FS2 0x00246340

UiAnim *UiSequence::findAnim(int objIndex)
{
    for (int i = 0; i < m_actorCount; i++) {
        if (m_actors[i] == nullptr)
            continue;
        UiAnim *anim = m_actors[i]->findAnim(objIndex);
        if (anim != nullptr)
            return anim;
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Draws the actors and widgets in priority order, merged: before each
// actor, the widgets at or below its priority. Each actor's anims build its
// matrix, colour and offset colour from identity / 1 / 0; the colour is
// multiplied by the sequence's base colour and the layout's, the offset
// added to theirs. An actor with a non-zero offset colour is drawn again
// in the drawer's additive pass.
// FS2 0x00246400
// callcheck: identity, setPixelMode, copy, apply, setTranslate, concat, updateMatrix, setAddPass, draw (sortActors / sortWidgets / drawTracks inline in the ROM)

void UiSequence::draw(UiDrawer *drawer)
{
    UiVec4 baseColor = {1.0f, 1.0f, 1.0f, 1.0f};
    UiVec4 baseOffset = {0.0f, 0.0f, 0.0f, 0.0f};

    sortActors();
    sortWidgets();

    int w = 0;
    m_xform.identity();
    m_parentXform.identity();
    drawer->setPixelMode(1);
    for (int i = 0; i < m_actorCount; i++) {
        if (m_actors[i] == nullptr)
            continue;
        // The widget scan runs over the whole capacity (empty slots skipped).
        for (; w < m_widgetCapacity; w++) {
            UiWidget *widget = m_widgets[w];
            if (widget == nullptr)
                continue;
            if (m_actors[i]->m_priority < widget->m_priority)
                break;
            widget->draw(drawer);
        }

        UiMatrix xform;
        UiMatrix parent;
        xform.copy(m_xform);
        parent.copy(m_parentXform);
        UiVec4 color = {1.0f, 1.0f, 1.0f, 1.0f};
        UiVec4 offset = {0.0f, 0.0f, 0.0f, 0.0f};
        UiDrawState state = {UiCoord_Keep, 0, 0, 0};
        UiActor *actor = m_actors[i];
        for (u_int j = 0; j < actor->m_animCount; j++)
            actor->m_anims[j]->apply(&xform, &parent, &color, &offset, &state);

        float savedTranslate[2] = {drawer->m_translate[0], drawer->m_translate[1]};
        if (state.coordMode == UiCoord_Screen) {
            drawer->setPixelMode(0);
            float none[2] = {0.0f, 0.0f};
            drawer->setTranslate(none);
        } else if (state.coordMode == UiCoord_Pixel) {
            drawer->setPixelMode(1);
            float none[2] = {0.0f, 0.0f};
            drawer->setTranslate(none);
        }

        if (state.setBase) {
            baseColor = color;
            baseOffset = offset;
        }
        if (state.setLayout) {
            m_layout->m_color = color;
            m_layout->m_offset = offset;
        }
        // With setBase the actor's own colour is applied twice, as the ROM
        // does.
        const UiVec4 &layoutColor = m_layout->m_color;
        color.x = color.x * baseColor.x * layoutColor.x;
        color.y = color.y * baseColor.y * layoutColor.y;
        color.z = color.z * baseColor.z * layoutColor.z;
        color.w = color.w * baseColor.w * layoutColor.w;
        const UiVec4 &layoutOffset = m_layout->m_offset;
        offset.x = offset.x + baseOffset.x + layoutOffset.x;
        offset.y = offset.y + baseOffset.y + layoutOffset.y;
        offset.z = offset.z + baseOffset.z + layoutOffset.z;
        offset.w = offset.w + baseOffset.w + layoutOffset.w;

        xform.concat(parent);
        drawer->m_stack[drawer->m_top].copy(xform);
        drawer->updateMatrix();
        drawer->m_color = color;
        drawer->m_fixedColor = state.drawerFlag;
        m_actors[i]->drawTracks(drawer);

        if (offset.x != 0.0f || offset.y != 0.0f || offset.z != 0.0f || offset.w != 0.0f) {
            drawer->m_addColor = offset;
            u_char old = drawer->setAddPass(1);
            m_actors[i]->drawTracks(drawer);
            drawer->setAddPass(old);
        }
        drawer->setTranslate(savedTranslate);
    }
    for (; w < m_widgetCapacity; w++) {
        if (m_widgets[w] != nullptr)
            m_widgets[w]->draw(drawer);
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00246da0

void UiWidget::draw(UiDrawer *drawer)
{
}

// ──────────────────────────────────────────────────────────────────────
// Steps every track of every actor and advances the widgets, in the same
// merged priority walk as draw (only the widgets are sorted here).
// FS2 0x00246db0
// callcheck: step, advance (sortWidgets inline in the ROM)

void UiSequence::advance()
{
    sortWidgets();

    int w = 0;
    for (int i = 0; i < m_actorCount; i++) {
        if (m_actors[i] == nullptr)
            continue;
        for (; w < m_widgetCapacity; w++) {
            UiWidget *widget = m_widgets[w];
            if (widget == nullptr)
                continue;
            if (m_actors[i]->m_priority < widget->m_priority)
                break;
            widget->advance();
        }
        UiActor *actor = m_actors[i];
        for (u_int j = 0; j < actor->m_animCount; j++) {
            UiAnim *anim = actor->m_anims[j];
            for (u_int k = 0; k < anim->m_trackCount; k++)
                anim->m_tracks[k]->step();
        }
    }
    for (; w < m_widgetCapacity; w++) {
        if (m_widgets[w] != nullptr)
            m_widgets[w]->advance();
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00247060

void UiWidget::advance()
{
}

// ──────────────────────────────────────────────────────────────────────
// Play flags for the anims each entry of the set names, on the actors of
// this sequence (entries of a missing actor and anim -1 are skipped).
// FS2 0x00247070
// callcheck: setAnimPlay (findActor inline in the ROM)

void UiSequence::setAnimSetPlay(S2dAnimSet *set, u_int flags)
{
    if (set == nullptr)
        return;
    int lastId = -1;
    UiActor *actor = nullptr;
    for (u_int i = 0; i < set->count; i++) {
        const S2dAnimSetEntry &entry = set->entries[i];
        if (entry.actorId != lastId) {
            lastId = entry.actorId;
            actor = findActor(entry.actorId);
        }
        if (actor == nullptr)
            lastId = -1;
        else if (entry.anim != UiAnimSelect_None)
            actor->setAnimPlay(flags, entry.anim);
    }
}

// ──────────────────────────────────────────────────────────────────────
// As setAnimSetPlay, with a frame.
// FS2 0x002471d0
// callcheck: setAnimFrame (findActor inline in the ROM)

void UiSequence::setAnimSetFrame(S2dAnimSet *set, int frame)
{
    if (set == nullptr)
        return;
    int lastId = -1;
    UiActor *actor = nullptr;
    for (u_int i = 0; i < set->count; i++) {
        const S2dAnimSetEntry &entry = set->entries[i];
        if (entry.actorId != lastId) {
            lastId = entry.actorId;
            actor = findActor(entry.actorId);
        }
        if (actor == nullptr)
            lastId = -1;
        else if (entry.anim != UiAnimSelect_None)
            actor->setAnimFrame(frame, entry.anim);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Brings the set's actors into the sequence (a missing one is created from
// its definition), gives each its entry's priority and starts the anims
// the entries name; an entry with anim -1 and UiPlay_Stop removes its actor.
// FS2 0x00247330
// callcheck: alloc, realloc, operator_new, init, removeActor, startAnim (findActor / addActor inline in the ROM)

void UiSequence::addAnimSet(S2dAnimSet *set)
{
    if (set == nullptr)
        return;
    int lastId = -1;
    UiActor *actor = nullptr;
    for (u_int i = 0; i < set->count; i++) {
        const S2dAnimSetEntry &entry = set->entries[i];
        if (entry.actorId != lastId) {
            actor = findActor(entry.actorId);
            lastId = entry.actorId;
            if (actor == nullptr) {
                actor = addActor();
                actor->init(m_data->actorDefs[entry.actorId], entry.actorId, entry.priority);
            }
        }
        actor->m_priority = entry.priority;
        if (entry.anim == UiAnimSelect_None && (entry.flags & UiPlay_Stop)) {
            removeActor(lastId);
            lastId = -1;
            actor = nullptr;
        } else {
            actor->startAnim(entry.flags, entry.anim);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x002475f0

UiWidget **UiSequence::reserveWidgetSlot()
{
    if (m_widgetCapacity <= m_widgetCount) {
        if (m_widgets == nullptr)
            m_widgets = (UiWidget **)gGlobalHeap.alloc(kWidgetArrayGrow * sizeof(UiWidget *), 16);
        else
            m_widgets = (UiWidget **)gGlobalHeap.realloc(m_widgets, (m_widgetCapacity + kWidgetArrayGrow) * sizeof(UiWidget *));
        for (int i = m_widgetCapacity; i < m_widgetCapacity + kWidgetArrayGrow; i++)
            m_widgets[i] = nullptr;
        m_widgetCapacity += kWidgetArrayGrow;
    }
    return &m_widgets[m_widgetCount];
}

// ──────────────────────────────────────────────────────────────────────
// Deletes the widget with that id (the last one takes its slot) and
// answers the slot, or -1; id < 0 deletes them all and the array (answers 0).
// FS2 0x002476c0
// callcheck: free, ~UiWidget (the delete-expression, a virtual call in the ROM)

int UiSequence::removeWidget(int id)
{
    if (m_widgets == nullptr)
        return 0;
    if (id < 0) {
        for (int i = 0; i < m_widgetCount; i++) {
            if (m_widgets[i] != nullptr)
                delete m_widgets[i];
        }
        gGlobalHeap.free(m_widgets);
        m_widgetCapacity = 0;
        m_widgetCount = 0;
        m_widgets = nullptr;
        return 0;
    }
    for (int i = 0; i < m_widgetCount; i++) {
        if (id == m_widgets[i]->m_id) {
            if (m_widgets[i] != nullptr)
                delete m_widgets[i];
            m_widgets[i] = nullptr;
            UiWidget *last = m_widgets[m_widgetCount - 1];
            m_widgets[m_widgetCount - 1] = nullptr;
            m_widgets[i] = last;
            m_widgetCount--;
            return i;
        }
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00247820
// callcheck: operator_delete (the deleting destructor)

UiWidget::~UiWidget()
{
}

// ──────────────────────────────────────────────────────────────────────
// The widget whose definition has that name, or null.
// FS2 0x00247870

UiWidget *UiSequence::findWidgetByName(const char *name)
{
    if (m_widgets == nullptr)
        return nullptr;
    for (int i = 0; i < m_widgetCapacity; i++) {
        UiWidget *widget = m_widgets[i];
        if (widget == nullptr)
            continue;
        const char *own = widget->m_def != nullptr ? (const char *)widget->m_def->name : nullptr;
        // PC: the ROM hands a null name to strcmp; the host skips it.
        if (own != nullptr && strcmp(own, name) == 0)
            return m_widgets[i];
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00247930

UiWidget *UiSequence::findWidget(int id)
{
    if (m_widgets == nullptr || id < 0)
        return nullptr;
    for (int i = 0; i < m_widgetCapacity; i++) {
        if (m_widgets[i] != nullptr && id == m_widgets[i]->m_id)
            return m_widgets[i];
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Deletes the actor with that id (the last one takes its slot) and answers
// the slot, or -1; id < 0 deletes them all and the array (answers 0).
// FS2 0x002479b0
// callcheck: free, operator_delete, ~UiTrack (the delete-expressions: ~UiActor / ~UiAnim inline, the track's a virtual call in the ROM)

int UiSequence::removeActor(int id)
{
    if (m_actors == nullptr)
        return 0;
    if (id < 0) {
        for (int i = 0; i < m_actorCount; i++) {
            if (m_actors[i] != nullptr)
                delete m_actors[i];
        }
        gGlobalHeap.free(m_actors);
        m_actorCapacity = 0;
        m_actorCount = 0;
        m_actors = nullptr;
        return 0;
    }
    for (int i = 0; i < m_actorCount; i++) {
        if (id == m_actors[i]->m_id) {
            if (m_actors[i] != nullptr)
                delete m_actors[i];
            m_actors[i] = nullptr;
            UiActor *last = m_actors[m_actorCount - 1];
            m_actors[m_actorCount - 1] = nullptr;
            m_actors[i] = last;
            m_actorCount--;
            return i;
        }
    }
    return -1;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00247d10
// callcheck: operator_delete (the deleting destructor)

UiTrack::~UiTrack()
{
}

// ──────────────────────────────────────────────────────────────────────
// The layout fills the definition, commands and priority after (in
// UiLayout::startSequence); the two matrices are set at each draw.
// FS2 0x00247d60

UiSequence::UiSequence(S2dData *data, UiLayout *layout, int value)
    : m_def(nullptr), m_id(-1), m_priority(0), m_cmdBegin(nullptr), m_cmdEnd(nullptr),
      m_cursor(nullptr), m_data(data), m_actorCapacity(0), m_actorCount(0), m_actors(nullptr),
      m_widgetCapacity(0), m_widgetCount(0), m_widgets(nullptr), m_pending(0), m_layout(layout),
      m_value(value)
{
}

// ──────────────────────────────────────────────────────────────────────
// The first track of kind kind in the anim named by string nameString in
// the widget's sequence, or null.
// FS2 0x00247db0
// callcheck: getString, findObject (findAnim and UiSequence::findAnim inline in the ROM)

UiTrack *UiWidget::findTrack(int nameString, int kind)
{
    UiAnim *anim = findAnim(nameString);
    if (anim == nullptr)
        return nullptr;
    for (u_int i = 0; i < anim->m_trackCount; i++) {
        if (kind == anim->m_tracks[i]->m_rec->kind)
            return anim->m_tracks[i];
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The anim playing the object named by string nameString in the widget's
// sequence, or null (UiSequence::findAnimByName on the widget's own data).
// FS2 0x00247f20
// callcheck: findAnim (inline in the ROM)

UiAnim *UiWidget::findAnim(int nameString)
{
    const char *name = m_data->getString(nameString);
    if (name == nullptr)
        return nullptr;
    int obj = m_data->findObject(name);
    if (obj < 0)
        return nullptr;
    return m_seq->findAnim(obj);
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00248030

void UiWidget::setDef(S2dWidgetDef *def)
{
    m_id = def->id;
    m_priority = def->priority;
    m_def = def;
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00248050
// callcheck: setDef (inline in the ROM)

UiWidget::UiWidget(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg)
    : m_seq(seq), m_data(data), m_id(-1), m_priority(-1), m_def(nullptr), m_layout(layout), m_arg(arg)
{
    setDef(def);
}

// ──────────────────────────────────────────────────────────────────────
// Sets the frame of the chosen anims' playing tracks: a UiAnimSelect group
// of keyed anims, or the keyed (or empty) anim playing object which.
// FS2 0x002480a0

void UiActor::setAnimFrame(int frame, int which)
{
    for (u_int i = 0; i < m_animCount; i++) {
        UiAnim *anim = m_anims[i];
        bool chosen;
        switch (which) {
        case UiAnimSelect_KeyedC:
            chosen = anim->isKind(UiTrackKind_KeyedC, UiTrackKind_KeyedC2);
            break;
        case UiAnimSelect_KeyedB:
            chosen = anim->isKind(UiTrackKind_KeyedB, UiTrackKind_KeyedB2);
            break;
        case UiAnimSelect_KeyedA:
            chosen = anim->isKind(UiTrackKind_KeyedA, UiTrackKind_KeyedA2);
            break;
        case UiAnimSelect_Keyed:
            chosen = anim->isKeyed();
            break;
        case UiAnimSelect_None:
            return;
        default:
            chosen = !anim->isUnkeyed() && which == anim->m_objIndex;
            break;
        }
        if (chosen)
            anim->setFrame(frame);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Play flags for the playing tracks of the chosen anims (as setAnimFrame
// chooses); stopped tracks stay stopped.
// FS2 0x00248410
// callcheck: setPlay

void UiActor::setAnimPlay(u_int flags, int which)
{
    for (u_int i = 0; i < m_animCount; i++) {
        UiAnim *anim = m_anims[i];
        bool chosen;
        switch (which) {
        case UiAnimSelect_KeyedC:
            chosen = anim->isKind(UiTrackKind_KeyedC, UiTrackKind_KeyedC2);
            break;
        case UiAnimSelect_KeyedB:
            chosen = anim->isKind(UiTrackKind_KeyedB, UiTrackKind_KeyedB2);
            break;
        case UiAnimSelect_KeyedA:
            chosen = anim->isKind(UiTrackKind_KeyedA, UiTrackKind_KeyedA2);
            break;
        case UiAnimSelect_Keyed:
            chosen = anim->isKeyed();
            break;
        case UiAnimSelect_None:
            return;
        default:
            chosen = !anim->isUnkeyed() && which == anim->m_objIndex;
            break;
        }
        if (!chosen)
            continue;
        for (u_int j = 0; j < anim->m_trackCount; j++) {
            UiTrack *track = anim->m_tracks[j];
            if (track->m_play != UiTrackPlay_Stopped)
                track->setPlay(flags);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// An anim set entry's start: play flags for every track of the chosen
// anims. which -1 stops every anim (UiPlay_Stop) or plays every unkeyed
// one forward; an object index picks its anim whatever its kind.
// FS2 0x00248920
// callcheck: setPlay

void UiActor::startAnim(u_int flags, int which)
{
    for (u_int i = 0; i < m_animCount; i++) {
        UiAnim *anim = m_anims[i];
        bool chosen;
        u_int play = flags;
        switch (which) {
        case UiAnimSelect_KeyedC:
            chosen = anim->isKind(UiTrackKind_KeyedC, UiTrackKind_KeyedC2);
            break;
        case UiAnimSelect_KeyedB:
            chosen = anim->isKind(UiTrackKind_KeyedB, UiTrackKind_KeyedB2);
            break;
        case UiAnimSelect_KeyedA:
            chosen = anim->isKind(UiTrackKind_KeyedA, UiTrackKind_KeyedA2);
            break;
        case UiAnimSelect_Keyed:
            chosen = anim->isKeyed();
            break;
        case UiAnimSelect_None:
            if (flags & UiPlay_Stop) {
                chosen = true;
                play = UiPlay_Stop;
            } else {
                chosen = anim->isUnkeyed();
                play = UiPlay_Forward;
            }
            break;
        default:
            chosen = which == anim->m_objIndex;
            break;
        }
        if (!chosen)
            continue;
        for (u_int j = 0; j < anim->m_trackCount; j++)
            anim->m_tracks[j]->setPlay(play);
    }
}

// ──────────────────────────────────────────────────────────────────────
// Rebuilds the actor from a definition: an anim per entry, playing the
// entry's object, its tracks looping when the entry says so.
// FS2 0x00248e80
// callcheck: free, operator_delete, ~UiTrack, alloc, operator_new (clear inline; the delete- and new-expressions)

void UiActor::init(S2dActorDef *def, int id, int priority)
{
    clear();
    m_def = def;
    m_id = id;
    m_priority = priority;
    m_animCount = def->animCount;
    m_anims = (UiAnim **)gGlobalHeap.alloc(m_animCount * sizeof(UiAnim *), 16);
    for (u_int i = 0; i < m_animCount; i++) {
        // A bounds-checked element access in the ROM (null past the end).
        const S2dActorAnim *entry = (int)i < (int)m_def->animCount ? &m_def->anims[i] : nullptr;
        m_anims[i] = new UiAnim;
        m_anims[i]->init(m_data, m_data->objects[entry->object], entry->object);
        int loop = entry->loop;
        UiAnim *anim = m_anims[i];
        for (u_int j = 0; j < anim->m_trackCount; j++)
            anim->m_tracks[j]->m_loop = loop != 0;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The first anchor a track of the anim has (the first vertex of its first
// shape), or null.
// FS2 0x00249130

const float *UiAnim::getAnchor()
{
    for (u_int i = 0; i < m_trackCount; i++) {
        const float *anchor = m_tracks[i]->getAnchor();
        if (anchor != nullptr)
            return anchor;
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// The frame of every playing track: clamped to the record's length, or
// wrapped for a looping track. The clamped / wrapped value carries on to
// the next track, as in the ROM.
// FS2 0x002491c0

void UiAnim::setFrame(int frame)
{
    for (u_int i = 0; i < m_trackCount; i++) {
        UiTrack *track = m_tracks[i];
        if (track->m_play == UiTrackPlay_Stopped)
            continue;
        int length = track->m_rec->length;
        if (!track->m_loop) {
            if (length < frame)
                frame = length;
        } else if (length == 0) {
            frame = 0;
        } else {
            frame = frame % length;
        }
        track->m_frame = frame;
    }
}

// ──────────────────────────────────────────────────────────────────────
// The anim's share of an actor's draw state: the colour kinds reset the
// colour and offset (and mark them for the sequence base or the layout);
// the others let each playing track apply itself, the KeyedC kinds to the
// offset colour, the rest to the colour.
// FS2 0x00249260

void UiAnim::apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiVec4 *offset, UiDrawState *state)
{
    if (isKind(UiTrackKind_BaseColor)) {
        for (u_int i = 0; i < m_trackCount; i++) {
            if (m_tracks[i]->m_play != UiTrackPlay_Stopped) {
                color->x = color->y = color->z = color->w = 1.0f;
                offset->x = offset->y = offset->z = offset->w = 0.0f;
                state->setBase = 1;
            }
        }
    } else if (isKind(UiTrackKind_LayoutColor)) {
        for (u_int i = 0; i < m_trackCount; i++) {
            if (m_tracks[i]->m_play != UiTrackPlay_Stopped) {
                color->x = color->y = color->z = color->w = 1.0f;
                offset->x = offset->y = offset->z = offset->w = 0.0f;
                state->setLayout = 1;
            }
        }
    } else if (isKind(UiTrackKind_KeyedC, UiTrackKind_KeyedC2)) {
        for (u_int i = 0; i < m_trackCount; i++) {
            UiTrack *track = m_tracks[i];
            if (track->m_play != UiTrackPlay_Stopped)
                track->apply(xform, parent, offset, state);
        }
    } else {
        for (u_int i = 0; i < m_trackCount; i++) {
            UiTrack *track = m_tracks[i];
            if (track->m_play != UiTrackPlay_Stopped)
                track->apply(xform, parent, color, state);
        }
    }
}

// ──────────────────────────────────────────────────────────────────────
// FS2 0x00249540

void UiTrack::apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state)
{
}
