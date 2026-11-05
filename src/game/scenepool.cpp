// game/scenepool.cpp -- the stage's lines of text at the foot of the screen (FS2 0x288c00..0x289cc0):
// four slots a frame, 0 and 1 the voice's subtitle (they follow the option), 2 and 3 the notices
// (PauseMenuState), on a soft dark strip the ROM tiles from the init pack's item InitItem_PoolImage.

#include "game/gamehooks.h"

#include <string.h>

#include "game/fontdata.h"
#include "game/gsutil.h"
#include "game/sysdata.h"
#include "sdk/libgraph.h"
#include "yamipan/rendering/yamipan_drawui.h"

namespace {

constexpr u_int kSlotRgba = 0x80606060;
constexpr int   kFirstSlotWithoutSubtitles = 2;
constexpr short kPrinterRestingOrigin = 0x700;
constexpr int   kSlotLines = 4;                 // of one slot's text

// The strip: a cell of the image per corner mask, the corners those the
// lines of text cover around the cell.
constexpr int      kCellWidth   = 20;
constexpr int      kCellHeight  = 24;
constexpr int      kFixedOne    = 16;           // the GS window's 12.4
constexpr uint64_t kStripPrim   = 0x15e;        // textured, blended sprites
constexpr uint8_t  kStripRgba[4] = { 0x10, 0x10, 0x10, 0x80 };
constexpr uint32_t kStripZ      = 0xff0fffff;

// data 0x2fb4b0: each mask's cell in the image, 16ths of a pixel. Mask 0 draws none.
constexpr u_short kStripCells[16][2] = {
    { 320, 384 }, { 1600, 0 }, { 1280, 0 }, { 320, 0 }, { 960, 0 }, { 3520, 0 }, { 320, 384 }, { 2880, 0 },
    { 640, 0 }, { 320, 384 }, { 3200, 0 }, { 2560, 0 }, { 0, 0 }, { 2240, 0 }, { 1920, 0 }, { 0, 384 },
};

enum StripCorner
{
    StripCorner_TopLeft     = 1 << 0,
    StripCorner_TopRight    = 1 << 1,
    StripCorner_BottomLeft  = 1 << 2,
    StripCorner_BottomRight = 1 << 3,
    StripCorner_Top    = StripCorner_TopLeft | StripCorner_TopRight,
    StripCorner_Bottom = StripCorner_BottomLeft | StripCorner_BottomRight,
};

// The cells a line of width pixels covers, even so that it centres.
int cellsOfWidth(int width)
{
    int cells = (width + kCellWidth - 1) / kCellWidth;
    return (cells & 1) != 0 ? cells + 1 : cells;
}

// A run of cells centred in the row: mask inside, the edges beside it.
void markRun(u_char *row, int rowCells, int cells, int mask, int leftEdge, int rightEdge)
{
    if (cells == 0)
        return;
    const int first = (rowCells - cells) >> 1;
    for (int i = first; i < first + cells; i++)
        row[i] |= (u_char)mask;
    row[first - 1] |= (u_char)leftEdge;
    row[first + cells] |= (u_char)rightEdge;
}

} // namespace

// FS2 0x00289be0
// FS2 0x002895c0
// callcheck: FUN_002895c0, operator_new[], operator_new[], operator_delete[], operator_new[]

ScenePool::ScenePool(u_int count, int size, int size2)
    : m_count((int)count), m_originX(size), m_originY(size2), m_image(nullptr), m_printer(nullptr)
{
    m_widths = new int[count]();
    m_slots = new const u_short *[count];
    m_colours = new u_int[count];
    for (u_int i = 0; i < count; i++) {
        m_slots[i] = nullptr;
        m_colours[i] = kSlotRgba;
    }
}

// FS2 0x00289b60
// FS2 0x00289530
// callcheck: FUN_00289530, operator_delete[], operator_delete[], operator_delete

ScenePool::~ScenePool()
{
    delete m_image;
    delete[] m_widths;
    delete[] m_slots;
    delete[] m_colours;
}

// FS2 0x002894b0
// callcheck: operator_new

void ScenePool::setImage(const void *image)
{
    delete m_image;
    m_image = image != nullptr ? new GsTexture(image) : nullptr;
}

// FS2 0x002896a0

void ScenePool::setColour(u_int rgba, int index)
{
    if (index < 0) {
        for (int i = 0; i < m_count; i++)
            m_colours[i] = rgba;
    } else if (index < m_count) {
        m_colours[index] = rgba;
    }
}

// FS2 0x00289b20

int ScenePool::setSlot(int index, const u_short *text)
{
    if (index >= m_count)
        return -1;
    m_slots[index] = text;
    return index;
}

// FS2 0x00289b50

void ScenePool::setPrinter(WideFontPrinter *printer)
{
    m_printer = printer;
}

// One row of cells: the corners of the lines above, here and below.
// FS2 0x00289180

static void markRow(u_char *row, int rowCells, int above, int here, int below)
{
    memset(row, 0, rowCells);
    markRun(row, rowCells, above, StripCorner_Top, StripCorner_TopRight, StripCorner_TopLeft);
    markRun(row, rowCells, here, StripCorner_Top | StripCorner_Bottom,
            StripCorner_TopRight | StripCorner_BottomRight, StripCorner_TopLeft | StripCorner_BottomLeft);
    markRun(row, rowCells, below, StripCorner_Bottom, StripCorner_BottomRight, StripCorner_BottomLeft);
}

// The strip behind the widths of the frame's lines, then the widths cleared.
// FS2 0x00288c00
// callcheck: place, putTex0, upload, putTex0, sceVif1PkCnt, sceVif1PkAddCode, sceVif1PkOpenDirectCode, gfxUiPresetNormal, sceVif1PkOpenGifTag, sceVif1PkAddGsAD, sceVif1PkCloseGifTag, sceVif1PkOpenGifTag, sceVif1PkAddGsData, sceVif1PkAddGsData, sceVif1PkCloseGifTag, sceVif1PkOpenGifTag, operator_new[], FUN_00289180, sceVif1PkAddGsData, sceVif1PkAddGsData, sceVif1PkAddGsData, sceVif1PkAddGsData, operator_delete[], sceVif1PkCloseGifTag, gfxUiPresetDrawEnv, sceVif1PkCloseDirectCode, sceVif1PkCloseDmaTag

void ScenePool::drawStrip(sceVif1Packet *packet)
{
    int widest = 0;
    for (int i = 0; i < m_count; i++) {
        if (widest < m_widths[i])
            widest = m_widths[i];
    }
    if (widest != 0 && m_image != nullptr) {
        const int rowCells = cellsOfWidth(widest) + 2;
        const int left = (m_originX - rowCells * kCellWidth / 2) * kFixedOne;
        gfxUiPresetNormal(packet);
        YamiPan_DrawUiTexture(packet, m_image);
        YamiPan_DrawUiReg(packet, SCE_GS_TEX1_1, 0);
        YamiPan_DrawUiPrim(packet, kStripPrim, kStripRgba);
        YamiPan_DrawUiBegin(packet, YAMIPAN_UIV_UV);
        u_char *row = new u_char[rowCells];
        const auto cellsOf = [this](int line) {
            return line < 0 || line >= m_count ? 0 : cellsOfWidth(m_widths[line]);
        };
        for (int line = -1; line <= m_count; line++) {
            markRow(row, rowCells, cellsOf(line - 1), cellsOf(line), cellsOf(line + 1));
            const int top = (line * kCellHeight + m_originY) * kFixedOne;
            for (int cell = 0; cell < rowCells; cell++) {
                if (row[cell] == 0)
                    continue;
                YamiPanUiVertex corner = {};
                corner.z = kStripZ;
                corner.u = kStripCells[row[cell]][0];
                corner.v = kStripCells[row[cell]][1];
                corner.x = (uint16_t)(left + cell * kCellWidth * kFixedOne);
                corner.y = (uint16_t)top;
                YamiPan_DrawUiVertex(packet, &corner);
                corner.u = (uint16_t)(corner.u + kCellWidth * kFixedOne);
                corner.v = (uint16_t)(corner.v + kCellHeight * kFixedOne);
                corner.x = (uint16_t)(corner.x + kCellWidth * kFixedOne);
                corner.y = (uint16_t)(top + kCellHeight * kFixedOne);
                YamiPan_DrawUiVertex(packet, &corner);
            }
        }
        delete[] row;
        gfxUiPresetDrawEnv(packet);
    }
    for (int i = 0; i < m_count; i++)
        m_widths[i] = 0;
}

// The strip, then each slot on its own line, centred on the origin, in the
// slot's colour. Subtitles switched off leave out the first two.
// FS2 0x00289800
// callcheck: getGameOptions, measureLines, FUN_00288c00, setDrawState, uploadTexture, lineHeight, print, sceVif1PkAlign, sceVif1PkCloseDirectCode, sceVif1PkCloseDmaTag, sceVif1PkCloseGifTag, sceVif1PkCnt, sceVif1PkOpenDirectCode, sceVif1PkOpenGifTag (PC: the host draw module)

void ScenePool::draw(sceVif1Packet *packet, TexAreaList *texList)
{
    (void)texList;      // PC: the host binds textures by owner
    if (m_printer == nullptr)
        return;
    const int first = getGameOptions()->subtitles == 0 ? kFirstSlotWithoutSubtitles : 0;
    for (int i = 0; i < m_count; i++)
        m_widths[i] = 0;
    for (int i = first; i < m_count; i++) {
        if (m_slots[i] == nullptr || m_slots[i][0] == 0)
            continue;
        int widths[kSlotLines];
        int lines = m_printer->measureLines(widths, kSlotLines, reinterpret_cast<const char *>(m_slots[i]));
        if (lines > kSlotLines)
            lines = kSlotLines;
        for (int line = 0; line < lines && i + line < m_count; line++)
            m_widths[i + line] = widths[line];
    }
    drawStrip(packet);

    const u_int flags = m_printer->m_flags;
    m_printer->m_originX = (short)m_originX;
    m_printer->m_originY = (short)m_originY;
    m_printer->m_flags = TextPrinterFlag_CenterX;
    m_printer->uploadTexture(packet);
    m_printer->m_packet = packet;
    m_printer->setDrawState(packet, false);
    for (int i = first; i < m_count; i++) {
        if (m_slots[i] == nullptr)
            continue;
        m_printer->m_rgba = m_colours[i];
        m_printer->printText(0, i * m_printer->lineHeight(), m_slots[i]);
    }
    m_printer->m_packet = nullptr;
    m_printer->m_originX = kPrinterRestingOrigin;
    m_printer->m_originY = kPrinterRestingOrigin;
    m_printer->m_flags = flags;
}
