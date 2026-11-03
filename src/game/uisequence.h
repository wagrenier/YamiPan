/* ==========================================================================
 *  game/uisequence.h
 *
 *  A running sequence of a 2D layout (game/uisequence.cpp,
 *  0x246230..0x249550): the actors its anim sets brought in, each playing
 *  the anims (S2D objects) of its actor definition, each anim a list of
 *  tracks, and the widgets its script created. advance() steps the tracks
 *  and the widgets; draw() sorts both by priority and draws them merged:
 *  before each actor the widgets at or below its priority.
 *
 *  An actor's anims first apply themselves to the actor's matrix, colour
 *  and offset colour (UiAnim::apply: the tracks' apply slot), then the
 *  drawer gets them and each playing track draws (the draw slot); an actor
 *  with a non-zero offset colour is drawn once more in the drawer's
 *  additive pass.
 *
 *  The track classes (the 40 Vt_00316560 subclasses the factory
 *  UiAnim::init builds: game/uitrack.cpp) and the widget classes (Vt_00316580
 *  subclasses, UiWidget::create 0x25e260) are other TUs; their base classes
 *  and what this file calls of them are declared here. The names are the
 *  port's (no source string in the range).
 * ======================================================================== */
#ifndef _GAME_UISEQUENCE_H
#define _GAME_UISEQUENCE_H

#include <stddef.h>

#include "game/heap.h"
#include "game/s2ddata.h"
#include "game/uidrawer.h"
#include "sdk/scetypes.h"

class UiLayout;                 // game/uilayout.h
class UiSequence;

// S2dTrackRec::kind: the record kinds UiAnim::init builds a track class for
// (game/uitrack.h has the classes and their records). 9..14 are the keyed
// kinds (a length in frames that UiTrack::step plays through). Kind 0x20
// and kinds past 0x2b get no track in the ROM.
enum UiTrackKind
{
    UiTrackKind_LineStrip = 0x00,           // UiLineStripTrack: vertex colours
    UiTrackKind_Sprite = 0x01,              // UiSpriteTrack
    UiTrackKind_Triangles = 0x02,           // UiTrianglesTrack
    UiTrackKind_Text = 0x03,                // UiTextTrack
    UiTrackKind_Scissor = 0x04,             // UiScissorTrack
    UiTrackKind_TexSprite = 0x05,           // UiTexSpriteTrack
    UiTrackKind_SheetSprite = 0x06,         // UiSheetSpriteTrack
    UiTrackKind_TexTriangles = 0x07,        // UiTexTrianglesTrack
    UiTrackKind_TexStrip = 0x08,            // UiTexStripTrack
    UiTrackKind_KeyedFirst = 0x09,
    UiTrackKind_KeyedA = 0x09,              // UiMatrixFramesTrack; UiAnimSelect_KeyedA
    UiTrackKind_KeyedB = 0x0a,              // UiColorFramesTrack; UiAnimSelect_KeyedB
    UiTrackKind_KeyedC = 0x0b,              // UiOffsetFramesTrack; UiAnimSelect_KeyedC; applies to the offset colour
    UiTrackKind_KeyedA2 = 0x0c,             // UiMatrixKeysTrack
    UiTrackKind_KeyedB2 = 0x0d,             // UiColorKeysTrack
    UiTrackKind_KeyedC2 = 0x0e,             // UiOffsetKeysTrack; applies to the offset colour
    UiTrackKind_KeyedLast = 0x0e,
    UiTrackKind_Preset0 = 0x0f,             // UiPresetTrack<0> .. 0x14 UiPresetTrack<5>
    UiTrackKind_Preset5 = 0x14,
    UiTrackKind_Pin = 0x15,                 // UiPinTrack (also 0x17)
    UiTrackKind_PinRot = 0x16,              // UiPinRotTrack (also 0x18)
    UiTrackKind_Pin2 = 0x17,
    UiTrackKind_PinRot2 = 0x18,
    UiTrackKind_Rotate = 0x19,              // UiRotateTrack
    UiTrackKind_RotateTo = 0x1a,            // UiRotateToTrack
    UiTrackKind_AddBlend = 0x1b,            // UiAddBlendTrack
    UiTrackKind_BaseColor = 0x1c,           // UiBaseColorTrack. Playing: the actor's colour / offset
                                            // restart at 1 / 0 and become the base of the sequence's
                                            // next actors
    UiTrackKind_LineStripFlat = 0x1d,       // UiLineStripFlatTrack: one colour
    UiTrackKind_SpriteFlat = 0x1e,          // UiSpriteFlatTrack
    UiTrackKind_TrianglesFlat = 0x1f,       // UiTrianglesFlatTrack
    UiTrackKind_TexSpriteFlat = 0x21,       // UiTexSpriteFlatTrack
    UiTrackKind_SheetSpriteFlat = 0x22,     // UiSheetSpriteFlatTrack
    UiTrackKind_TexTrianglesFlat = 0x23,    // UiTexTrianglesFlatTrack
    UiTrackKind_TexStripFlat = 0x24,        // UiTexStripFlatTrack
    UiTrackKind_LayoutColor = 0x25,         // UiLayoutColorTrack. Playing: as BaseColor, and they
                                            // become the layout's
    UiTrackKind_DrawerFlag = 0x26,          // UiDrawerFlagTrack
    UiTrackKind_TexTrianglesRepeat = 0x27,  // UiTexTrianglesRepeatTrack
    UiTrackKind_Target = 0x28,              // UiTargetTrack (alt 0)
    UiTrackKind_Target2 = 0x29,             // UiTargetTrack (alt 1)
    UiTrackKind_DestAlphaBlend = 0x2a,      // UiDestAlphaBlendTrack
    UiTrackKind_Offset = 0x2b,              // UiOffsetTrack
};

// The anim an anim set entry (S2dAnimSetEntry::anim) or a script command
// names: an object index, or one of these groups.
enum UiAnimSelect
{
    UiAnimSelect_None = -1,     // setAnimFrame / setAnimPlay: nothing; startAnim: every
                                // unkeyed anim (or every anim, to stop it)
    UiAnimSelect_Keyed = -2,    // the keyed anims
    UiAnimSelect_KeyedA = -3,   // the keyed anims of kind 9 / 12
    UiAnimSelect_KeyedB = -4,   // kind 10 / 13
    UiAnimSelect_KeyedC = -5,   // kind 11 / 14
};

// UiTrack::m_play.
enum UiTrackPlay
{
    UiTrackPlay_Stopped = 0,    // not stepped, not drawn
    UiTrackPlay_Forward = 1,
    UiTrackPlay_Backward = 2,
    UiTrackPlay_Paused = 3,
};

// UiDrawState::coordMode: what an actor's anims ask of the drawer.
enum UiCoordMode
{
    UiCoord_Keep = 0,
    UiCoord_Pixel = 1,          // UiDrawer::setPixelMode(1), no translation
    UiCoord_Screen = 2,         // UiDrawer::setPixelMode(0), no translation
};

// What an actor's anims tell UiSequence::draw besides the matrix and
// colours (8 bytes on the EE stack).
struct UiDrawState
{
    int    coordMode;           // +0: UiCoordMode
    u_char setBase;             // +4: UiTrackKind_BaseColor played
    u_char setLayout;           // +5: UiTrackKind_LayoutColor played
    u_char drawerFlag;          // +6: UiDrawer::m_fixedColor (kind 0x26): the drawer colour alone
};
static_assert(sizeof(UiDrawState) == 0x8, "UiDrawState: EE 0x8");
static_assert(offsetof(UiDrawState, setBase) == 0x4, "UiDrawState::setBase");

// A track: one record of an object, played by frame. The base of the 40
// track classes (Vt_00316560, vptr at +0x10; slots after {type_info, 0}).
// layout: host -- EE 0x14: +0 record, +4 play, +8 frame, +0xc loop, +0x10 vptr.
class UiTrack
{
public:
    // Inline in the ROM (UiAnim::init, each class).
    UiTrack(S2dTrackRec *rec)
        : m_rec(rec), m_play(UiTrackPlay_Stopped), m_frame(0), m_loop(0)
    {
    }
    virtual ~UiTrack();                                                  // FS2 0x00247d10: deleting (obj, 1)
    virtual void draw(UiDrawer *drawer);                                 // FS2 0x00243350
    // Changes the actor's matrices and colour (or offset colour) for the
    // frame.
    virtual void apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state); // FS2 0x00249540
    // The first vertex (x, y) of the record (the shape kinds), or null.
    // UiWidget::create places widgets there.
    virtual const float *getAnchor();                                    // FS2 0x00243360

    bool isDone();                                                       // FS2 0x0024d2c0 (game/uitrack.cpp)
    void setPlay(u_int flags);                                           // FS2 0x0024d360: UiPlayFlag
    void step();                                                         // FS2 0x0024d200

    S2dTrackRec *m_rec;         // FS2 +0x00
    int          m_play;        // FS2 +0x04: UiTrackPlay
    int          m_frame;       // FS2 +0x08
    u_char       m_loop;        // FS2 +0x0c: wrap at the ends instead of clamping
};

// An object being played: its tracks (0x10 bytes).
// layout: host
class UiAnim
{
public:
    // Inline in the ROM (UiActor::init).
    UiAnim()
        : m_obj(nullptr), m_objIndex(-1), m_trackCount(0), m_tracks(nullptr)
    {
    }
    // Inline in the ROM (UiActor::clear).
    ~UiAnim()
    {
        clear();
    }

    // Builds a track per record of obj (the track factory).
    void init(S2dData *data, S2dObject *obj, int objIndex);              // FS2 0x00249550 (game/uitrack.cpp)
    const float *getAnchor();                                            // FS2 0x00249130
    UiTrack *findTrack(int kind);                                        // FS2 0x0024d1a0 (game/uitrack.cpp): null none
    void setFrame(int frame);                                            // FS2 0x002491c0
    void apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiVec4 *offset, UiDrawState *state); // FS2 0x00249260

    // Deletes the tracks. Inline in the ROM (UiActor::clear and the head
    // of init).
    void clear()
    {
        m_obj = nullptr;
        m_objIndex = -1;
        for (u_int i = 0; i < m_trackCount; i++) {
            if (m_tracks[i] != nullptr)
                delete m_tracks[i];
        }
        if (m_tracks != nullptr)
            gGlobalHeap.free(m_tracks);
        m_trackCount = 0;
        m_tracks = nullptr;
    }

    // The kind of the first track (the anim's kind), when it has tracks.
    // Inline in the ROM, everywhere.
    bool isKind(int kind) const
    {
        return m_trackCount != 0 && m_tracks[0]->m_rec->kind == kind;
    }
    bool isKind(int kind, int kind2) const
    {
        return m_trackCount != 0 && (m_tracks[0]->m_rec->kind == kind || m_tracks[0]->m_rec->kind == kind2);
    }
    bool isKeyed() const
    {
        return m_trackCount != 0
            && (u_int)(m_tracks[0]->m_rec->kind - UiTrackKind_KeyedFirst) <= UiTrackKind_KeyedLast - UiTrackKind_KeyedFirst;
    }
    bool isUnkeyed() const
    {
        return m_trackCount != 0
            && (u_int)(m_tracks[0]->m_rec->kind - UiTrackKind_KeyedFirst) > UiTrackKind_KeyedLast - UiTrackKind_KeyedFirst;
    }

    S2dObject *m_obj;           // FS2 +0x00
    int        m_objIndex;      // FS2 +0x04: S2dData::objects index; -1 none
    u_int      m_trackCount;    // FS2 +0x08
    UiTrack  **m_tracks;        // FS2 +0x0c: gGlobalHeap
};

// An animated object of a sequence: the anims of one actor definition
// (0x18 bytes).
// layout: host
class UiActor
{
public:
    // Inline in the ROM (UiSequence::addAnimSet).
    UiActor(S2dData *data)
        : m_data(data), m_def(nullptr), m_id(-1), m_animCount(0), m_anims(nullptr), m_priority(0)
    {
    }
    // Inline in the ROM (UiSequence::removeActor, twice).
    ~UiActor()
    {
        clear();
    }

    void init(S2dActorDef *def, int id, int priority);                   // FS2 0x00248e80
    void setAnimFrame(int frame, int which);                             // FS2 0x002480a0: UiAnimSelect or object
    void setAnimPlay(u_int flags, int which);                            // FS2 0x00248410: UiPlayFlag; skips stopped tracks
    void startAnim(u_int flags, int which);                              // FS2 0x00248920: every track of the chosen anims

    // Deletes the anims. Inline in the ROM (here and at the head of init).
    void clear()
    {
        m_def = nullptr;
        m_id = -1;
        for (u_int i = 0; i < m_animCount; i++) {
            if (m_anims[i] != nullptr)
                delete m_anims[i];
        }
        if (m_anims != nullptr)
            gGlobalHeap.free(m_anims);
        m_animCount = 0;
        m_anims = nullptr;
    }

    // The anim playing object objIndex, or null. Inline in the ROM.
    UiAnim *findAnim(int objIndex)
    {
        for (u_int i = 0; i < m_animCount; i++) {
            if (objIndex == m_anims[i]->m_objIndex)
                return m_anims[i];
        }
        return nullptr;
    }

    // Every track of every anim done. Inline in the ROM
    // (UiLayout::testCondition).
    bool isDone()
    {
        for (u_int i = 0; i < m_animCount; i++) {
            UiAnim *anim = m_anims[i];
            for (u_int j = 0; j < anim->m_trackCount; j++) {
                if (!anim->m_tracks[j]->isDone())
                    return false;
            }
        }
        return true;
    }

    // Every playing track draws. Inline in the ROM (UiSequence::draw,
    // twice).
    void drawTracks(UiDrawer *drawer)
    {
        for (u_int i = 0; i < m_animCount; i++) {
            UiAnim *anim = m_anims[i];
            for (u_int j = 0; j < anim->m_trackCount; j++) {
                UiTrack *track = anim->m_tracks[j];
                if (track->m_play != UiTrackPlay_Stopped)
                    track->draw(drawer);
            }
        }
    }

    S2dData     *m_data;        // FS2 +0x00
    S2dActorDef *m_def;         // FS2 +0x04
    int          m_id;          // FS2 +0x08: S2dData::actorDefs index; -1 none
    u_int        m_animCount;   // FS2 +0x0c
    UiAnim     **m_anims;       // FS2 +0x10: gGlobalHeap
    int          m_priority;    // FS2 +0x14: draw order, lowest first
};

// A sequence's widget: the base of the widget classes (Vt_00316580, vptr
// at +0x1c; UiWidget::create builds them from S2dWidgetDef::type).
// layout: host -- EE 0x20 for the base.
class UiWidget
{
public:
    UiWidget(UiSequence *seq, UiLayout *layout, S2dData *data, S2dWidgetDef *def, int arg); // FS2 0x00248050
    virtual ~UiWidget();                                                 // FS2 0x00247820: deleting (obj, 1)
    virtual void draw(UiDrawer *drawer);                                 // FS2 0x00246da0
    virtual void advance();                                              // FS2 0x00247060
    // create on an existing widget: after the new arg is stored.
    virtual void refresh(bool argChanged);                               // FS2 0x0024d400
    virtual int  getStatus();                                            // FS2 0x0024d410

    static void create(UiSequence *seq, S2dWidgetDef *def, UiLayout *layout, int arg); // FS2 0x0025e260

    void     setDef(S2dWidgetDef *def);                                  // FS2 0x00248030
    UiAnim  *findAnim(int nameString);                                   // FS2 0x00247f20: in its sequence
    UiTrack *findTrack(int nameString, int kind);                        // FS2 0x00247db0: of that anim

    UiSequence   *m_seq;        // FS2 +0x00
    S2dData      *m_data;       // FS2 +0x04
    int           m_id;         // FS2 +0x08: the def's
    int           m_priority;   // FS2 +0x0c: the def's
    S2dWidgetDef *m_def;        // FS2 +0x10
    UiLayout     *m_layout;     // FS2 +0x14
    int           m_arg;        // FS2 +0x18: create's arg
};

// A running sequence (0x70 bytes).
// layout: host
class UiSequence
{
public:
    UiSequence(S2dData *data, UiLayout *layout, int value);              // FS2 0x00247d60
    // Inline in the ROM (UiLayout::removeSequence).
    ~UiSequence()
    {
        removeActor(-1);
        removeWidget(-1);
    }

    void advance();                                                      // FS2 0x00246db0
    void draw(UiDrawer *drawer);                                         // FS2 0x00246400
    void addAnimSet(S2dAnimSet *set);                                    // FS2 0x00247330
    int  removeActor(int id);                                            // FS2 0x002479b0: -1 all; answers the slot, -1
    int  removeWidget(int id);                                           // FS2 0x002476c0: -1 all; answers the slot, -1
    void setAnimSetFrame(S2dAnimSet *set, int frame);                    // FS2 0x002471d0
    void setAnimSetPlay(S2dAnimSet *set, u_int flags);                   // FS2 0x00247070

    UiAnim    *findAnim(int objIndex);                                   // FS2 0x00246340
    UiAnim    *findAnimByName(int nameString);                           // FS2 0x00246230
    UiWidget  *findWidget(int id);                                       // FS2 0x00247930
    UiWidget  *findWidgetByName(const char *name);                       // FS2 0x00247870
    // The next free widget slot, the array grown when full; the caller
    // stores the widget there and counts it.
    UiWidget **reserveWidgetSlot();                                      // FS2 0x002475f0

    // The actor with that id, or null. Inline in the ROM (it searches the
    // whole capacity, skipping empty entries).
    UiActor *findActor(int id)
    {
        if (m_actors == nullptr || id < 0)
            return nullptr;
        for (int i = 0; i < m_actorCapacity; i++) {
            if (m_actors[i] != nullptr && m_actors[i]->m_id == id)
                return m_actors[i];
        }
        return nullptr;
    }

    // Every actor of the set this sequence waits on (entries without
    // UiPlay_Stop) is here and done. Inline in the ROM
    // (UiLayout::testCondition, twice).
    bool isAnimSetDone(const S2dAnimSet *set)
    {
        for (u_int i = 0; i < set->count; i++) {
            const S2dAnimSetEntry &entry = set->entries[i];
            if (entry.flags & UiPlay_Stop)
                continue;
            UiActor *actor = findActor(entry.actorId);
            if (actor == nullptr || !actor->isDone())
                return false;
        }
        return true;
    }

    // Moves the cursor n commands on: the command there, or null past the
    // end (or when there is no cursor). Inline in the ROM.
    S2dCmd *step(int n)
    {
        if (m_cursor == nullptr)
            return nullptr;
        m_cursor += n;
        return m_cursor < m_cmdEnd ? m_cursor : nullptr;
    }

    S2dSeqDef *m_def;           // FS2 +0x00
    int        m_id;            // FS2 +0x04: its index in S2dData::seqs
    int        m_priority;      // FS2 +0x08: m_def->priority
    S2dCmd    *m_cmdBegin;      // FS2 +0x0c
    S2dCmd    *m_cmdEnd;        // FS2 +0x10
    S2dCmd    *m_cursor;        // FS2 +0x14: where the next frame pass starts
    S2dData   *m_data;          // FS2 +0x18
    int        m_actorCapacity; // FS2 +0x1c
    int        m_actorCount;    // FS2 +0x20
    UiActor  **m_actors;        // FS2 +0x24: gGlobalHeap, grown by 16
    int        m_widgetCapacity; // FS2 +0x28
    int        m_widgetCount;   // FS2 +0x2c
    UiWidget **m_widgets;       // FS2 +0x30: gGlobalHeap, grown by 16
    int        m_pending;       // FS2 +0x34: UiLayout::update's not-yet-run mark
    UiMatrix   m_xform;         // FS2 +0x38: identity at each draw; each actor's starts from it
    UiMatrix   m_parentXform;   // FS2 +0x50: likewise, concatenated after the actor's
    UiLayout  *m_layout;        // FS2 +0x68
    int        m_value;         // FS2 +0x6c: the start value; script-readable

private:
    // Priority order, lowest first (exchange sorts, as UiLayout's).
    // Inline in the ROM (draw, advance); defined in game/uisequence.cpp,
    // the only user.
    void sortActors();
    void sortWidgets();
    // A new empty actor at the end of the array, grown when full. Inline in
    // the ROM (addAnimSet).
    UiActor *addActor();
};

#endif /* _GAME_UISEQUENCE_H */
