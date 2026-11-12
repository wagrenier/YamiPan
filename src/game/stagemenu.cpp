/* ==========================================================================
 *  game/stagemenu.cpp -- the stage's own menu (FS2 0x2303c0..0x230980, vtable
 *  0x3164b0): a state machine around a list window of 0xaa8 bytes, its code
 *  at 0x208d40..0x20acb0 and its fades at 0x22fea0..0x2303c0. Triangle opens
 *  and closes it, the first button leaves the picked line's text in `pending`
 *  for one frame, where the scripts read it (native 0x45, getMenuMode 3).
 *  The file name is the port's.
 *
 *  The ROM writes the plates as GS packets and uploads the plate's picture;
 *  here the same values go to the host UI draw module, the texture bound by
 *  its owner. The plates and text keep their shape at any aspect, as the Use
 *  prompt beside them does.
 * ======================================================================== */
#include "game/stagemenu.h"
#include "game/characterinput.h"
#include "game/clock.h"
#include "game/fontdata.h"
#include "game/gamesystem.h"
#include "game/stagescene.h"
#include "game/thread.h"
#include "game/uiwidget.h"
#include "game/wstring.h"
#include "rena/rena_2d_texture_tim2.h"
#include "sdk/libgraph.h"
#include "yamipan/rendering/yamipan_drawui.h"

// The rows: on a line at the plate's left, centred on the screen's middle.
static constexpr float kRowX = -89.0f;
static constexpr float kRowTop = -27.0f;
static constexpr float kRowStep = 28.050001f;

// data 0x307178: the plate in the picture, 12.4.
static constexpr u_int kPlateU = 0x10;
static constexpr u_int kPlateV = 0x10;
static constexpr u_int kPlateW = 0xe60;
static constexpr u_int kPlateH = 0x1d0;
static constexpr float kPlateScale = 0.75f;
static constexpr float kPlateAspect = 512.0f / 448.0f;

static constexpr float kGsCenter = 2048.0f;     // the draw env's origin
static constexpr float kGsSubpixels = 16.0f;    // XY are 12.4
static constexpr u_long kPlatePrim = SCE_GS_SET_PRIM(6, 0, 0, 0, 1, 0, 1, 0, 0);
static constexpr u_long kPlatePrimTextured = SCE_GS_SET_PRIM(6, 0, 1, 0, 1, 0, 1, 0, 0);
static constexpr u_long kPlateTest = SCE_GS_SET_TEST(1, 0, 0, 3, 0, 0, 1, 1);
static constexpr u_long kPlateAlpha = SCE_GS_SET_ALPHA(0, 1, 0, 1, 0);
static constexpr u_int kPlateZ0 = 0xff0fffff;
static constexpr u_int kPlateZ1 = 0xffffffff;

// The text on a plate: black, the track's alpha, (x, y) from the plate's.
static constexpr float kTextScale = 0.75f;
static constexpr float kTextX = 260.0f;
static constexpr float kTextY = 256.5f;
static constexpr u_int kTextAlphaMask = 0xff000000;
static constexpr int   kTextSpacing = 0x20;

// The show flag that lets the menu open.
static constexpr int kShowFlagStageMenu = 3;

// The colours, ABGR, alpha 0x7f the most; data 0x2fa840..0x2fa98c. Each of the
// four looks is a word of its own in sdata 0x31aa70.. (the same values three
// more times there); one is enough here.
static const u_int kNormal[] = { 0x7f606060 };
static const u_int kOff[] = { 0x40606060 };
static const u_int kSelected[] = { 0x7f00507f };
static const u_int kSelectedOff[] = { 0x4000507f };
static const u_int kNormalIn[] = { 0x157a7a7a, 0x2a757575, 0x40707070, 0x556a6a6a, 0x6a656565, 0x7f606060 };
static const u_int kNormalOut[] = { 0x6a656565, 0x556a6a6a, 0x40707070, 0x2a757575, 0x157a7a7a, 0x007f7f7f };
static const u_int kNormalInFast[] = { 0x2a757575, 0x556a6a6a, 0x7f606060 };
static const u_int kNormalOutFast[] = { 0x556a6a6a, 0x2a757575, 0x007f7f7f };
static const u_int kOffIn[] = { 0x0a7a7a7a, 0x15757575, 0x20707070, 0x2a6a6a6a, 0x35656565, 0x40606060 };
static const u_int kOffOut[] = { 0x35656565, 0x2a6a6a6a, 0x20707070, 0x15757575, 0x0a7a7a7a, 0x007f7f7f };
static const u_int kOffInFast[] = { 0x15757575, 0x2a6a6a6a, 0x40606060 };
static const u_int kOffOutFast[] = { 0x2a6a6a6a, 0x15757575, 0x007f7f7f };
static const u_int kSelectedIn[] = { 0x15004f7f, 0x2a004f7f, 0x4000507f, 0x55004f7f, 0x6a004f7f, 0x7f00507f };
static const u_int kSelectedOut[] = { 0x6a004f7f, 0x55004f7f, 0x4000507f, 0x2a004f7f, 0x15004f7f, 0x0000507f };
static const u_int kSelectedInFast[] = { 0x2a004f7f, 0x55004f7f, 0x7f00507f };
static const u_int kSelectedOutFast[] = { 0x55004f7f, 0x2a004f7f, 0x0000507f };
static const u_int kSelectedOffIn[] = { 0x0a004f7f, 0x15004f7f, 0x2000507f, 0x2a004f7f, 0x35004f7f, 0x4000507f };
static const u_int kSelectedOffOut[] = { 0x35004f7f, 0x2a004f7f, 0x2000507f, 0x15004f7f, 0x0a004f7f, 0x0000507f };
static const u_int kSelectedOffInFast[] = { 0x15004f7f, 0x2a004f7f, 0x4000507f };
static const u_int kSelectedOffOutFast[] = { 0x2a004f7f, 0x15004f7f, 0x0000507f };
static const u_int kPicked[] = { 0x00606060, 0x7f00507f, 0x00606060, 0x7f00507f, 0x00606060, 0x7f00507f };
static const u_int kNoColor[] = { 0 };          // bss 0x31ae38

#define MENU_TRACK(colors, text) { (int)(sizeof(colors) / sizeof(colors[0])), colors, text }

// data 0x2fa9a0
static const StageMenuTrack kTracks[] = {
    MENU_TRACK(kNormal, 1), MENU_TRACK(kOff, 1), MENU_TRACK(kSelected, 1), MENU_TRACK(kSelectedOff, 1),
    MENU_TRACK(kNormalIn, 1), MENU_TRACK(kNormalOut, 1), MENU_TRACK(kNormalInFast, 0), MENU_TRACK(kNormalOutFast, 0),
    MENU_TRACK(kNormal, 1), MENU_TRACK(kOff, 1), MENU_TRACK(kSelected, 1), MENU_TRACK(kSelectedOff, 1),
    MENU_TRACK(kOffIn, 1), MENU_TRACK(kOffOut, 1), MENU_TRACK(kOffInFast, 0), MENU_TRACK(kOffOutFast, 0),
    MENU_TRACK(kNormal, 1), MENU_TRACK(kOff, 1), MENU_TRACK(kSelected, 1), MENU_TRACK(kSelectedOff, 1),
    MENU_TRACK(kSelectedIn, 1), MENU_TRACK(kSelectedOut, 1), MENU_TRACK(kSelectedInFast, 0), MENU_TRACK(kSelectedOutFast, 0),
    MENU_TRACK(kNormal, 1), MENU_TRACK(kOff, 1), MENU_TRACK(kSelected, 1), MENU_TRACK(kSelectedOff, 1),
    MENU_TRACK(kSelectedOffIn, 1), MENU_TRACK(kSelectedOffOut, 1), MENU_TRACK(kSelectedOffInFast, 0), MENU_TRACK(kSelectedOffOutFast, 0),
};
static const StageMenuTrack kPickedTrack = MENU_TRACK(kPicked, 1);      // data 0x2fab20
static const StageMenuTrack kNoTrack = MENU_TRACK(kNoColor, 1);         // data 0x2fab2c

// data 0x2fab40: the track from one look (row) to another, -1 none.
static const int kLookTracks[StageMenuLook_Count][StageMenuLook_Count] = {
    { -1, -1, 0x06, 0x0e, 0x16, 0x1e },
    { -1, -1, 0x04, 0x0c, 0x14, 0x1c },
    { 0x07, 0x05, 0x00, 0x01, 0x02, 0x03 },
    { 0x0f, 0x0d, 0x08, 0x09, 0x0a, 0x0b },
    { 0x17, 0x15, 0x10, 0x11, 0x12, 0x13 },
    { 0x1f, 0x1d, 0x18, 0x19, 0x1a, 0x1b },
};

// ---- the fade ------------------------------------------------------------

// FS2 0x0020a720

int StageMenuColor::frames() const
{
    return track != nullptr ? track->frames : 0;
}

// Inline in the ROM's clears.

void StageMenuFade::clear()
{
    fromX = fromY = 0.0f;
    toX = toY = 0.0f;
    x = y = 0.0f;
    moveFrame = 0;
    moveFrames = 0;
    color.frame = 0;
    color.track = nullptr;
    color.from = -1;
    color.to = -1;
}

// FS2 0x0022fea0

bool StageMenuFade::setLook(int to, int from)
{
    if (to == kStageMenuLookPicked) {
        color.from = StageMenuLook_Selected;
        color.to = StageMenuLook_Selected;
        color.track = &kPickedTrack;
        color.frame = 0;
        return true;
    }

    bool started = true;
    if (from != kStageMenuFromCurrent) {
        color.from = from;
        color.to = to;
    } else if (color.from == -1) {
        color.from = to;
        color.to = to;
    } else if (to != color.to) {
        color.from = color.to;
        color.to = to;
    } else {
        started = false;
        if (color.frame < color.frames() || color.from == color.to)
            return false;
        color.from = color.to;
    }

    const int track = kLookTracks[color.from][color.to];
    color.frame = 0;
    if (track == -1) {
        color.track = &kNoTrack;
        return false;
    }
    color.track = &kTracks[track];
    return started;
}

// The colour of the track's frame.
static u_int trackColor(const StageMenuColor &color)
{
    const StageMenuTrack *track = color.track;
    int frame = color.frame - 1;
    if (frame < 0)
        frame = 0;
    if (track->frames <= frame)
        frame = track->frames - 1;
    return track->colors[frame];
}

// FS2 0x0022ffd0
// callcheck: place, upload, putTex0, sceVif1PkCnt, sceVif1PkAddCode, sceVif1PkOpenDirectCode, sceVif1PkOpenGifTag, sceVif1PkAddGsData, sceVif1PkAddGsData, sceVif1PkAddGsData, sceVif1PkAddGsData, sceVif1PkAddGsData, sceVif1PkAddGsData, sceVif1PkCloseGifTag, sceVif1PkCloseDirectCode, sceVif1PkCloseDmaTag, sceVif1PkAddGsData (PC: YamiPan_DrawUi*)

void StageMenuFade::draw(sceVif1Packet *packet, TexAreaList *texList, Tim2Texture *plate)
{
    (void)texList;      // PC: the host binds the picture by its owner
    const u_int rgba = color.track != nullptr ? trackColor(color) : 0;
    const int height = (int)((float)kPlateH * kPlateScale * kPlateAspect);
    const int left = (int)((x + kGsCenter) * kGsSubpixels);
    const int top = (int)((y + kGsCenter) * kGsSubpixels) - height / 2;

    YamiPan_DrawUiReg(packet, SCE_GS_TEST_1, kPlateTest);
    YamiPan_DrawUiReg(packet, SCE_GS_ALPHA_1, kPlateAlpha);
    YamiPan_DrawUiReg(packet, SCE_GS_CLAMP_1, 0);
    YamiPan_DrawUiTexture(packet, plate);

    const u_char rgbaq[4] = { (u_char)rgba, (u_char)(rgba >> 8), (u_char)(rgba >> 16), (u_char)(rgba >> 24) };
    YamiPan_DrawUiPrim(packet, plate != nullptr ? kPlatePrimTextured : kPlatePrim, rgbaq);
    YamiPan_DrawUiBegin(packet, YAMIPAN_UIV_UV);
    YamiPanUiVertex corner = {};
    corner.u = (uint16_t)kPlateU;
    corner.v = (uint16_t)kPlateV;
    corner.x = (uint16_t)left;
    corner.y = (uint16_t)top;
    corner.z = kPlateZ0;
    YamiPan_DrawUiVertex(packet, &corner);
    corner.u = (uint16_t)(kPlateU + kPlateW);
    corner.v = (uint16_t)(kPlateV + kPlateH);
    corner.x = (uint16_t)(left + (int)((float)kPlateW * kPlateScale));
    corner.y = (uint16_t)(top + height);
    corner.z = kPlateZ1;
    YamiPan_DrawUiVertex(packet, &corner);
}

// FS2 0x0020a6e0

void StageMenuFade::settle()
{
    fromX = toX;
    fromY = toY;
    x = toX;
    y = toY;
}

// FS2 0x0020a710

void StageMenuFade::startMove(int frames)
{
    moveFrame = 0;
    moveFrames = frames;
}

// The row's y, the rows centred on the screen's middle.
static float rowY(int row, int rows)
{
    if (rows == 0)
        return kRowTop;
    return (float)row * kRowStep + (kRowTop - ((float)(rows - 1) * kRowStep) / 2.0f) + 0.0f;
}

// FS2 0x0020a740

void StageMenuFade::aimAtRow(int row, int rows)
{
    toX = kRowX;
    toY = rowY(row, rows);
}

// FS2 0x0020a7d0

void StageMenuFade::placeAtRow(int row, int rows)
{
    fromX = kRowX;
    fromY = rowY(row, rows);
    x = fromX;
    y = fromY;
}

// Inline in drawLines.

void StageMenuFade::advance()
{
    if (moveFrames != 0) {
        moveFrame++;
        if (moveFrames < moveFrame) {
            x = toX;
            y = toY;
        } else {
            x = (float)moveFrame * ((toX - fromX) / (float)moveFrames) + fromX + 0.0f;
            y = (float)moveFrame * ((toY - fromY) / (float)moveFrames) + fromY + 0.0f;
        }
    }
    color.frame++;
}

// ---- a line --------------------------------------------------------------

// Inline in addLine, layout and reset.

void StageMenuLine::clear()
{
    state = StageMenuLine_Free;
    text = nullptr;
    order = 0;
    asked = 0;
    changed = 0;
    selected = 0;
    shown = 0;
    fade.clear();
    rowChanged = 0;
    row = -1;
    rows = 0;
    slidesIn = 0;
    unused50 = -1;
}

void StageMenuLine::setShown(bool on)
{
    if (shown != (u_char)on) {
        shown = on;
        changed = 1;
    }
}

void StageMenuLine::setSelected(bool on)
{
    if (selected != (u_char)on) {
        selected = on;
        changed = 1;
    }
}

int StageMenuLine::restLook() const
{
    if (enabled)
        return selected ? StageMenuLook_Selected : StageMenuLook_Normal;
    return selected ? StageMenuLook_SelectedOff : StageMenuLook_Off;
}

// ---- the window ----------------------------------------------------------

// FS2 0x0020ac30
// callcheck: createChild, reset

StageMenuList::StageMenuList(WideFontPrinter *printer, int withClock) : m_printer(printer)
{
    if (withClock != 0) {
        m_clock = gGameSystem->m_clock.createChild();
        m_noClock = 0;
    }
    reset();
}

// FS2 0x0020aba0
// callcheck: ~Clock, operator_delete (deleting)

StageMenuList::~StageMenuList()
{
    delete m_plate;
    if (m_noClock == 0)
        delete m_clock;
}

// FS2 0x0020aaf0

void StageMenuList::reset()
{
    m_page = 0;
    m_laidOut = 0;
    m_lineCount = 0;
    for (int page = 0; page < kStageMenuPages; page++) {
        for (int i = 0; i < kStageMenuLines; i++)
            m_pages[page][i].clear();
    }
}

// FS2 0x0020aaa0
// callcheck: operator_new, Tim2Texture

void StageMenuList::setData(void *data)
{
    m_data = data;
    m_plate = new Tim2Texture(data);
}

// FS2 0x00208d40
// callcheck: setLook

bool StageMenuList::closeDone()
{
    if (m_closeStep == StageMenuClose_Done)
        return true;
    StageMenuLine *line = lines();
    if (m_closeStep == StageMenuClose_Picked) {
        const int selected = selectedIndex();
        if (line[selected < 0 ? 0 : selected].fade.finished()) {
            for (int i = 0; i < kStageMenuLines; i++) {
                if (!line[i].isFree())
                    line[i].fade.setLook(StageMenuLook_Closed);
            }
            m_closeStep = StageMenuClose_Fading;
        }
        return false;
    }
    for (int i = 0; i < kStageMenuLines; i++) {
        if (!line[i].isFree() && !line[i].fade.finished())
            return false;
    }
    m_closeStep = StageMenuClose_Done;
    return true;
}

// FS2 0x00208f40
// callcheck: setLook, setLook

void StageMenuList::close(bool picked)
{
    StageMenuLine *line = lines();
    if (picked) {
        m_closeStep = StageMenuClose_Picked;
        const int selected = selectedIndex();
        line[selected < 0 ? 0 : selected].fade.setLook(kStageMenuLookPicked);
        return;
    }
    m_closeStep = StageMenuClose_Fading;
    for (int i = 0; i < kStageMenuLines; i++) {
        if (!line[i].isFree())
            line[i].fade.setLook(StageMenuLook_Closed);
    }
}

// FS2 0x00209080
// callcheck: drawLines, closeDone, reset

void StageMenuList::draw(sceVif1Packet *packet, TexAreaList *texList)
{
    if (m_state == StageMenuList_Closed)
        return;
    m_printer->m_flags = 0;
    drawLines(packet, texList);
    m_lineCount = usedLines();

    if (m_state == StageMenuList_Closing) {
        if (closeDone()) {
            reset();
            m_state = StageMenuList_Closed;
        }
    } else {
        const StageMenuLine *from = lines();
        m_page = (m_page + 1) & 1;
        StageMenuLine *to = lines();
        for (int i = 0; i < kStageMenuLines; i++) {
            to[i] = from[i];
            to[i].asked = 0;
        }
        m_laidOut = 0;
    }
    if (m_lineCount != 0 && m_state == StageMenuList_Opening)
        m_state = StageMenuList_Open;
}

// FS2 0x00209260
// callcheck: layout, draw, sceVif1PkCnt, sceVif1PkAlign, sceVif1PkOpenDirectCode, sceVif1PkOpenGifTag, sceVif1PkCloseGifTag, sceVif1PkCloseDirectCode, sceVif1PkCloseDmaTag (PC: drawText's YamiPan_DrawUi*)

void StageMenuList::drawLines(sceVif1Packet *packet, TexAreaList *texList)
{
    if (m_state != StageMenuList_Closing)
        layout();
    StageMenuLine *line = lines();

    for (int i = 0; i < kStageMenuLines; i++) {
        if (!line[i].isFree())
            line[i].fade.advance();
    }

    YamiPan_DrawUiSqueeze(packet, 1);   // PC: the menu keeps its shape at any aspect
    for (int i = 0; i < kStageMenuLines; i++) {
        if (!line[i].isFree())
            line[i].fade.draw(packet, texList, m_plate);
    }

    WideFontPrinter *printer = m_printer;
    bool first = true;
    for (int i = 0; i < kStageMenuLines; i++) {
        const StageMenuFade &fade = line[i].fade;
        if (line[i].isFree() || fade.color.track == nullptr || fade.color.track->showsText == 0
            || line[i].text == nullptr)
            continue;
        const float x = fade.x + kTextX;
        const float y = fade.y;
        printer->m_flags = TextPrinterFlag_CenterY;
        if (first) {
            printer->uploadTexture(packet);
            printer->m_spacing = kTextSpacing;
            printer->m_lineSpacing = 0;
            printer->m_packet = packet;
            printer->setDrawState(packet, true);
            first = false;
        }
        printer->m_rgba = trackColor(fade.color) & kTextAlphaMask;
        printer->printScaledText(kTextScale, (int)x, (int)(y + kTextY), line[i].text);
        printer->m_flags = 0;
    }
    if (!first)
        printer->m_packet = nullptr;
    YamiPan_DrawUiSqueeze(packet, 0);
}

// FS2 0x002096c0

int StageMenuList::usedLines()
{
    int used = 0;
    const StageMenuLine *line = lines();
    for (int i = 0; i < kStageMenuLines; i++) {
        if (!line[i].isFree())
            used++;
    }
    return used;
}

int StageMenuList::selectedIndex()
{
    const StageMenuLine *line = lines();
    for (int i = 0; i < kStageMenuLines; i++) {
        if (line[i].selected != 0)
            return i;
    }
    return -1;
}

// FS2 0x00209710
// callcheck: layout, close

const u_short *StageMenuList::pick()
{
    if (m_state == StageMenuList_Closed)
        return nullptr;
    if (m_state != StageMenuList_Closing)
        layout();
    const int selected = selectedIndex();
    if (selected < 0)
        return nullptr;
    StageMenuLine &line = lines()[selected];
    if (line.state == StageMenuLine_Leaving || line.enabled == 0)
        return nullptr;
    if (m_state != StageMenuList_Closing) {
        m_state = StageMenuList_Closing;
        close(true);
    }
    return line.isFree() ? nullptr : line.text;
}

// What 0x209870 and 0x209b00 share: they differ by the step.

bool StageMenuList::moveSelection(int step)
{
    if (m_state == StageMenuList_Closed || m_state == StageMenuList_Closing)
        return false;
    layout();
    const int selected = selectedIndex();
    if (selected < 0)
        return false;

    StageMenuLine *line = lines();
    int next = selected;
    do {
        next += step;
        if (next >= kStageMenuLines)
            next = 0;
        else if (next < 0)
            next = kStageMenuLines - 1;
        if (next == selected)
            return false;
    } while (line[next].shown == 0);

    line[selected].setSelected(false);
    line[selected].fade.setLook(line[selected].restLook());
    line[next].setSelected(true);
    line[next].fade.setLook(line[next].restLook());
    return true;
}

// FS2 0x00209870
// callcheck: layout, setLook, setLook, setLook, setLook, setLook, setLook, setLook, setLook

bool StageMenuList::selectNext()
{
    return moveSelection(1);
}

// FS2 0x00209b00
// callcheck: layout, setLook, setLook, setLook, setLook, setLook, setLook, setLook, setLook

bool StageMenuList::selectPrev()
{
    return moveSelection(-1);
}

// FS2 0x00209d90
// callcheck: wstrcmp

bool StageMenuList::addLine(const u_short *text, int order, bool enabled)
{
    if (m_state == StageMenuList_Closed || m_state == StageMenuList_Closing)
        return false;

    StageMenuLine *page = lines();
    StageMenuLine *line = nullptr;
    bool added = false;
    for (int i = 0; i < kStageMenuLines && line == nullptr; i++) {
        if (!page[i].isFree() && wstrcmp(page[i].text, text) == 0)
            line = &page[i];
    }
    if (line == nullptr) {
        for (int i = 0; i < kStageMenuLines && line == nullptr; i++) {
            if (page[i].isFree())
                line = &page[i];
        }
        if (line == nullptr)
            return false;       // PC: the ROM writes through null with all 16 taken
        line->clear();
        line->state = StageMenuLine_New;
        added = true;
    }

    line->changed = 0;
    if (line->text == nullptr)
        line->changed = 1;
    line->text = text;
    line->order = order;
    if ((u_char)enabled != line->enabled)
        line->changed = 1;
    line->enabled = enabled;
    line->shown = 1;
    line->asked = 1;
    return added;
}

// FS2 0x00209f90
// callcheck: sort, setLook, placeAtRow, aimAtRow, frames, startMove, settle

void StageMenuList::layout()
{
    if (m_laidOut != 0 || m_state == StageMenuList_Closing)
        return;

    bool firstEnabled = m_state == StageMenuList_Opening;
    sort();

    StageMenuLine *page = lines();
    int rows = 0;
    const auto takeRow = [&rows](StageMenuLine &line) {
        line.rowChanged = line.row != rows;
        line.row = rows;
        rows++;
    };

    for (int i = 0; i < kStageMenuLines; i++) {
        StageMenuLine &line = page[i];
        StageMenuFade &fade = line.fade;
        if (line.asked == 1) {
            if (line.state == StageMenuLine_Shown) {
                line.setShown(true);
                fade.setLook(line.restLook());
                line.slidesIn = 0;
                takeRow(line);
            } else if (line.state == StageMenuLine_New || line.state == StageMenuLine_Gone) {
                line.state = StageMenuLine_Shown;
                line.setShown(true);
                if (m_state == StageMenuList_Opening) {
                    if (line.enabled == 0) {
                        fade.setLook(StageMenuLook_Off, StageMenuLook_Closed);
                        line.setSelected(false);
                    } else if (firstEnabled) {
                        line.setSelected(true);
                        fade.setLook(StageMenuLook_Selected, StageMenuLook_Closed);
                        firstEnabled = false;
                    } else {
                        fade.setLook(StageMenuLook_Normal, StageMenuLook_Closed);
                        line.setSelected(false);
                    }
                } else {
                    fade.setLook(line.enabled == 0 ? StageMenuLook_Off : StageMenuLook_Normal, StageMenuLook_Gone);
                    line.setSelected(false);
                }
                line.slidesIn = 1;
                takeRow(line);
            } else if (line.state == StageMenuLine_Leaving) {
                line.state = StageMenuLine_Shown;
                fade.setLook(line.enabled == 0 ? StageMenuLook_Off : StageMenuLook_Normal);
                line.slidesIn = 1;
                takeRow(line);
            }
        } else if (line.state == StageMenuLine_Gone) {
            line.setShown(false);
            if (fade.finished())
                line.clear();
            else
                fade.setLook(StageMenuLook_Gone);
        } else if (line.state == StageMenuLine_Leaving || line.state == StageMenuLine_Shown) {
            line.setShown(false);
            if (line.selected == 0) {
                fade.setLook(StageMenuLook_Gone);
                line.state = StageMenuLine_Gone;
            } else {
                line.state = StageMenuLine_Leaving;
                fade.setLook(StageMenuLook_SelectedOff);
                line.slidesIn = 0;
                takeRow(line);
            }
        }
    }

    for (int i = 0; i < kStageMenuLines; i++) {
        StageMenuLine &line = page[i];
        if (line.isFree())
            continue;
        if (line.rowChanged == 0 && line.rows == rows)
            continue;
        line.rows = rows;
        if (line.slidesIn == 0) {
            line.fade.settle();
            line.fade.aimAtRow(line.row, line.rows);
        } else {
            line.fade.placeAtRow(line.row, line.rows);
            line.fade.aimAtRow(line.row, line.rows);
        }
        line.fade.startMove(line.fade.color.frames());
    }

    if (selectedIndex() < 0) {
        for (int i = 0; i < kStageMenuLines; i++) {
            if (page[i].shown != 0) {
                page[i].setSelected(true);
                break;
            }
        }
    }
    m_laidOut = 1;
}

// FS2 0x0020a870

void StageMenuList::sort()
{
    constexpr int kFreeOrder = 0x7fffffff;
    StageMenuLine *page = lines();
    const auto key = [](const StageMenuLine &line) { return line.isFree() ? kFreeOrder : line.order; };
    for (int i = 0; i < kStageMenuLines - 1; i++) {
        for (int j = i + 1; j < kStageMenuLines; j++) {
            if (key(page[j]) < key(page[i])) {
                const StageMenuLine kept = page[i];
                page[i] = page[j];
                page[j] = kept;
            }
        }
    }
}

// ---- the menu ------------------------------------------------------------

// The player may use the menu: it is allowed, and the action leaves the
// player free (the list wantsMapMenu has).
static bool menuAllowed()
{
    const int action = gStageScene->playerAction();
    return gStageScene->showFlags->flag[kShowFlagStageMenu] != 0 && actionAllowsMenu(action);
}

static bool menuActionPressed(const GamePad &pad, int action)
{
    return (pad.m_pressed & bitMask(pad.actionButton(action))) != 0;
}

// FS2 0x002308a0
// callcheck: operator_new, StageMenuList

StageMenu::StageMenu(WideFontPrinter *printer, int arg)
{
    list = new StageMenuList(printer, arg);
}

// FS2 0x00230820
// callcheck: ~StageMenuList, operator_delete

void StageMenu::destroy()
{
    delete list;
    delete this;
}

// FS2 0x00230930: the base's deleting destructor (vtable 0x3164d0), nothing of its own.

// FS2 0x00230800
// callcheck: addLine

void StageMenu::addLine(const u_short *text, int order, bool enabled)
{
    list->addLine(text, order, enabled);
}

// FS2 0x00230810
// callcheck: setData

void StageMenu::setData(void *data)
{
    list->setData(data);
}

// FS2 0x002303c0
// callcheck: draw

void StageMenu::draw(sceVif1Packet *packet, TexAreaList *texList)
{
    if (state != StageMenu_Closed)
        list->draw(packet, texList);
}

// FS2 0x00230400
// callcheck: reset, close, playerAction, getInputFocus, bitMask, pick, bitMask, bitMask, selectPrev, selectNext, playerAction, bitMask, usedLines

void StageMenu::update()
{
    pendingOn = 0;
    if (request == StageMenuRequest_Close) {
        if (state == StageMenu_Open) {
            state = StageMenu_Closing;
            if (list->m_state != StageMenuList_Closed && list->m_state != StageMenuList_Closing) {
                list->m_state = StageMenuList_Closing;
                list->close(false);
            }
            request = StageMenuRequest_None;
        } else if (state != StageMenu_Opening) {
            request = StageMenuRequest_None;
        }
    } else if (request == StageMenuRequest_Open) {
        if (state == StageMenu_Closed) {
            state = StageMenu_Opening;
            list->m_lineCount = 0;
            if (list->m_state == StageMenuList_Closed) {
                list->reset();
                list->m_state = StageMenuList_Opening;
            }
            request = StageMenuRequest_None;
        } else if (state != StageMenu_Closing) {
            request = StageMenuRequest_None;
        }
    }
    pending = nullptr;

    const GamePad &pad = gGameSystem->m_pad0;
    switch (state) {
    case StageMenu_Open:
        if (!menuAllowed()) {
            request = StageMenuRequest_Close;
        } else {
            const bool focused = gStageScene->getInputFocus() == this;
            if (focused && menuActionPressed(pad, PadAction_Use)) {
                pending = list->pick();
                if (pending != nullptr) {
                    pendingOn = 1;
                    state = StageMenu_Closing;
                }
            } else if ((focused && menuActionPressed(pad, PadAction_Squat))
                       || menuActionPressed(pad, PadAction_Menu)) {
                request = StageMenuRequest_Close;
            } else if (focused) {
                int step = 0;
                if ((pad.m_repeat & SCE_PADLup) != 0)
                    step = -1;
                if ((pad.m_repeat & SCE_PADLdown) != 0)
                    step++;
                if (step < 0)
                    list->selectPrev();
                else if (step > 0)
                    list->selectNext();
            }
        }
        break;
    case StageMenu_Closed:
        if (menuAllowed() && menuActionPressed(pad, PadAction_Menu))
            request = StageMenuRequest_Open;
        break;
    case StageMenu_Closing:
        if (list->m_state == StageMenuList_Closed)
            state = StageMenu_Closed;
        break;
    case StageMenu_Opening:
        state = StageMenu_Open;
        break;
    default:
        break;
    }
    if (state == StageMenu_Open && list->m_state != StageMenuList_Closed)
        list->usedLines();      // the answer goes out of the ROM's update, unused
}
