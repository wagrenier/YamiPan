/*
 *  game/uiitemlist.cpp -- the Use button's list (FS2 0x23e740..0x23f0b0 + the
 *  ctor), and its prompt UiPrompt (0x208330.. + 0x208bb0..0x208d40).
 *
 *  What the player can do where he stands is said by the motion's conditions
 *  (CharacterAllows) and by scripts (post). Each frame update picks the one
 *  of highest priority into the back slot and turns the slots over, so a
 *  slot is one frame old when it is read: the press of Use goes to it, the
 *  input through offer for the built-in kinds, the scene through pick for a
 *  script's. The kinds' labels and priorities are the scripts' (natives 0xf7,
 *  0xf8), and a kind no script touched still works, with the ROM's label
 *  "未定義".
 *
 *  Not drawn: the button's picture beside the label (the init pack's sheet,
 *  a sprite per button of the options' map).
 */
#include "game/gamehooks.h"
#include "game/fontdata.h"
#include "game/playermotion.h"
#include "game/stagescene.h"
#include "game/sysdata.h"
#include "game/uigame.h"
#include "game/uiwidget.h"

// The ROM's labels: data 0x307400 (none) and 0x307408, 未定義 (undefined).
static const char kNoLabel[] = "";
static const char kUndefinedLabel[] = "\x96\xa2\x92\xe8\x8b\x60"; // 未定義: disc text, Shift-JIS
static constexpr int kDefaultPriority = 0x80;
// UiShowFlags byte: the list is on.
static constexpr int kShowItemList = 4;

// One frame of the prompt's fade: a colour, and which of the button's two
// pictures (data 0x2fa6e0, 0x2fa720, 0x2fa780).
// layout: host
struct UiPromptKey
{
    u_int rgba;
    u_int lit;
};
// layout: host
struct UiPromptFade
{
    int frames;
    const UiPromptKey *keys;
};
enum UiPromptAnim
{
    UiPromptAnim_Open = 0,
    UiPromptAnim_Press = 2,
    UiPromptAnim_Close = 3,
};
static const UiPromptKey kOpenKeys[] = {
    { 0x007f7f7f, 1 }, { 0x157f7f7f, 1 }, { 0x2a7f7f7f, 1 }, { 0x407f7f7f, 1 },
    { 0x557f7f7f, 1 }, { 0x6a7f7f7f, 1 }, { 0x7f7f7f7f, 1 },
};
static const UiPromptKey kPressKeys[] = {
    { 0x007f7f7f, 0 }, { 0x7f7f7f7f, 1 }, { 0x007f7f7f, 0 }, { 0x7f7f7f7f, 1 },
    { 0x007f7f7f, 0 }, { 0x7f7f7f7f, 1 }, { 0x6a7f7f7f, 1 }, { 0x557f7f7f, 1 },
    { 0x407f7f7f, 1 }, { 0x2a7f7f7f, 1 }, { 0x157f7f7f, 1 }, { 0x007f7f7f, 1 },
};
static const UiPromptKey kCloseKeys[] = {
    { 0x6a7f7f7f, 1 }, { 0x557f7f7f, 1 }, { 0x407f7f7f, 1 }, { 0x2a7f7f7f, 1 },
    { 0x157f7f7f, 1 }, { 0x007f7f7f, 1 },
};
static const UiPromptFade kOpenFade = { 7, kOpenKeys };     // data 0x2fa7b0
static const UiPromptFade kPressFade = { 12, kPressKeys };  // data 0x2fa7c0
static const UiPromptFade kCloseFade = { 6, kCloseKeys };   // data 0x2fa7c8
static const UiPromptKey kBlockedKey = { 0x7f7f7f7f, 0 };   // data 0x31aa08
// Which sprite of the sheet shows each button of the map.
static const int kButtonSprites[] = { 10, 14, 8, 12, 0, 2, 4, 6 };

// FS2 0x00208c70

UiPrompt::UiPrompt(WideFontPrinter *printer_, int arg_)
    : printer(printer_), arg(arg_)
{
}

// FS2 0x00208bb0

bool UiPrompt::setLabel(const void *text, int value_)
{
    if (state == State_Pressed)
        return false;
    label = text;
    value = value_;
    return true;
}

// FS2 0x00208330

const void *UiPrompt::press()
{
    if (state == State_Pressed)
        return shown;
    if (canPress == 0 || label == nullptr)
        return nullptr;
    state = State_Pressed;
    fade = &kPressFade;
    animFrame = 0;
    animKind = UiPromptAnim_Press;
    return shown;
}

// FS2 0x00208380
// callcheck: operator_new, timSheetRelocate, timSheetRelocate, Tim2Texture (the bar's texture is the host pass's, not written)

void UiPrompt::setSheet(void *sheet_)
{
    if (sheet_ != nullptr)
        sheet = sheet_;
}

// FS2 0x00208490
// callcheck: getGameOptions, timSheetRelocate, timSheetRelocate, place, upload, putTex0, sceVif1PkAddCode, sceVif1PkAddGsData, sceVif1PkAlign, sceVif1PkCloseDirectCode, sceVif1PkCloseDmaTag, sceVif1PkCloseGifTag, sceVif1PkCnt, sceVif1PkOpenDirectCode, sceVif1PkOpenGifTag (the button's picture and the ROM's packets are not drawn; the text goes through the printer)

void UiPrompt::draw(sceVif1Packet *packet)
{
    constexpr int kScreenCenter = 0x100;
    constexpr int kPromptY = -210 + kScreenCenter;  // data 0x3068b8
    constexpr int kLeftLimit = -0x6b;
    constexpr int kTextRgb = 0x606060;

    if (label == shown) {
        if (state != State_Pressed && animKind != UiPromptAnim_Open) {
            fade = &kOpenFade;
            animFrame = 0;
            animKind = UiPromptAnim_Open;
        }
    }
    else if (label == nullptr) {
        if (animKind != UiPromptAnim_Close) {
            fade = &kCloseFade;
            animFrame = 0;
            animKind = UiPromptAnim_Close;
        }
    }
    else {
        shown = label;
        fade = &kOpenFade;
        animFrame = 0;
        animKind = UiPromptAnim_Open;
    }
    animFrame++;
    if (shown == nullptr || fade == nullptr)
        return;

    const int frame = animFrame - 1 < fade->frames ? animFrame - 1 : fade->frames - 1;
    const UiPromptKey *key = &fade->keys[frame];
    if (canPress == 0 && (key->rgba >> 24) != 0 && animKind != UiPromptAnim_Press)
        key = &kBlockedKey;

    int button = getGameOptions()->buttonMap.button[0];
    int spriteIndex = 0 <= button && button < 8 ? kButtonSprites[button] : 0;
    if (key->lit != 0)
        spriteIndex++;
    const TimSheetSprite *sprite = sheet != nullptr ? timSheetSprite(static_cast<TimSheet *>(sheet), spriteIndex) : nullptr;
    const int buttonWidth = sprite != nullptr ? sprite->w : 0;

    const u_short *text = static_cast<const u_short *>(shown);
    int width = printer->textWidth(reinterpret_cast<const char *>(text)) >> 4;
    int x = -((width + buttonWidth) / 2);
    if (x < kLeftLimit)
        x = kLeftLimit;

    printer->uploadTexture(packet);
    printer->m_packet = packet;
    printer->setDrawState(packet, false);
    printer->m_spacing = 0x20;
    printer->m_lineSpacing = 0;
    printer->m_rgba = (key->rgba & 0xff000000) | kTextRgb;
    printer->m_flags = 0;
    printer->printText(x + buttonWidth + kScreenCenter, kPromptY, text);
    printer->m_packet = nullptr;
}

// FS2 0x00208be0

void UiPrompt::endFrame()
{
    label = nullptr;
    lastValue = value;
    value = -1;
}

// FS2 0x00208cc0

void UiPrompt::stepPress()
{
    if (state != State_Pressed || (fade != nullptr && animFrame <= fade->frames))
        return;
    label = nullptr;
    lastValue = value;
    value = -1;
    state = State_Idle;
    shown = nullptr;
    canPress = 1;
}

void UiPickSlot::clear()
{
    object = nullptr;
    kind = UiItemList::kNoPriority;
    priority = UiItemList::kNoPriority;
    label = kNoLabel;
    enabled = 0;
}

// FS2 0x0023e740

bool UiPickSlot::isReady()
{
    return object != nullptr && UiUse_Jump < kind;
}

// FS2 0x0023f0b0
// callcheck: operator_new, FUN_00208c70

UiItemList::UiItemList(WideFontPrinter *printer, int arg)
{
    prompt = new UiPrompt(printer, arg);
    flip = 0;
    pendingCount = 0;
    for (KindRow &row : kinds) {
        row.enabled = 1;
        row.priority = kDefaultPriority;
        row.label = kUndefinedLabel;
    }
    slots[0].clear();
    slots[1].clear();
}

// FS2 0x0023f040
// callcheck: FUN_00208c00, operator_delete (the host deletes)

void UiItemList::destroy()
{
    delete prompt;
    delete this;
}

// One built-in kind asks for the slot; the higher priority stands.

void UiItemList::takeKind(UiPickSlot *slot, StageCharacter *who, int kind)
{
    const KindRow &row = kinds[kind];
    if (row.label == nullptr || row.enabled == 0)
        return;
    if (slot->object != nullptr && row.priority < slot->priority)
        return;
    slot->object = who;
    slot->kind = kind;
    slot->priority = row.priority;
    slot->label = row.label;
    slot->enabled = 1;
}

// FS2 0x0020c0e0
// callcheck: isRunning, isWalkBusy, isQueued, isNotRunning

bool StageCharacter::canTakePrompt()
{
    bool free;
    if (gStageScene->player == this)
        free = !gStageScene->party->isRunning();
    else
        free = m_motion->motionId != 0x11 && m_motion->motionId != 0x2a; // jack, sq_jack
    if (!free)
        free = m_altAction != 0;
    if (!free || m_motion->isWalkBusy() || m_motion->isQueued())
        return false;
    return gStageScene->fade->isNotRunning();
}

// The armed motions a script's prompt waits out, attacks among them.

static bool isArmedBusyMotion(int id)
{
    switch (id) {
    case 0x90: case 0x81: case 0x72: case 0x50: case 0x4f:
    case 0x8a: case 0x7b: case 0x6c: case 0x51:
    case 0x8b: case 0x7c: case 0x6d: case 0x52:
    case 0x8d: case 0x8c: case 0x7e: case 0x7d: case 0x6f: case 0x6e: case 0x54: case 0x53:
    case 0x8f: case 0x8e: case 0x80: case 0x7f: case 0x71: case 0x70: case 0x58: case 0x57: case 0x56: case 0x55:
    case 0x93: case 0x88: case 0x79: case 0x69: case 0x68:
        return true;
    }
    return isAttackMotion(id);
}

// FS2 0x0020b590
// callcheck: isRunning, isWalkBusy, isQueued, isNotRunning (canTakePrompt)

bool StageCharacter::canUsePrompt()
{
    if (!canTakePrompt() || isArmedBusyMotion(m_motion->motionId))
        return false;
    if (m_gear != nullptr && m_abilities.driving == 1)
        return true;
    const int id = m_motion->motionId;
    return isRestingMotion(id) || isMovingMotion(id);
}

// FS2 0x0023e7b0
// callcheck: FUN_0020c0e0, FUN_0020b590, FUN_00208bb0, getInputFocus, getInputFocus (canTakePrompt, canUsePrompt)

void UiItemList::update()
{
    pendingCount = 0;
    UiPickSlot *back = &slots[(flip + 1) & 1];
    if (gStageScene->showFlags->flag[kShowItemList] == 0) {
        back->clear();
    }
    else {
        StageCharacter *who = static_cast<StageCharacter *>(
            gStageScene->controlled != nullptr ? gStageScene->controlled : gStageScene->player);
        if (who != nullptr) {
            if (who->canTakePrompt()) {
                const CharacterAllows &allows = who->m_allows;
                if (allows.level == 1)
                    takeKind(back, who, UiUse_LevelUp);
                else if (allows.level == 2)
                    takeKind(back, who, UiUse_LevelDown);
                if (allows.ladder == 1 || allows.ladder == 2)
                    takeKind(back, who, UiUse_Ladder);
                if (allows.useItem5 != 0)
                    takeKind(back, who, UiUse_Gate);
                if (allows.jump != 0)
                    takeKind(back, who, UiUse_Jump);
            }
            if (back->isReady() && !who->canUsePrompt())
                back->clear();
        }
    }

    flip++;
    slots[(flip + 1) & 1].clear();
    UiPickSlot *front = frontSlot();
    if (front->object != nullptr && !prompt->setLabel(front->label, 0))
        front->clear();
    if (front->isReady()) {
        prompt->canPress = gStageScene->getInputFocus() == this;
    }
    else {
        StageObject *who = gStageScene->controlled != nullptr ? gStageScene->controlled : gStageScene->player;
        prompt->canPress = gStageScene->getInputFocus() == who;
    }
}

// FS2 0x0023ed10
// callcheck: isControlled, FUN_00208bb0, FUN_00208330

u_char UiItemList::offer(StageCharacter *who, int kind)
{
    UiPickSlot *front = frontSlot();
    bool mine;
    if (who->isControlled())
        mine = front->object == who && front->kind == kind;
    else
        mine = front->object != who || front->kind == kind;
    if (!mine)
        return 0;
    if (front->object == who && front->enabled != 0) {
        prompt->setLabel(front->label, 0);
        prompt->press();
        const int scriptIndex = front->kind - UiUse_Script;
        pendingCount = scriptIndex > 0 ? scriptIndex : 0;
    }
    return 1;
}

// FS2 0x0023ee90
// callcheck: FUN_00208bb0, FUN_00208330

int UiItemList::pick()
{
    UiPickSlot *front = frontSlot();
    if (front->object == nullptr || front->enabled == 0)
        return -1;
    prompt->setLabel(front->label, 0);
    prompt->press();
    const int scriptIndex = front->kind - UiUse_Script;
    pendingCount = scriptIndex > 0 ? scriptIndex : 0;
    return front->kind;
}

// FS2 0x0023ef40

bool UiItemList::post(void *object, int scriptKind, const void *label, int priority, bool enabled)
{
    UiPickSlot *back = &slots[(flip + 1) & 1];
    if (back->object != nullptr && priority < back->priority)
        return false;
    back->object = object;
    back->kind = scriptKind + UiUse_Script;
    back->priority = priority;
    back->label = label;
    back->enabled = enabled;
    return true;
}

// FS2 0x0023efb0

void UiItemList::enableKind(int kind, bool enabled)
{
    if (kind < kKinds)
        kinds[kind].enabled = enabled;
}

// FS2 0x0023eff0

void UiItemList::setKind(int kind, const void *label, int priority)
{
    if (kind < kKinds) {
        kinds[kind].label = label;
        kinds[kind].priority = priority;
    }
}

// FS2 0x0023f030
// callcheck: FUN_00208380

void UiItemList::setSheet(void *sheet)
{
    prompt->setSheet(sheet);
}

// FS2 0x0023e770
// callcheck: FUN_00208490, FUN_00208be0, FUN_00208cc0

void UiItemList::draw(sceVif1Packet *packet, TexAreaList *)
{
    prompt->draw(packet);
    prompt->endFrame();
    prompt->stepPress();
}
