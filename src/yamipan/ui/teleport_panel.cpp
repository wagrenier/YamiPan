#include "yamipan/ui/teleport_panel.h"

#include <cfloat>
#include <cstdio>
#include <format>
#include <string>

#include <imgui.h>

#include "game/teleport.h"

namespace yamipan::ui
{
namespace
{

constexpr ImVec2 kWindowSize(640.0f, 680.0f);
constexpr ImVec4 kPendingColour(0.94f, 0.58f, 0.36f, 1.0f);
constexpr ImGuiTableFlags kTableFlags = ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp;
constexpr float kPartsColumnWidth = 110.0f;

// StageKind's order, after the "keep" of index 0.
constexpr const char *kDifficulties[] = {"As it is", "Easy", "Normal", "Hard"};

const char *kindLabel(int kind)
{
    switch (kind)
    {
    case Teleport_FileEvent:
        return "Event";
    case Teleport_SimpleEvent:
        return "Scripted";
    case Teleport_Movie:
        return "Movie";
    default:
        return "";
    }
}

class TeleportPanel final : public Panel
{
public:
    void drawMenu() override
    {
        if (!ImGui::BeginMenu("Debug"))
            return;
        ImGui::MenuItem("Teleport...", "F2", &m_visible);
        ImGui::EndMenu();
    }

    void drawWindow() override
    {
        if (!m_visible)
            return;
        ImGui::SetNextWindowSize(kWindowSize, ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Teleport", &m_visible))
        {
            drawStatus();
            drawOptions();
            ImGui::Separator();
            if (ImGui::BeginChild("##places"))
            {
                for (int i = 0; i < teleportStageCount(); i++)
                    drawStage(teleportStage(i));
                drawFilms();
            }
            ImGui::EndChild();
        }
        ImGui::End();
    }

    bool handleEvent(const SDL_Event &event) override
    {
        if (event.type != SDL_EVENT_KEY_DOWN || event.key.repeat || event.key.scancode != SDL_SCANCODE_F2)
            return false;
        m_visible = !m_visible;
        return true;
    }

private:
    void drawStatus()
    {
        const TeleportStatus status = teleportStatus();
        if (status.pending)
            ImGui::TextColored(kPendingColour, "Going there: the game leaves when it next can...");
        else if (!status.inStage)
            ImGui::TextDisabled("In the menus.");
        else if (status.event[0] != '\0')
            ImGui::Text("Stage %02d part %d, event %s", status.stage, status.variant + 1, status.event);
        else
            ImGui::Text("Stage %02d part %d", status.stage, status.variant + 1);
    }

    void drawOptions()
    {
        ImGui::SetNextItemWidth(120.0f);
        ImGui::Combo("Difficulty", &m_difficulty, kDifficulties, IM_ARRAYSIZE(kDifficulties));
        ImGui::SameLine();
        ImGui::Checkbox("Skip the opening movie and briefing", &m_skipIntro);
        ImGui::Checkbox("Run events from their trigger", &m_fromTrigger);
        ImGui::SetItemTooltip("Puts the player on what starts the event and runs that script, so what it does "
                              "after the event runs too. Off: the event just plays on the new stage.");

        // F2, a name, Enter. The last name stays, selected: F2, Enter plays it again.
        if (ImGui::IsWindowAppearing())
            ImGui::SetKeyboardFocusHere();
        ImGui::SetNextItemWidth(-FLT_MIN);
        const bool entered = ImGui::InputTextWithHint("##filter", "Filter: d0705, sd_mt, AREA_AC02 ... Enter goes to the first",
                                                      m_filter.InputBuf, IM_ARRAYSIZE(m_filter.InputBuf),
                                                      ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
        m_filter.Build();
        if (entered && m_filter.IsActive())
            goToFirstMatch();
    }

    void goToFirstMatch()
    {
        for (int i = 0; i < teleportEventCount(); i++)
        {
            const TeleportEvent &event = teleportEvent(i);
            if (!passes(event))
                continue;
            const int variant = (event.parts & 1) != 0 ? 0 : 1;
            go(static_cast<TeleportKind>(event.kind), event.stage, variant, event.branch, event.name);
            return;
        }
    }

    bool passes(const TeleportEvent &event) const
    {
        const std::string text = std::format("{} {}", event.name, event.startedBy);
        return m_filter.PassFilter(text.c_str());
    }

    void drawStage(const TeleportStage &stage)
    {
        int shown = 0;
        for (int i = 0; i < teleportEventCount(); i++)
        {
            const TeleportEvent &event = teleportEvent(i);
            shown += event.stage == stage.stage && passes(event);
        }
        if (m_filter.IsActive() && shown == 0)
            return;

        if (m_filter.IsActive())
            ImGui::SetNextItemOpen(true);
        const TeleportStatus status = teleportStatus();
        const bool here = status.inStage && status.stage == stage.stage;
        const std::string label = std::format("Stage {:02}  ({} events){}##stage{}", stage.stage, shown,
                                              here ? "  - you are here" : "", stage.stage);
        if (!ImGui::CollapsingHeader(label.c_str()))
            return;

        if (!ImGui::BeginTable("##events", 4, kTableFlags))
            return;
        ImGui::TableSetupColumn("Event");
        ImGui::TableSetupColumn("Kind");
        ImGui::TableSetupColumn("Started by");
        ImGui::TableSetupColumn("Go", ImGuiTableColumnFlags_WidthFixed, kPartsColumnWidth);
        ImGui::TableHeadersRow();

        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::TextDisabled("The stage itself");
        ImGui::TableSetColumnIndex(3);
        drawGoButtons(stage.parts, std::format("stage{}", stage.stage),
                      [&](int variant) { go(Teleport_Stage, stage.stage, variant, 0, ""); });

        for (int i = 0; i < teleportEventCount(); i++)
        {
            const TeleportEvent &event = teleportEvent(i);
            if (event.stage != stage.stage || !passes(event))
                continue;
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(event.name);
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(kindLabel(event.kind));
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(event.startedBy[0] != '\0' ? event.startedBy : "(no script)");
            ImGui::TableNextColumn();
            drawGoButtons(event.parts, event.name, [&](int variant) {
                go(static_cast<TeleportKind>(event.kind), event.stage, variant, event.branch, event.name);
            });
        }
        ImGui::EndTable();
    }

    template <class Action>
    void drawGoButtons(int parts, const std::string &id, Action action)
    {
        for (int variant = 0; variant < kTeleportParts; variant++)
        {
            if ((parts & (1 << variant)) == 0)
                continue;
            ImGui::SameLine(0.0f, variant == 0 ? 0.0f : -1.0f);
            const std::string label = std::format("Part {}##{}", variant + 1, id);
            if (ImGui::SmallButton(label.c_str()))
                action(variant);
        }
    }

    void drawFilms()
    {
        if (m_filter.IsActive() || !ImGui::CollapsingHeader("Films (the navigator's nodes)"))
            return;
        for (int i = 0; i < teleportFilmCount(); i++)
        {
            const int node = teleportFilm(i);
            if (i % kFilmsPerRow != 0)
                ImGui::SameLine();
            const std::string label = std::format("{}##film", node);
            if (ImGui::SmallButton(label.c_str()))
                go(Teleport_Stage, node, 0, 0, "");
        }
    }

    // Closes the window: the game reads a released pad while it has the keyboard.
    void go(TeleportKind kind, int stage, int variant, int branch, const char *event)
    {
        TeleportRequest request{};
        request.kind = kind;
        request.stage = stage;
        request.variant = variant;
        request.difficulty = m_difficulty - 1; // kTeleportKeepDifficulty is -1
        request.branch = branch;
        request.skipIntro = m_skipIntro;
        request.fromTrigger = m_fromTrigger;
        std::snprintf(request.event, sizeof(request.event), "%s", event);
        teleportRequest(request);
        m_visible = false;
    }

    static constexpr int kFilmsPerRow = 13;

    bool m_visible = false;
    int m_difficulty = 0;
    bool m_skipIntro = true;
    bool m_fromTrigger = true;
    ImGuiTextFilter m_filter;
};

} // namespace

std::unique_ptr<Panel> createTeleportPanel()
{
    return std::make_unique<TeleportPanel>();
}

} // namespace yamipan::ui
