// The host's settings: window, picture, audio, controls. They deliberately live outside the game's save
// data, in yamipan.ini next to memcard/ in the user directory, hand-editable. Precedence is defaults <
// file < environment, and a run with an environment override does not save, so a test run can never
// disturb what the player chose.
#pragma once

#include <filesystem>
#include <memory>
#include <string>

namespace yamipan
{

class IniFile;

enum class WindowMode
{
    Windowed,
    Borderless, // desktop fullscreen: no video mode switch, so alt-tab is instant
};

enum class RenderResolution
{
    MatchWindow, // the window's own pixels
    NativePs2,   // the PS2's picture height times the scale, whatever the window
    WindowScale, // the window times the scale
};

enum class UpscaleFilter
{
    Nearest,
    Linear,
};

// The shape of the picture. Auto is the window's: no bars, ever.
enum class AspectMode
{
    Auto,
    Ratio4x3,
    Ratio16x9,
    Ratio16x10,
    Ratio21x9,
    Custom,
};

enum class HdrMode
{
    Off,
    Auto,
    ScRgb,
    Hdr10,
};

enum class PresentMode
{
    VSync,
    Mailbox,
    Immediate,
};

struct DisplaySettings
{
    WindowMode windowMode = WindowMode::Windowed;
    int windowWidth = 1280;
    int windowHeight = 960;
    AspectMode aspectMode = AspectMode::Auto;
    float aspectRatio = 16.0f / 9.0f;
    RenderResolution renderMode = RenderResolution::MatchWindow;
    float renderScale = 1.0f;
    UpscaleFilter upscaleFilter = UpscaleFilter::Linear;
    PresentMode presentMode = PresentMode::VSync;

    // The output transform: all at their defaults, the present pass is skipped.
    HdrMode hdrMode = HdrMode::Off;
    float hdrPaperWhite = 200.0f; // nits; 0 follows the OS's SDR white level
    float hdrPeak = 0.0f;         // nits; 0 follows the display
    float hdrExpansion = 0.35f;   // highlight lift, 0..1
    float hdrExpansionKnee = 0.75f;
    float gradeBrightness = 1.0f;
    float gradeContrast = 1.0f;
    float gradeGamma = 1.0f;
    float gradeSaturation = 1.0f;

    template <class Archive>
    void serialize(Archive &archive);
};

struct PathSettings
{
    std::string dataFolder;

    template <class Archive>
    void serialize(Archive &archive);
};

struct GameSettings
{
    bool skipIntro = false;

    template <class Archive>
    void serialize(Archive &archive);
};

// Percent on top of the game's own mix: 100 is the level the game asks for.
struct AudioSettings
{
    int master = 100;
    int music = 100;
    int effects = 100;

    template <class Archive>
    void serialize(Archive &archive);
};

class Config
{
public:
    DisplaySettings display;
    PathSettings paths;
    GameSettings game;
    AudioSettings audio;

    // Reads the file over the defaults, then the environment over that. A missing file is the first
    // run: the defaults stand and the file is written.
    void load(const std::filesystem::path &file);

    // False when the write failed. Does nothing, successfully, for a run with environment overrides.
    bool save() const;

    IniFile &ini() { return *m_ini; }

    Config();
    ~Config();

private:
    void applyEnvironment();

    std::filesystem::path m_file;
    std::unique_ptr<IniFile> m_ini;
    bool m_environmentOverride = false;
};

} // namespace yamipan
