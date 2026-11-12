#include "yamipan/ui/panels.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cfloat>
#include <cstdlib>
#include <format>
#include <mutex>
#include <optional>
#include <string>

#include <imgui.h>
#include <implot.h>
#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_misc.h>

#include "yamipan/core/application.h"
#include "yamipan/core/config.h"
#include "yamipan/core/paths.h"
#include "yamipan/core/window.h"
#include "yamipan/io/input_system.h"
#include "yamipan/rendering/renderer.h"
#include "yamipan/ui/teleport_panel.h"

namespace yamipan::ui
{
namespace
{

constexpr ImVec4 kWarningColour(0.94f, 0.58f, 0.36f, 1.0f);
constexpr ImVec4 kGoodColour(0.45f, 0.80f, 0.45f, 1.0f);

void openInShell(const std::filesystem::path &folder)
{
    SDL_OpenURL(std::format("file:///{}", pathToUtf8(folder)).c_str());
}

// One enumerated setting as radio items in a menu.
template <class Enum>
bool radioItems(Enum &value, std::initializer_list<std::pair<const char *, Enum>> choices)
{
    bool changed = false;
    for (const auto &[label, choice] : choices)
    {
        if (ImGui::MenuItem(label, nullptr, value == choice) && value != choice)
        {
            value = choice;
            changed = true;
        }
    }
    return changed;
}

// ---- Settings > Display, Graphics, Audio -----------------------------------------------------------

class SettingsPanel final : public Panel
{
public:
    explicit SettingsPanel(Application &application)
        : m_application(application), m_config(application.config())
    {
    }

    void drawMenu() override
    {
        if (!ImGui::BeginMenu("Settings"))
            return;
        drawDisplayMenu();
        drawGraphicsMenu();
        drawAudioMenu();
        ImGui::EndMenu();
    }

private:
    void drawDisplayMenu()
    {
        if (!ImGui::BeginMenu("Display"))
            return;
        DisplaySettings &display = m_config.display;

        ImGui::TextDisabled("Window");
        const WindowMode mode = m_application.window().mode();
        if (ImGui::MenuItem("Windowed", "Alt+Enter", mode == WindowMode::Windowed))
            setWindowMode(WindowMode::Windowed);
        if (ImGui::MenuItem("Borderless fullscreen", "Alt+Enter", mode == WindowMode::Borderless))
            setWindowMode(WindowMode::Borderless);

        ImGui::Separator();
        ImGui::TextDisabled("Aspect ratio");
        if (radioItems(display.aspectMode, {{"Auto (window)", AspectMode::Auto},
                                            {"4:3", AspectMode::Ratio4x3},
                                            {"16:9", AspectMode::Ratio16x9},
                                            {"16:10", AspectMode::Ratio16x10},
                                            {"21:9", AspectMode::Ratio21x9},
                                            {"Custom", AspectMode::Custom}}))
        {
            m_config.save();
        }
        ImGui::SetNextItemWidth(180.0f);
        if (ImGui::SliderFloat("Custom ratio", &display.aspectRatio, 0.5f, 5.0f, "%.3f"))
            display.aspectMode = AspectMode::Custom;
        saveAfterEdit();

        const rendering::PictureLayout layout = m_application.renderer().layout();
        ImGui::TextDisabled("  picture %dx%d at +%d+%d", layout.picture.width, layout.picture.height, layout.picture.x,
                            layout.picture.y);

        ImGui::Separator();
        ImGui::TextDisabled("Grade");
        gradeSlider("Brightness", display.gradeBrightness);
        gradeSlider("Contrast", display.gradeContrast);
        gradeSlider("Gamma", display.gradeGamma);
        gradeSlider("Saturation", display.gradeSaturation);
        if (ImGui::MenuItem("Reset grade"))
        {
            display.gradeBrightness = display.gradeContrast = display.gradeGamma = display.gradeSaturation = 1.0f;
            m_config.save();
        }
        ImGui::EndMenu();
    }

    void drawGraphicsMenu()
    {
        if (!ImGui::BeginMenu("Graphics"))
            return;
        DisplaySettings &display = m_config.display;

        ImGui::TextDisabled("Render resolution");
        if (radioItems(display.renderMode, {{"Match window", RenderResolution::MatchWindow},
                                            {"Native PS2", RenderResolution::NativePs2},
                                            {"Window scale", RenderResolution::WindowScale}}))
        {
            m_config.save();
        }
        ImGui::SetNextItemWidth(180.0f);
        ImGui::SliderFloat("Scale", &display.renderScale, 0.25f, 8.0f, "%.2fx");
        saveAfterEdit();

        const rendering::PictureLayout layout = m_application.renderer().layout();
        ImGui::TextDisabled("  rasterised at %dx%d", layout.renderWidth, layout.renderHeight);

        ImGui::Separator();
        ImGui::TextDisabled("Upscale filter");
        if (radioItems(display.upscaleFilter,
                       {{"Nearest", UpscaleFilter::Nearest}, {"Linear", UpscaleFilter::Linear}}))
        {
            m_config.save();
        }

        ImGui::Separator();
        ImGui::TextDisabled("Present");
        if (radioItems(display.presentMode, {{"VSync", PresentMode::VSync},
                                             {"Mailbox", PresentMode::Mailbox},
                                             {"Immediate", PresentMode::Immediate}}))
        {
            m_application.renderer().applyPresentMode();
            m_config.save();
        }

        ImGui::Separator();
        if (ImGui::Checkbox("Smooth motion", &display.smoothMotion))
            m_config.save();
        const rendering::Renderer::Stats stats = m_application.renderer().stats();
        ImGui::TextDisabled("  game %.1f fps, shown %.1f, blended %.1f", stats.gameRate, stats.presentRate,
                            stats.blendRate);
        ImGui::EndMenu();
    }

    void drawAudioMenu()
    {
        if (!ImGui::BeginMenu("Audio"))
            return;
        AudioSettings &audio = m_config.audio;
        bool changed = volumeSlider("Master", audio.master);
        changed |= volumeSlider("Music, voices, ambience", audio.music);
        changed |= volumeSlider("Effects", audio.effects);
        if (changed)
            m_application.applyAudioSettings();
        ImGui::EndMenu();
    }

    void setWindowMode(WindowMode mode)
    {
        m_application.window().setMode(mode);
        m_config.display.windowMode = mode;
        m_config.save();
    }

    void gradeSlider(const char *label, float &value)
    {
        ImGui::SetNextItemWidth(180.0f);
        ImGui::SliderFloat(label, &value, 0.25f, 2.0f, "%.2f");
        saveAfterEdit();
    }

    // True while the value moves: the sound follows the slider, the file is written when it is let go.
    bool volumeSlider(const char *label, int &value)
    {
        ImGui::SetNextItemWidth(180.0f);
        const bool changed = ImGui::SliderInt(label, &value, 0, 100, "%d%%");
        saveAfterEdit();
        return changed;
    }

    // A drag saves once, when it ends.
    void saveAfterEdit()
    {
        if (ImGui::IsItemDeactivatedAfterEdit())
            m_config.save();
    }

    Application &m_application;
    Config &m_config;
};

// ---- Controls ---------------------------------------------------------------------------------------

class ControlsPanel final : public Panel
{
public:
    explicit ControlsPanel(Application &application) : m_application(application), m_input(application.input()) {}

    void drawMenu() override
    {
        if (ImGui::BeginMenu("Input"))
        {
            ImGui::MenuItem("Controls...", nullptr, &m_visible);
            ImGui::EndMenu();
        }
    }

    // A key press while a cell waits is the binding: kept away from ImGui, whose keyboard nav would
    // otherwise re-activate the very button that armed the capture.
    bool handleEvent(const SDL_Event &event) override
    {
        if (!m_capture)
            return false;

        io::InputSource source;
        if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat)
        {
            if (event.key.scancode == SDL_SCANCODE_ESCAPE)
            {
                stopCapture();
                return true;
            }
            if (event.key.scancode != SDL_SCANCODE_BACKSPACE) // Backspace clears the slot
            {
                source.kind = io::InputSource::Kind::Key;
                source.code = static_cast<int>(event.key.scancode);
            }
        }
        else if (event.type == SDL_EVENT_GAMEPAD_BUTTON_DOWN)
        {
            source.kind = io::InputSource::Kind::Button;
            source.code = event.gbutton.button;
        }
        else if (event.type == SDL_EVENT_GAMEPAD_AXIS_MOTION)
        {
            // Half travel, so a stick resting off-centre does not bind itself.
            constexpr int kCaptureTravel = 16000;
            constexpr int kAxisThreshold = 8000;
            if (std::abs(event.gaxis.value) < kCaptureTravel)
                return false;
            source.kind = io::InputSource::Kind::Axis;
            source.code = event.gaxis.axis;
            source.direction = event.gaxis.value < 0 ? -1 : 1;
            source.threshold = kAxisThreshold;
        }
        else
        {
            return false;
        }

        applyCapture(source);
        return true;
    }

    void drawWindow() override
    {
        if (!m_visible)
        {
            stopCapture();
            return;
        }

        ImGui::SetNextWindowSize(ImVec2(620.0f, 520.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Controls", &m_visible))
        {
            drawGamepadChoice();
            ImGui::Separator();
            ImGui::TextDisabled("Click a cell, then press what it should be. Esc cancels, Backspace clears.");
            drawButtonTable();
            ImGui::Separator();
            drawStickTable();
            ImGui::Separator();
            ImGui::BeginDisabled(m_input.bindingsAreDefault());
            if (ImGui::Button("Reset to defaults"))
            {
                m_input.resetBindings();
                save();
            }
            ImGui::EndDisabled();
        }
        ImGui::End();
    }

private:
    enum class Target
    {
        Button,
        StickAnalog,
        StickNegative,
        StickPositive,
    };

    struct Capture
    {
        Target target;
        int index;
        int slot;

        bool operator==(const Capture &) const = default;
    };

    void drawGamepadChoice()
    {
        const auto active = m_input.activeGamepad();
        ImGui::Text("Gamepad: %s", active ? active->name.c_str() : "none");

        const std::string preferred = m_input.preferredGamepad();
        ImGui::SetNextItemWidth(320.0f);
        if (ImGui::BeginCombo("Use", preferred.empty() ? "First connected" : preferred.c_str()))
        {
            if (ImGui::Selectable("First connected", preferred.empty()))
            {
                m_input.setPreferredGamepad({});
                save();
            }
            for (const io::InputSystem::GamepadInfo &gamepad : m_input.gamepads())
            {
                if (ImGui::Selectable(gamepad.name.c_str(), preferred == gamepad.guid))
                {
                    m_input.setPreferredGamepad(gamepad.guid);
                    save();
                }
            }
            ImGui::EndCombo();
        }
    }

    void drawButtonTable()
    {
        if (!ImGui::BeginTable("buttons", 1 + io::kSourcesPerButton, ImGuiTableFlags_RowBg))
            return;
        for (std::size_t button = 0; button < io::kPadButtonCount; button++)
        {
            const auto pad = static_cast<io::PadButton>(button);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(io::padButtonName(pad).data());

            const io::ButtonBinding binding = m_input.buttonBinding(pad);
            for (std::size_t slot = 0; slot < binding.size(); slot++)
            {
                ImGui::TableNextColumn();
                drawCell(binding[slot], {Target::Button, static_cast<int>(button), static_cast<int>(slot)});
            }
        }
        ImGui::EndTable();
    }

    void drawStickTable()
    {
        if (!ImGui::BeginTable("sticks", 4, ImGuiTableFlags_RowBg))
            return;
        ImGui::TableSetupColumn("Stick");
        ImGui::TableSetupColumn("Axis");
        ImGui::TableSetupColumn("Pull down");
        ImGui::TableSetupColumn("Pull up");
        ImGui::TableHeadersRow();
        for (std::size_t stick = 0; stick < io::kPadStickCount; stick++)
        {
            const auto pad = static_cast<io::PadStick>(stick);
            const io::StickBinding binding = m_input.stickBinding(pad);
            const int index = static_cast<int>(stick);

            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(io::padStickName(pad).data());
            ImGui::TableNextColumn();
            drawCell(binding.analog, {Target::StickAnalog, index, 0});
            ImGui::TableNextColumn();
            drawCell(binding.negative, {Target::StickNegative, index, 0});
            ImGui::TableNextColumn();
            drawCell(binding.positive, {Target::StickPositive, index, 0});
        }
        ImGui::EndTable();
    }

    void drawCell(const io::InputSource &source, const Capture &cell)
    {
        ImGui::PushID(static_cast<int>(cell.target) * 1000 + cell.index * 10 + cell.slot);
        if (m_capture == cell)
        {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.55f, 0.16f, 0.18f, 1.0f));
            if (ImGui::Button("press...", ImVec2(-FLT_MIN, 0.0f)))
                stopCapture();
            ImGui::PopStyleColor();
        }
        else
        {
            const std::string text = source.toText();
            if (ImGui::Button(text.empty() ? "-" : text.c_str(), ImVec2(-FLT_MIN, 0.0f)))
            {
                m_capture = cell;
                m_input.setSuppressed(true);
            }
        }
        ImGui::PopID();
    }

    void applyCapture(const io::InputSource &source)
    {
        const Capture capture = *m_capture;
        if (capture.target == Target::Button)
        {
            const auto pad = static_cast<io::PadButton>(capture.index);
            io::ButtonBinding binding = m_input.buttonBinding(pad);
            binding[static_cast<std::size_t>(capture.slot)] = source;
            m_input.setButtonBinding(pad, binding);
        }
        else
        {
            const auto pad = static_cast<io::PadStick>(capture.index);
            io::StickBinding binding = m_input.stickBinding(pad);
            if (capture.target == Target::StickAnalog)
                binding.analog = source;
            else if (capture.target == Target::StickNegative)
                binding.negative = source;
            else
                binding.positive = source;
            m_input.setStickBinding(pad, binding);
        }
        stopCapture();
        save();
    }

    void stopCapture()
    {
        if (m_capture)
            m_input.setSuppressed(false);
        m_capture.reset();
    }

    void save()
    {
        m_input.saveBindings(m_application.config().ini());
        m_application.config().save();
    }

    Application &m_application;
    io::InputSystem &m_input;
    bool m_visible = false;
    std::optional<Capture> m_capture;
};

// ---- Game data ----------------------------------------------------------------------------------------

class DataFolderPanel final : public Panel
{
public:
    explicit DataFolderPanel(Application &application) : m_application(application), m_paths(application.paths())
    {
        // Without its files the port has nothing to run: the question comes up by itself.
        m_visible = !m_paths.hasDataFolder();
    }

    void drawMenu() override
    {
        if (ImGui::BeginMenu("Game"))
        {
            ImGui::MenuItem("Game data...", nullptr, &m_visible);
            ImGui::EndMenu();
        }
    }

    void drawWindow() override
    {
        if (!m_visible)
            return;

        if (!m_primed)
        {
            m_primed = true;
            m_edit = m_application.config().paths.dataFolder;
            m_edit.resize(kEditCapacity);
        }
        {
            std::scoped_lock lock(m_pickLock);
            if (m_pick)
            {
                m_edit = *m_pick;
                m_edit.resize(kEditCapacity);
                m_pick.reset();
            }
        }

        ImGui::SetNextWindowSize(ImVec2(660.0f, 0.0f), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Game Data", &m_visible))
        {
            ImGui::TextWrapped("Where YamiPan reads the game's files from: the disc's root, the folder with "
                               "SPK.ROM and ROM.000 in it.");
            ImGui::Separator();
            ImGui::TextDisabled("In use");
            ImGui::TextWrapped("%s", m_paths.hasDataFolder() ? pathToUtf8(m_paths.dataFolder()).c_str() : "(none)");

            ImGui::Separator();
            constexpr float kBrowseWidth = 100.0f;
            ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - kBrowseWidth - ImGui::GetStyle().ItemSpacing.x);
            ImGui::InputText("##folder", m_edit.data(), m_edit.size());
            ImGui::SameLine();
            const bool picking = m_dialogOpen.load();
            ImGui::BeginDisabled(picking);
            if (ImGui::Button(picking ? "Choosing..." : "Browse...", ImVec2(kBrowseWidth, 0.0f)))
                browse();
            ImGui::EndDisabled();

            const std::string folder = m_edit.c_str();
            const bool valid = !folder.empty() && Paths::isDataFolder(pathFromUtf8(folder));
            if (!folder.empty())
            {
                if (valid)
                    ImGui::TextColored(kGoodColour, "The game's files are there.");
                else
                    ImGui::TextColored(kWarningColour, "No SPK.ROM and ROM.000 there.");
            }

            ImGui::BeginDisabled(!valid);
            if (ImGui::Button("Use this folder"))
            {
                m_application.config().paths.dataFolder = folder;
                m_application.config().save();
                m_application.dataFolderChosen(pathFromUtf8(folder));
            }
            ImGui::EndDisabled();

            ImGui::Separator();
            ImGui::TextDisabled("Settings and saves");
            ImGui::TextWrapped("%s", pathToUtf8(m_paths.userDirectory()).c_str());
            if (ImGui::Button("Open user folder"))
                openInShell(m_paths.userDirectory());
        }
        ImGui::End();
    }

private:
    static constexpr std::size_t kEditCapacity = 1024;

    // OS dialogs belong to the window's thread; the call only opens one, so waiting costs nothing.
    void browse()
    {
        m_dialogOpen.store(true);
        runOnMainThread([this] {
            SDL_ShowOpenFolderDialog(
                [](void *panel, const char *const *files, int) {
                    auto *const self = static_cast<DataFolderPanel *>(panel);
                    if (files != nullptr && files[0] != nullptr)
                    {
                        std::scoped_lock lock(self->m_pickLock);
                        self->m_pick = files[0];
                    }
                    self->m_dialogOpen.store(false);
                },
                this, m_application.window().get(), nullptr, false);
        });
    }

    Application &m_application;
    Paths &m_paths;
    bool m_visible = false;
    bool m_primed = false;
    std::string m_edit;
    std::mutex m_pickLock;
    std::optional<std::string> m_pick;
    std::atomic<bool> m_dialogOpen{false};
};

// ---- Performance -----------------------------------------------------------------------------------------

class PerformancePanel final : public Panel
{
public:
    explicit PerformancePanel(Application &application) : m_application(application) {}

    void drawMenu() override
    {
        if (ImGui::BeginMenu("Diagnostics"))
        {
            ImGui::MenuItem("Performance", nullptr, &m_summary);
            ImGui::MenuItem("Frame graph", nullptr, &m_graph);
            ImGui::EndMenu();
        }
    }

    void drawWindow() override
    {
        const rendering::Renderer::Stats stats = m_application.renderer().stats();
        m_presentHistory[m_cursor] = static_cast<float>(stats.presentRate);
        m_gameHistory[m_cursor] = static_cast<float>(stats.gameRate);
        m_cursor = (m_cursor + 1) % kHistory;

        if (m_summary)
        {
            constexpr ImGuiWindowFlags kFlags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove |
                                                ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings |
                                                ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoNav;
            ImGui::SetNextWindowPos(ImGui::GetMainViewport()->WorkPos, ImGuiCond_Always);
            if (ImGui::Begin("Performance", nullptr, kFlags))
            {
                const rendering::PictureLayout layout = m_application.renderer().layout();
                ImGui::Text("Game %.1f FPS | Present %.1f FPS", stats.gameRate, stats.presentRate);
                ImGui::Text("%zu draws, %zu vertices | %dx%d", stats.draws, stats.vertices, layout.renderWidth,
                            layout.renderHeight);
            }
            ImGui::End();
        }

        if (m_graph)
        {
            ImGui::SetNextWindowSize(ImVec2(520.0f, 260.0f), ImGuiCond_FirstUseEver);
            if (ImGui::Begin("Frame graph", &m_graph))
            {
                if (ImPlot::BeginPlot("##rates", ImVec2(-1.0f, -1.0f)))
                {
                    ImPlot::SetupAxes("", "FPS", ImPlotAxisFlags_NoTickLabels, ImPlotAxisFlags_AutoFit);
                    ImPlot::PlotLine("Game", m_gameHistory.data(), static_cast<int>(kHistory), 1.0, 0.0, 0,
                                     static_cast<int>(m_cursor));
                    ImPlot::PlotLine("Present", m_presentHistory.data(), static_cast<int>(kHistory), 1.0, 0.0, 0,
                                     static_cast<int>(m_cursor));
                    ImPlot::EndPlot();
                }
            }
            ImGui::End();
        }
    }

private:
    static constexpr std::size_t kHistory = 600;

    Application &m_application;
    bool m_summary = false;
    bool m_graph = false;
    std::array<float, kHistory> m_presentHistory{};
    std::array<float, kHistory> m_gameHistory{};
    std::size_t m_cursor = 0;
};

} // namespace

std::vector<std::unique_ptr<Panel>> createPanels(Application &application)
{
    std::vector<std::unique_ptr<Panel>> panels;
    panels.push_back(std::make_unique<DataFolderPanel>(application));
    panels.push_back(std::make_unique<SettingsPanel>(application));
    panels.push_back(std::make_unique<ControlsPanel>(application));
    panels.push_back(std::make_unique<PerformancePanel>(application));
    panels.push_back(createTeleportPanel());
    return panels;
}

} // namespace yamipan::ui
