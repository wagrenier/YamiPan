// game/uitrack.cpp -- the track factory UiAnim::init, the track classes of
// the 2D layouts (one per S2D record kind) and the UiTrack frame stepping
// (game/uitrack.h).

#include "game/uitrack.h"

#include "game/gamehooks.h"
#include "game/gfxtarget.h"
#include "game/heap.h"
#include "game/s2ddata.h"
#include "game/uidrawer.h"
#include "game/uisequence.h"
#include "sdk/scetypes.h"

// The PRIM values the shapes draw with: every one gouraud and blended,
// the textured ones with texel coordinates.
constexpr u_long kUiPrimShape = UiPrim_Gouraud | UiPrim_Blend;
constexpr u_long kUiPrimTexShape = UiPrim_UV | UiPrim_Blend | UiPrim_Textured | UiPrim_Gouraud;

// The fourth argument of the textured strip meshes (0x2fac70 / 0x2fac80,
// the same values).
static const UiVec4 kStripUvRectGouraud = { 0.5f, 0.0f, 0.5f, 1.0f };
static const UiVec4 kStripUvRectFlat = { 0.5f, 0.0f, 0.5f, 1.0f };

// ──────────────────────────────────────────────────────────────────────
// Helpers, inline in the ROM.

// The keyed kinds play through a length (UiTrack::step, isDone).
static inline bool isKeyedKind(int kind)
{
    return (u_int)(kind - UiTrackKind_KeyedFirst) <= UiTrackKind_KeyedLast - UiTrackKind_KeyedFirst;
}

// The track's frame clamped to the record's length (every keyed apply).
static inline int clampedFrame(const UiTrack *track)
{
    int frame = track->m_frame;
    if (frame < 0)
        return 0;
    if (frame >= track->m_rec->length)
        return track->m_rec->length - 1;
    return frame;
}

// The keys around frame: the last key before it and the first after it,
// or both the key on it (or the first / last key outside the keys'
// range). A record without keys answers its first slot, as the ROM.
template <class Key>
static inline void findKeys(const Key *keys, int count, int frame, const Key **prev, const Key **next)
{
    const Key *before = &keys[0];
    for (int i = 0; i < count; i++) {
        if (frame < keys[i].frame) {
            *prev = before;
            *next = &keys[i];
            return;
        }
        if (frame == keys[i].frame) {
            *prev = *next = &keys[i];
            return;
        }
        before = &keys[i];
    }
    *prev = *next = before;
}

// The colour value of a colour-key record at the track's frame.
static inline UiVec4 colorKeyValue(const UiTrack *track)
{
    const S2dColorKeysRec *rec = (const S2dColorKeysRec *)track->m_rec;
    int frame = clampedFrame(track);
    const S2dColorKey *prev;
    const S2dColorKey *next;
    findKeys(rec->keys, rec->keyCount, frame, &prev, &next);
    UiVec4 v = next->value;
    if (prev->frame != next->frame) {
        float t = (float)(frame - prev->frame) / (float)(next->frame - prev->frame);
        v.x = (v.x - prev->value.x) * t + prev->value.x;
        v.y = (v.y - prev->value.y) * t + prev->value.y;
        v.z = (v.z - prev->value.z) * t + prev->value.z;
        v.w = (v.w - prev->value.w) * t + prev->value.w;
    }
    return v;
}

// The PRIM of a triangle record's mode, 0 for another mode (no PRIM is
// set then: the vertices go out with the last one).
static inline u_long triPrim(int mode, u_long flags)
{
    switch (mode) {
    case UiTriMode_Strip:
        return flags | UiPrim_TriStrip;
    case UiTriMode_Fan:
        return flags | UiPrim_TriFan;
    case UiTriMode_List:
        return flags | UiPrim_Triangle;
    default:
        return 0;
    }
}

// The S2D image index of the textured kinds (+0xc in all their records).
static inline int recImage(const S2dTrackRec *rec)
{
    return ((const S2dTexSpriteRec *)rec)->image;
}

// A textured shape's texture and image id (each case of UiAnim::init).
template <class Track>
static inline void loadTexture(Track *track, S2dData *data, int image)
{
    const void *src = data->getImage(image);
    track->m_texture = new GsTexture(src);
    track->m_imageName = data->getImageEntry(image)->name;
}

// The image of a text record: by S2D index, or (negative) by the name of
// an init-pack item in string -1 - index. key: the image's name, or the
// item's.
static inline const void *textImage(S2dData *data, int index, const char **key)
{
    if (index < 0) {
        const char *name = data->getString(-1 - index);
        *key = name;
        return getInitPackData(name);
    }
    const void *image = data->getImage(index);
    *key = data->getImageEntry(index)->name;
    return image;
}

// The rotate tracks' angle brought into [-pi, pi] (compared as doubles in
// the ROM: __extendsfdf2 and the soft-float compares 0x100250 / 0x100230).
static inline float wrapAngle(float angle)
{
    if (angle < -3.141592653589793)
        angle += 6.2831855f;
    if (angle > 3.141592653589793)
        angle -= 6.2831855f;
    return angle;
}

// A pinned matrix's origin moved from the actor to its parent (the pin
// applies; the parent was reset to identity before).
static inline void moveOriginToParent(UiMatrix *xform, UiMatrix *parent)
{
    parent->tx = xform->tx;
    parent->ty = xform->ty;
    xform->ty = 0.0f;
    xform->tx = 0.0f;
}

// The sprite's sheet image (both sheet-sprite draws, inline in the ROM).
static inline const void *sheetImage(const TimSheet *sheet, const TimSheetSprite *sprite)
{
    if (sprite == nullptr)
        return nullptr;
    return sheet->images.get()[sprite->image].image.get();
}

// Frees the text track's own copy (each setter and the destructor, inline
// in the ROM).
static inline void freeOwnedText(UiTextTrack *track)
{
    if (track->m_ownsText && track->m_text != nullptr) {
        delete[] track->m_text;
        track->m_text = nullptr;
        track->m_ownsText = 0;
    }
}

// A wide-string copy on the heap (setText / setTextAt).
static inline u_short *copyText(const u_short *text)
{
    u_short *copy = new u_short[wstrlen(text) + 1];
    wstrcpy(copy, text);
    return copy;
}

// ──────────────────────────────────────────────────────────────────────
// The track factory: one track per record of the object, by its kind.
// PC: a kind without a class (0x20, past 0x2b) leaves its slot unwritten
// in the ROM (heap garbage); the port stores null.
// FS2 0x00249550
// callcheck: operator_new, alloc, free, GsTexture, getImage, getImageEntry, getString, getInitPackData, UiFont (the inline clear, the per-kind ctors, the helpers loadTexture / textImage)

void UiAnim::init(S2dData *data, S2dObject *obj, int objIndex)
{
    clear();
    m_obj = obj;
    m_objIndex = objIndex;
    m_trackCount = obj->trackCount;
    m_tracks = (UiTrack **)gGlobalHeap.alloc(m_trackCount * sizeof(UiTrack *), 16);

    for (u_int i = 0; i < m_trackCount; i++) {
        S2dTrackRec *rec = i < m_obj->trackCount ? m_obj->tracks[i] : nullptr;
        UiTrack *track = nullptr;

        switch (rec->kind) {
        case UiTrackKind_LineStrip:
            track = new UiLineStripTrack(rec);
            break;
        case UiTrackKind_Sprite:
            track = new UiSpriteTrack(rec);
            break;
        case UiTrackKind_Triangles:
            track = new UiTrianglesTrack(rec);
            break;
        case UiTrackKind_Text: {
            const S2dTextRec *textRec = (const S2dTextRec *)rec;
            const char *fontKey;
            const char *glyphKey;
            const void *fontImage = textImage(data, textRec->fontImage, &fontKey);
            const void *glyphs = textImage(data, textRec->glyphImage, &glyphKey);
            UiTextTrack *text = new UiTextTrack(rec, glyphs, fontImage);
            text->m_fontKey = fontKey;
            text->m_glyphKey = glyphKey;
            track = text;
            break;
        }
        case UiTrackKind_Scissor:
            track = new UiScissorTrack(rec);
            break;
        case UiTrackKind_TexSprite: {
            UiTexSpriteTrack *tex = new UiTexSpriteTrack(rec);
            loadTexture(tex, data, recImage(rec));
            track = tex;
            break;
        }
        case UiTrackKind_SheetSprite: {
            UiSheetSpriteTrack *sheet = new UiSheetSpriteTrack(rec);
            sheet->m_sheet = (TimSheet *)data->getImage(recImage(rec));
            timSheetRelocate(sheet->m_sheet);
            sheet->m_imageName = data->getImageEntry(recImage(rec))->name;
            track = sheet;
            break;
        }
        case UiTrackKind_TexTriangles: {
            UiTexTrianglesTrack *tex = new UiTexTrianglesTrack(rec);
            loadTexture(tex, data, recImage(rec));
            track = tex;
            break;
        }
        case UiTrackKind_TexStrip: {
            UiTexStripTrack *tex = new UiTexStripTrack(rec);
            loadTexture(tex, data, recImage(rec));
            track = tex;
            break;
        }
        case UiTrackKind_KeyedA:
            track = new UiMatrixFramesTrack(rec);
            break;
        case UiTrackKind_KeyedB:
            track = new UiColorFramesTrack(rec);
            break;
        case UiTrackKind_KeyedC:
            track = new UiOffsetFramesTrack(rec);
            break;
        case UiTrackKind_KeyedA2:
            track = new UiMatrixKeysTrack(rec);
            break;
        case UiTrackKind_KeyedB2:
            track = new UiColorKeysTrack(rec);
            break;
        case UiTrackKind_KeyedC2:
            track = new UiOffsetKeysTrack(rec);
            break;
        case UiTrackKind_Preset0:
            track = new UiPresetTrack<0>(rec);
            break;
        case UiTrackKind_Preset0 + 1:
            track = new UiPresetTrack<1>(rec);
            break;
        case UiTrackKind_Preset0 + 2:
            track = new UiPresetTrack<2>(rec);
            break;
        case UiTrackKind_Preset0 + 3:
            track = new UiPresetTrack<3>(rec);
            break;
        case UiTrackKind_Preset0 + 4:
            track = new UiPresetTrack<4>(rec);
            break;
        case UiTrackKind_Preset5:
            track = new UiPresetTrack<5>(rec);
            break;
        case UiTrackKind_Pin:
        case UiTrackKind_Pin2:
            track = new UiPinTrack(rec);
            break;
        case UiTrackKind_PinRot:
        case UiTrackKind_PinRot2: {
            UiPinRotTrack *pin = new UiPinRotTrack(rec);
            pin->bindTarget(((S2dPinRec *)rec)->target);
            track = pin;
            break;
        }
        case UiTrackKind_Rotate:
            track = new UiRotateTrack(rec);
            break;
        case UiTrackKind_RotateTo: {
            UiRotateToTrack *rotate = new UiRotateToTrack(rec);
            rotate->bindTarget(((S2dRotateRec *)rec)->target);
            track = rotate;
            break;
        }
        case UiTrackKind_AddBlend:
            track = new UiAddBlendTrack(rec);
            break;
        case UiTrackKind_BaseColor:
            track = new UiBaseColorTrack(rec);
            break;
        case UiTrackKind_LineStripFlat:
            track = new UiLineStripFlatTrack(rec);
            break;
        case UiTrackKind_SpriteFlat:
            track = new UiSpriteFlatTrack(rec);
            break;
        case UiTrackKind_TrianglesFlat:
            track = new UiTrianglesFlatTrack(rec);
            break;
        case UiTrackKind_TexSpriteFlat: {
            UiTexSpriteFlatTrack *tex = new UiTexSpriteFlatTrack(rec);
            loadTexture(tex, data, recImage(rec));
            track = tex;
            break;
        }
        case UiTrackKind_SheetSpriteFlat: {
            UiSheetSpriteFlatTrack *sheet = new UiSheetSpriteFlatTrack(rec);
            sheet->m_sheet = (TimSheet *)data->getImage(recImage(rec));
            timSheetRelocate(sheet->m_sheet);
            sheet->m_imageName = data->getImageEntry(recImage(rec))->name;
            track = sheet;
            break;
        }
        case UiTrackKind_TexTrianglesFlat: {
            UiTexTrianglesFlatTrack *tex = new UiTexTrianglesFlatTrack(rec);
            loadTexture(tex, data, recImage(rec));
            track = tex;
            break;
        }
        case UiTrackKind_TexStripFlat: {
            UiTexStripFlatTrack *tex = new UiTexStripFlatTrack(rec);
            loadTexture(tex, data, recImage(rec));
            track = tex;
            break;
        }
        case UiTrackKind_LayoutColor:
            track = new UiLayoutColorTrack(rec);
            break;
        case UiTrackKind_DrawerFlag:
            track = new UiDrawerFlagTrack(rec);
            break;
        case UiTrackKind_TexTrianglesRepeat: {
            UiTexTrianglesRepeatTrack *tex = new UiTexTrianglesRepeatTrack(rec);
            loadTexture(tex, data, recImage(rec));
            track = tex;
            break;
        }
        case UiTrackKind_Target:
            track = new UiTargetTrack(rec, 0);
            break;
        case UiTrackKind_Target2:
            track = new UiTargetTrack(rec, 1);
            break;
        case UiTrackKind_DestAlphaBlend:
            track = new UiDestAlphaBlendTrack(rec);
            break;
        case UiTrackKind_Offset:
            track = new UiOffsetTrack(rec);
            break;
        }
        m_tracks[i] = track;
    }
}

// ──────────────────────────────────────────────────────────────────────
// Kind 0x2b.
// FS2 0x0024a720

void UiOffsetTrack::apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state)
{
    xform->translate(m_offset);
}

// FS2 0x0024a730
// callcheck: operator_delete (the deleting destructor)

UiOffsetTrack::~UiOffsetTrack()
{
}

// ──────────────────────────────────────────────────────────────────────
// Kinds 0x28 / 0x29.
// FS2 0x0024a790

void UiTargetTrack::draw(UiDrawer *drawer)
{
    if (m_alt == 0)
        drawer->copyTarget(1);
    else
        drawer->copyTarget(0);
}

// FS2 0x0024a7d0
// callcheck: operator_delete (the deleting destructor)

UiTargetTrack::~UiTargetTrack()
{
}

// ──────────────────────────────────────────────────────────────────────
// Kind 0x2a.
// FS2 0x0024a830

void UiDestAlphaBlendTrack::draw(UiDrawer *drawer)
{
    drawer->setBlend(UiBlend_AddDestAlpha);
}

// FS2 0x0024a840
// callcheck: operator_delete (the deleting destructor)

UiDestAlphaBlendTrack::~UiDestAlphaBlendTrack()
{
}

// ──────────────────────────────────────────────────────────────────────
// Kind 0x1b.
// FS2 0x0024a8a0

void UiAddBlendTrack::draw(UiDrawer *drawer)
{
    drawer->setBlend(UiBlend_Add);
}

// FS2 0x0024a8b0
// callcheck: operator_delete (the deleting destructor)

UiAddBlendTrack::~UiAddBlendTrack()
{
}

// ──────────────────────────────────────────────────────────────────────
// Kinds 0xf..0x14, one instance each.
// FS2 0x0024a910 (5), 0x0024a980 (4), 0x0024a9f0 (3), 0x0024aa60 (2),
// 0x0024aad0 (1), 0x0024ab40 (0)

template <int Preset>
void UiPresetTrack<Preset>::draw(UiDrawer *drawer)
{
    drawer->setDrawPreset(Preset);
}

// FS2 0x0024a920 (5), 0x0024a990 (4), 0x0024aa00 (3), 0x0024aa70 (2),
// 0x0024aae0 (1), 0x0024ab50 (0)

template <int Preset>
UiPresetTrack<Preset>::~UiPresetTrack()
{
}

template class UiPresetTrack<5>;
template class UiPresetTrack<4>;
template class UiPresetTrack<3>;
template class UiPresetTrack<2>;
template class UiPresetTrack<1>;
template class UiPresetTrack<0>;

// ──────────────────────────────────────────────────────────────────────
// Kind 0x26.
// FS2 0x0024abb0

void UiDrawerFlagTrack::apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state)
{
    state->drawerFlag = 1;
}

// FS2 0x0024abc0
// callcheck: operator_delete (the deleting destructor)

UiDrawerFlagTrack::~UiDrawerFlagTrack()
{
}

// ──────────────────────────────────────────────────────────────────────
// Kind 0x25: the colour restarts at white (the offset is restarted by
// UiAnim::apply's caller).
// FS2 0x0024ac20

void UiLayoutColorTrack::apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state)
{
    color->x = 1.0f;
    color->y = 1.0f;
    color->z = 1.0f;
    color->w = 1.0f;
    state->setLayout = 1;
}

// FS2 0x0024ac40
// callcheck: operator_delete (the deleting destructor)

UiLayoutColorTrack::~UiLayoutColorTrack()
{
}

// ──────────────────────────────────────────────────────────────────────
// Kind 0x1c.
// FS2 0x0024aca0

void UiBaseColorTrack::apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state)
{
    color->x = 1.0f;
    color->y = 1.0f;
    color->z = 1.0f;
    color->w = 1.0f;
    state->setBase = 1;
}

// FS2 0x0024acc0
// callcheck: operator_delete (the deleting destructor)

UiBaseColorTrack::~UiBaseColorTrack()
{
}

// ──────────────────────────────────────────────────────────────────────
// Kind 0x1a: the actor is drawn in pixel coordinates turned to the
// combined matrix's angle plus its own and the target's; its own and the
// parent's matrices are dropped.
// FS2 0x0024ad20
// callcheck: __extendsfdf2, FUN_00100250, FUN_00100230 (wrapAngle)

void UiRotateToTrack::apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state)
{
    UiMatrix world;
    world.multiply(*parent, *xform);
    float angle = wrapAngle(world.angle() + (m_angle + m_targetAngle));
    xform->identity();
    xform->rotate(angle);
    parent->identity();
    state->coordMode = UiCoord_Pixel;
}

// FS2 0x0024ae50
// callcheck: operator_delete (the deleting destructor)

UiRotateToTrack::~UiRotateToTrack()
{
}

// ──────────────────────────────────────────────────────────────────────
// Kind 0x19.
// FS2 0x0024aeb0
// callcheck: __extendsfdf2, FUN_00100250, FUN_00100230 (wrapAngle)

void UiRotateTrack::apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state)
{
    UiMatrix world;
    world.multiply(*parent, *xform);
    float angle = wrapAngle(world.angle() + m_angle);
    xform->identity();
    xform->rotate(angle);
    parent->identity();
    state->coordMode = UiCoord_Pixel;
}

// FS2 0x0024afd0
// callcheck: operator_delete (the deleting destructor)

UiRotateTrack::~UiRotateTrack()
{
}

// ──────────────────────────────────────────────────────────────────────
// Kinds 0x16 / 0x18: the pin of kinds 0x15 / 0x17 after the target's
// move and turn.
// FS2 0x0024b030

void UiPinRotTrack::apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state)
{
    const S2dPinRec *rec = (const S2dPinRec *)m_rec;
    UiMatrix world;
    world.multiply(*parent, *xform);
    if (rec->mode == UiPinMode_Upright) {
        float origin[2];
        uiTransformPoint(origin, &world, m_pos, 0);
        xform->setScale(rec->scale[0], rec->scale[1]);
        parent->identity();
        parent->translate(origin);
    } else {
        UiMatrix target;
        target.identity();
        target.rotate(m_angle);
        target.translate(m_pos);
        world.multiply(*parent, target);
        UiMatrix pinned;
        pinned.copy(world);
        world.setScaleRotTrans(pinned, rec->scale);
        xform->copy(world);
        parent->identity();
        moveOriginToParent(xform, parent);
    }
}

// FS2 0x0024b150
// callcheck: operator_delete (the deleting destructor)

UiPinRotTrack::~UiPinRotTrack()
{
}

// ──────────────────────────────────────────────────────────────────────
// Kinds 0x15 / 0x17.
// FS2 0x0024b1b0

void UiPinTrack::apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state)
{
    const S2dPinRec *rec = (const S2dPinRec *)m_rec;
    UiMatrix world;
    world.multiply(*parent, *xform);
    if (rec->mode == UiPinMode_Upright) {
        float zero[2] = { 0.0f, 0.0f };
        float origin[2];
        uiTransformPoint(origin, &world, zero, 0);
        xform->setScale(rec->scale[0], rec->scale[1]);
        parent->identity();
        parent->translate(origin);
    } else {
        parent->identity();
        UiMatrix pinned;
        pinned.copy(world);
        world.setScaleRotTrans(pinned, rec->scale);
        xform->copy(world);
        moveOriginToParent(xform, parent);
    }
}

// FS2 0x0024b2a0
// callcheck: operator_delete (the deleting destructor)

UiPinTrack::~UiPinTrack()
{
}

// ──────────────────────────────────────────────────────────────────────
// Kind 0xe.
// FS2 0x0024b300

void UiOffsetKeysTrack::apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state)
{
    UiVec4 v = colorKeyValue(this);
    color->x += v.x;
    color->y += v.y;
    color->z += v.z;
    color->w += v.w;
}

// FS2 0x0024b460
// callcheck: operator_delete (the deleting destructor)

UiOffsetKeysTrack::~UiOffsetKeysTrack()
{
}

// ──────────────────────────────────────────────────────────────────────
// Kind 0xd.
// FS2 0x0024b4c0

void UiColorKeysTrack::apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state)
{
    UiVec4 v = colorKeyValue(this);
    color->x *= v.x;
    color->y *= v.y;
    color->z *= v.z;
    color->w *= v.w;
}

// FS2 0x0024b620
// callcheck: operator_delete (the deleting destructor)

UiColorKeysTrack::~UiColorKeysTrack()
{
}

// ──────────────────────────────────────────────────────────────────────
// Kind 0xc.
// FS2 0x0024b680

void UiMatrixKeysTrack::apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state)
{
    const S2dMatrixKeysRec *rec = (const S2dMatrixKeysRec *)m_rec;
    int frame = clampedFrame(this);
    const S2dMatrixKey *prev;
    const S2dMatrixKey *next;
    findKeys(rec->keys, rec->keyCount, frame, &prev, &next);
    UiMatrix m;
    if (prev->frame == next->frame)
        m.copy(next->value);
    else
        m.interpolate(prev->value, next->value, (float)(frame - prev->frame) / (float)(next->frame - prev->frame));
    xform->concat(m);
}

// FS2 0x0024b7c0
// callcheck: operator_delete (the deleting destructor)

UiMatrixKeysTrack::~UiMatrixKeysTrack()
{
}

// ──────────────────────────────────────────────────────────────────────
// Kind 0xb.
// FS2 0x0024b820

void UiOffsetFramesTrack::apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state)
{
    const UiVec4 &v = ((const S2dColorFramesRec *)m_rec)->frames[clampedFrame(this)];
    color->x += v.x;
    color->y += v.y;
    color->z += v.z;
    color->w += v.w;
}

// FS2 0x0024b8a0
// callcheck: operator_delete (the deleting destructor)

UiOffsetFramesTrack::~UiOffsetFramesTrack()
{
}

// ──────────────────────────────────────────────────────────────────────
// Kind 0xa.
// FS2 0x0024b900

void UiColorFramesTrack::apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state)
{
    const UiVec4 &v = ((const S2dColorFramesRec *)m_rec)->frames[clampedFrame(this)];
    color->x *= v.x;
    color->y *= v.y;
    color->z *= v.z;
    color->w *= v.w;
}

// FS2 0x0024b980
// callcheck: operator_delete (the deleting destructor)

UiColorFramesTrack::~UiColorFramesTrack()
{
}

// ──────────────────────────────────────────────────────────────────────
// Kind 9.
// FS2 0x0024b9e0

void UiMatrixFramesTrack::apply(UiMatrix *xform, UiMatrix *parent, UiVec4 *color, UiDrawState *state)
{
    xform->concat(((const S2dMatrixFramesRec *)m_rec)->frames[clampedFrame(this)]);
}

// FS2 0x0024ba40
// callcheck: operator_delete (the deleting destructor)

UiMatrixFramesTrack::~UiMatrixFramesTrack()
{
}

// ──────────────────────────────────────────────────────────────────────
// Kind 0x24.
// FS2 0x0024baa0

void UiTexStripFlatTrack::draw(UiDrawer *drawer)
{
    if (m_texture == nullptr)
        return;
    drawer->setTexture(m_texture);
    const S2dTexStripFlatRec *rec = (const S2dTexStripFlatRec *)m_rec;
    drawer->setPrimColor(kUiPrimTexShape | UiPrim_TriStrip, &rec->rgba);
    UiVec4 uvRect = kStripUvRectFlat;
    drawer->drawTexStripFlat(rec->width, rec->verts, rec->count, &uvRect);
}

// FS2 0x0024bb40
// callcheck: operator_delete (the deleting destructor)

UiTexStripFlatTrack::~UiTexStripFlatTrack()
{
    if (m_texture != nullptr)
        delete m_texture;
    m_texture = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Kind 8.
// FS2 0x0024bc10

void UiTexStripTrack::draw(UiDrawer *drawer)
{
    if (m_texture == nullptr)
        return;
    drawer->setTexture(m_texture);
    const S2dTexStripRec *rec = (const S2dTexStripRec *)m_rec;
    drawer->setPrim(kUiPrimTexShape | UiPrim_TriStrip);
    UiVec4 uvRect = kStripUvRectGouraud;
    drawer->drawTexStripGouraud(rec->width, rec->verts, rec->count, &uvRect);
}

// FS2 0x0024bcb0
// callcheck: operator_delete (the deleting destructor)

UiTexStripTrack::~UiTexStripTrack()
{
    if (m_texture != nullptr)
        delete m_texture;
}

// ──────────────────────────────────────────────────────────────────────
// Kind 0x27.
// FS2 0x0024bd40

void UiTexTrianglesRepeatTrack::draw(UiDrawer *drawer)
{
    if (m_texture == nullptr)
        return;
    drawer->setTexRepeat(1);
    drawer->setTexture(m_texture);
    const S2dTexTrianglesRec *rec = (const S2dTexTrianglesRec *)m_rec;
    drawer->setGsReg(kGsRegTex1_1, rec->tex1);
    if (u_long prim = triPrim(rec->mode, kUiPrimTexShape))
        drawer->setPrim(prim);
    drawer->drawTexGouraud(rec->verts, rec->count, 0);
    drawer->setTexRepeat(0);
}

// FS2 0x0024be20
// callcheck: operator_delete (the deleting destructor)

UiTexTrianglesRepeatTrack::~UiTexTrianglesRepeatTrack()
{
    if (m_texture != nullptr)
        delete m_texture;
}

// ──────────────────────────────────────────────────────────────────────
// Kind 0x23.
// FS2 0x0024beb0

void UiTexTrianglesFlatTrack::draw(UiDrawer *drawer)
{
    if (m_texture == nullptr)
        return;
    drawer->setTexture(m_texture);
    const S2dTexTrianglesFlatRec *rec = (const S2dTexTrianglesFlatRec *)m_rec;
    drawer->setGsReg(kGsRegTex1_1, rec->tex1);
    if (u_long prim = triPrim(rec->mode, kUiPrimTexShape))
        drawer->setPrimColor(prim, &rec->rgba);
    drawer->drawTexFlat(rec->verts, rec->count, 0);
}

// FS2 0x0024bf80
// callcheck: operator_delete (the deleting destructor)

UiTexTrianglesFlatTrack::~UiTexTrianglesFlatTrack()
{
    if (m_texture != nullptr)
        delete m_texture;
    m_texture = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Kind 7.
// FS2 0x0024c050

void UiTexTrianglesTrack::draw(UiDrawer *drawer)
{
    if (m_texture == nullptr)
        return;
    drawer->setTexture(m_texture);
    const S2dTexTrianglesRec *rec = (const S2dTexTrianglesRec *)m_rec;
    drawer->setGsReg(kGsRegTex1_1, rec->tex1);
    if (u_long prim = triPrim(rec->mode, kUiPrimTexShape))
        drawer->setPrim(prim);
    drawer->drawTexGouraud(rec->verts, rec->count, 0);
}

// FS2 0x0024c110
// callcheck: operator_delete (the deleting destructor)

UiTexTrianglesTrack::~UiTexTrianglesTrack()
{
    if (m_texture != nullptr)
        delete m_texture;
}

// ──────────────────────────────────────────────────────────────────────
// Kind 0x22: the sheet is the S2D's (not deleted).
// FS2 0x0024c1a0

void UiSheetSpriteFlatTrack::draw(UiDrawer *drawer)
{
    const S2dSheetSpriteFlatRec *rec = (const S2dSheetSpriteFlatRec *)m_rec;
    const TimSheetSprite *sprite = findSprite();
    if (sprite == nullptr)
        return;
    drawer->setImage(sheetImage(m_sheet, sprite));
    drawer->setGsReg(kGsRegTex1_1, rec->tex1);
    drawer->setPrimColor(kUiPrimTexShape | UiPrim_Sprite, &rec->rgba);
    drawer->drawSheetFlat(rec->verts, rec->count, sprite, 0);
}

// FS2 0x0024c260
// callcheck: operator_delete (the deleting destructor)

UiSheetSpriteFlatTrack::~UiSheetSpriteFlatTrack()
{
}

// ──────────────────────────────────────────────────────────────────────
// Kind 6.
// FS2 0x0024c2d0

void UiSheetSpriteTrack::draw(UiDrawer *drawer)
{
    const S2dSheetSpriteRec *rec = (const S2dSheetSpriteRec *)m_rec;
    const TimSheetSprite *sprite = findSprite();
    if (sprite == nullptr)
        return;
    drawer->setImage(sheetImage(m_sheet, sprite));
    drawer->setGsReg(kGsRegTex1_1, rec->tex1);
    drawer->setPrim(kUiPrimTexShape | UiPrim_Sprite);
    drawer->drawSheetGouraud(rec->verts, rec->count, sprite, 0);
}

// FS2 0x0024c380
// callcheck: operator_delete (the deleting destructor)

UiSheetSpriteTrack::~UiSheetSpriteTrack()
{
}

// ──────────────────────────────────────────────────────────────────────
// Kind 0x21.
// FS2 0x0024c3e0

void UiTexSpriteFlatTrack::draw(UiDrawer *drawer)
{
    if (m_texture == nullptr)
        return;
    drawer->setTexture(m_texture);
    const S2dTexSpriteFlatRec *rec = (const S2dTexSpriteFlatRec *)m_rec;
    drawer->setGsReg(kGsRegTex1_1, rec->tex1);
    drawer->setPrimColor(kUiPrimTexShape | UiPrim_Sprite, &rec->rgba);
    drawer->drawTexFlat(rec->verts, rec->count, 0);
}

// FS2 0x0024c460
// callcheck: operator_delete (the deleting destructor)

UiTexSpriteFlatTrack::~UiTexSpriteFlatTrack()
{
    if (m_texture != nullptr)
        delete m_texture;
    m_texture = nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// Kind 5.
// FS2 0x0024c530

void UiTexSpriteTrack::draw(UiDrawer *drawer)
{
    if (m_texture == nullptr)
        return;
    drawer->setTexture(m_texture);
    const S2dTexSpriteRec *rec = (const S2dTexSpriteRec *)m_rec;
    drawer->setGsReg(kGsRegTex1_1, rec->tex1);
    drawer->setPrim(kUiPrimTexShape | UiPrim_Sprite);
    drawer->drawTexGouraud(rec->verts, rec->count, 0);
}

// FS2 0x0024c5a0
// callcheck: operator_delete (the deleting destructor)

UiTexSpriteTrack::~UiTexSpriteTrack()
{
    if (m_texture != nullptr)
        delete m_texture;
}

// ──────────────────────────────────────────────────────────────────────
// Kind 4.
// FS2 0x0024c630

void UiScissorTrack::draw(UiDrawer *drawer)
{
    const S2dScissorRec *rec = (const S2dScissorRec *)m_rec;
    if (rec->enable == 0)
        drawer->resetScissor();
    else
        drawer->setScissor(rec->rect);
}

// FS2 0x0024c680
// callcheck: operator_delete (the deleting destructor)

UiScissorTrack::~UiScissorTrack()
{
}

// ──────────────────────────────────────────────────────────────────────
// Kind 3: the text setters the widgets call, then the slots.
// FS2 0x0024c6e0

const u_short *UiTextTrack::getText()
{
    if (m_font == nullptr)
        return nullptr;
    return m_font->m_text;
}

// FS2 0x0024c700
// callcheck: operator_delete[], wstrlen, operator_new[], wstrcpy

int UiTextTrack::setText(const u_short *text, bool copy)
{
    freeOwnedText(this);
    if (!copy) {
        m_text = text;
        m_ownsText = 0;
    } else if (text == nullptr) {
        m_ownsText = 0;
    } else {
        m_text = copyText(text);
        m_ownsText = 1;
    }
    m_font->m_text = m_text;
    return 0;
}

// FS2 0x0024c7c0
// callcheck: operator_delete[], wstrlen, operator_new[], wstrcpy

int UiTextTrack::setTextAt(float x, float y, const u_short *text)
{
    m_x = x;
    m_y = y;
    freeOwnedText(this);
    if (text == nullptr) {
        m_ownsText = 0;
    } else {
        m_text = copyText(text);
        m_ownsText = 1;
    }
    m_font->m_text = m_text;
    m_message = kUiTextLiteral;
    return 0;
}

// FS2 0x0024c870
// callcheck: operator_delete[], operator_delete[], fMsgRelocate, fMsgRelocate (freeOwnedText, fMsgCheck, fMsgEntry)

int UiTextTrack::setMessage(int index, void *msgFile)
{
    if (msgFile == nullptr) {
        freeOwnedText(this);
        m_text = nullptr;
        m_ownsText = 0;
        m_font->m_text = m_text;
        m_message = -1;
        return -1;
    }
    FMsgFile *file = fMsgCheck(msgFile);
    if (file == nullptr) {
        m_message = -1;
        return -1;
    }
    m_message = index;
    const FMsgEntry *entry = fMsgEntry(file, m_message);
    const u_short *text = entry != nullptr ? entry->text.get() : nullptr;
    freeOwnedText(this);
    m_text = text;
    m_ownsText = 0;
    m_font->m_text = m_text;
    return m_message;
}

// FS2 0x0024ca00
// callcheck: fMsgRelocate, fMsgFind, fMsgRelocate, operator_delete[] (fMsgCheck, fMsgEntry, freeOwnedText)

int UiTextTrack::setMessageByName(float x, float y, const char *name, void *msgFile, int offset)
{
    m_x = x;
    m_y = y;
    FMsgFile *file = fMsgCheck(msgFile);
    if (file == nullptr) {
        m_message = -1;
        return -1;
    }
    m_message = fMsgFind(file, name);
    if (m_message < 0)
        return -1;
    m_message += offset;
    const FMsgEntry *entry = fMsgEntry(file, m_message);
    const u_short *text = entry != nullptr ? entry->text.get() : nullptr;
    freeOwnedText(this);
    m_text = text;
    m_ownsText = 0;
    m_font->m_text = m_text;
    return m_message;
}

// FS2 0x0024cb80

void UiTextTrack::draw(UiDrawer *drawer)
{
    if (m_message < 0)
        return;
    if (!m_visible)
        return;
    drawer->drawText(m_font, m_message, &((const S2dTextRec *)m_rec)->box, m_x, m_y);
}

// FS2 0x0024cbd0
// callcheck: operator_delete[], ~UiFont, operator_delete (the deleting destructor)

UiTextTrack::~UiTextTrack()
{
    freeOwnedText(this);
    delete m_font;
}

// ──────────────────────────────────────────────────────────────────────
// Kind 0x1f.
// FS2 0x0024cc70

void UiTrianglesFlatTrack::draw(UiDrawer *drawer)
{
    const S2dTrianglesFlatRec *rec = (const S2dTrianglesFlatRec *)m_rec;
    if (u_long prim = triPrim(rec->mode, kUiPrimShape))
        drawer->setPrimColor(prim, &rec->rgba);
    drawer->drawFlat(rec->verts, rec->count, 0);
}

// FS2 0x0024cd10
// callcheck: operator_delete (the deleting destructor)

UiTrianglesFlatTrack::~UiTrianglesFlatTrack()
{
}

// ──────────────────────────────────────────────────────────────────────
// Kind 2.
// FS2 0x0024cd80

void UiTrianglesTrack::draw(UiDrawer *drawer)
{
    const S2dTrianglesRec *rec = (const S2dTrianglesRec *)m_rec;
    if (u_long prim = triPrim(rec->mode, kUiPrimShape))
        drawer->setPrim(prim);
    drawer->drawGouraud(rec->verts, rec->count, 0);
}

// FS2 0x0024ce20
// callcheck: operator_delete (the deleting destructor)

UiTrianglesTrack::~UiTrianglesTrack()
{
}

// ──────────────────────────────────────────────────────────────────────
// Kind 0x1e.
// FS2 0x0024ce80

void UiSpriteFlatTrack::draw(UiDrawer *drawer)
{
    const S2dSpriteFlatRec *rec = (const S2dSpriteFlatRec *)m_rec;
    drawer->setPrimColor(kUiPrimShape | UiPrim_Sprite, &rec->rgba);
    drawer->drawFlat(rec->verts, rec->count, 0);
}

// FS2 0x0024ced0
// callcheck: operator_delete (the deleting destructor)

UiSpriteFlatTrack::~UiSpriteFlatTrack()
{
}

// ──────────────────────────────────────────────────────────────────────
// Kind 1.
// FS2 0x0024cf40

void UiSpriteTrack::draw(UiDrawer *drawer)
{
    const S2dSpriteRec *rec = (const S2dSpriteRec *)m_rec;
    drawer->setPrim(kUiPrimShape | UiPrim_Sprite);
    drawer->drawGouraud(rec->verts, rec->count, 0);
}

// FS2 0x0024cf90
// callcheck: operator_delete (the deleting destructor)

UiSpriteTrack::~UiSpriteTrack()
{
}

// ──────────────────────────────────────────────────────────────────────
// Kind 0x1d.
// FS2 0x0024cff0

void UiLineStripFlatTrack::draw(UiDrawer *drawer)
{
    const S2dLineStripFlatRec *rec = (const S2dLineStripFlatRec *)m_rec;
    u_long aa = (rec->flags & kUiLineAntialias) ? UiPrim_Antialias : 0;
    drawer->setPrimColor(aa | kUiPrimShape | UiPrim_LineStrip, &rec->rgba);
    drawer->drawFlat(rec->verts, rec->count, 0);
}

// FS2 0x0024d060
// callcheck: operator_delete (the deleting destructor)

UiLineStripFlatTrack::~UiLineStripFlatTrack()
{
}

// ──────────────────────────────────────────────────────────────────────
// Kind 0.
// FS2 0x0024d0d0

void UiLineStripTrack::draw(UiDrawer *drawer)
{
    const S2dLineStripRec *rec = (const S2dLineStripRec *)m_rec;
    u_long aa = (rec->flags & kUiLineAntialias) ? UiPrim_Antialias : 0;
    drawer->setPrim(aa | kUiPrimShape | UiPrim_LineStrip);
    drawer->drawGouraud(rec->verts, rec->count, 0);
}

// FS2 0x0024d140
// callcheck: operator_delete (the deleting destructor)

UiLineStripTrack::~UiLineStripTrack()
{
}

// ──────────────────────────────────────────────────────────────────────
// The track of the anim whose record has that kind, or null.
// FS2 0x0024d1a0

UiTrack *UiAnim::findTrack(int kind)
{
    for (u_int i = 0; i < m_trackCount; i++) {
        if (kind == m_tracks[i]->m_rec->kind)
            return m_tracks[i];
    }
    return nullptr;
}

// ──────────────────────────────────────────────────────────────────────
// One frame on (or back) for a playing keyed track: a stopping track ends
// one past its last frame either way (length, or -1), a looping one wraps.
// FS2 0x0024d200

void UiTrack::step()
{
    if (m_play == UiTrackPlay_Stopped || !isKeyedKind(m_rec->kind))
        return;
    int frame = m_frame;
    int length = m_rec->length;
    if (m_play == UiTrackPlay_Forward)
        frame++;
    else if (m_play == UiTrackPlay_Backward)
        frame--;
    if (!m_loop) {
        if (frame < 0)
            frame = -1;
        if (frame >= length)
            frame = length;
    } else {
        if (frame < 0)
            frame = length - 1;
        if (frame >= length)
            frame = 0;
    }
    m_frame = frame;
}

// ──────────────────────────────────────────────────────────────────────
// Stopped, unkeyed, looping and paused tracks are done at once; a playing
// one when it ran off its end.
// FS2 0x0024d2c0

bool UiTrack::isDone()
{
    if (m_play == UiTrackPlay_Stopped)
        return true;
    if (!isKeyedKind(m_rec->kind))
        return true;
    if (m_loop)
        return true;
    if (m_play == UiTrackPlay_Stopped || m_play == UiTrackPlay_Paused)
        return true;
    if (m_play == UiTrackPlay_Forward) {
        if (m_frame >= m_rec->length)
            return true;
    } else if (m_play == UiTrackPlay_Backward && m_frame < 0) {
        return true;
    }
    return false;
}

// ──────────────────────────────────────────────────────────────────────
// UiPlay_Stop wins; else the first of forward / backward / pause, and the
// first of rewind / to-end.
// FS2 0x0024d360

void UiTrack::setPlay(u_int flags)
{
    if (flags & UiPlay_Stop) {
        m_play = UiTrackPlay_Stopped;
        return;
    }
    if (flags & UiPlay_Forward)
        m_play = UiTrackPlay_Forward;
    else if (flags & UiPlay_Backward)
        m_play = UiTrackPlay_Backward;
    else if (flags & UiPlay_Pause)
        m_play = UiTrackPlay_Paused;
    if (flags & UiPlay_Rewind)
        m_frame = 0;
    else if (flags & UiPlay_ToEnd)
        m_frame = m_rec->length - 1;
}

// ──────────────────────────────────────────────────────────────────────
// The inline virtuals, emitted at the end of the file: the widget base's
// two empty slots and the shapes' anchors (their first vertex's xy).
// FS2 0x0024d400

void UiWidget::refresh(bool argChanged)
{
}

// FS2 0x0024d410

int UiWidget::getStatus()
{
    return 0;
}

// FS2 0x0024d420

const float *UiTexStripFlatTrack::getAnchor()
{
    return ((const S2dTexStripFlatRec *)m_rec)->verts[0].xy;
}

// FS2 0x0024d430

const float *UiTexStripTrack::getAnchor()
{
    return ((const S2dTexStripRec *)m_rec)->verts[0].xy;
}

// FS2 0x0024d440

const float *UiTexTrianglesRepeatTrack::getAnchor()
{
    return ((const S2dTexTrianglesRec *)m_rec)->verts[0].xy;
}

// FS2 0x0024d450

const float *UiTexTrianglesFlatTrack::getAnchor()
{
    return ((const S2dTexTrianglesFlatRec *)m_rec)->verts[0].xy;
}

// FS2 0x0024d460

const float *UiTexTrianglesTrack::getAnchor()
{
    return ((const S2dTexTrianglesRec *)m_rec)->verts[0].xy;
}

// FS2 0x0024d470

const float *UiSheetSpriteFlatTrack::getAnchor()
{
    return ((const S2dSheetSpriteFlatRec *)m_rec)->verts[0].xy;
}

// FS2 0x0024d480

const float *UiSheetSpriteTrack::getAnchor()
{
    return ((const S2dSheetSpriteRec *)m_rec)->verts[0].xy;
}

// FS2 0x0024d490

const float *UiTexSpriteFlatTrack::getAnchor()
{
    return ((const S2dTexSpriteFlatRec *)m_rec)->verts[0].xy;
}

// FS2 0x0024d4a0

const float *UiTexSpriteTrack::getAnchor()
{
    return ((const S2dTexSpriteRec *)m_rec)->verts[0].xy;
}

// FS2 0x0024d4b0

const float *UiScissorTrack::getAnchor()
{
    return ((const S2dScissorRec *)m_rec)->rect;
}

// FS2 0x0024d4c0

const float *UiTextTrack::getAnchor()
{
    return ((const S2dTextRec *)m_rec)->box.topLeft;
}

// FS2 0x0024d4d0

const float *UiTrianglesFlatTrack::getAnchor()
{
    return ((const S2dTrianglesFlatRec *)m_rec)->verts[0].xy;
}

// FS2 0x0024d4e0

const float *UiTrianglesTrack::getAnchor()
{
    return ((const S2dTrianglesRec *)m_rec)->verts[0].xy;
}

// FS2 0x0024d4f0

const float *UiSpriteFlatTrack::getAnchor()
{
    return ((const S2dSpriteFlatRec *)m_rec)->verts[0].xy;
}

// FS2 0x0024d500

const float *UiSpriteTrack::getAnchor()
{
    return ((const S2dSpriteRec *)m_rec)->verts[0].xy;
}

// FS2 0x0024d510

const float *UiLineStripFlatTrack::getAnchor()
{
    return ((const S2dLineStripFlatRec *)m_rec)->verts[0].xy;
}

// FS2 0x0024d520

const float *UiLineStripTrack::getAnchor()
{
    return ((const S2dLineStripRec *)m_rec)->verts[0].xy;
}
