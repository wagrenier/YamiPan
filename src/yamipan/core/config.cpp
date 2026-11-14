#include "yamipan/core/config.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdlib>
#include <format>
#include <optional>
#include <string_view>
#include <type_traits>
#include <utility>

#include "yamipan/core/ini_file.h"
#include "yamipan/core/logger.h"

namespace yamipan
{
namespace
{

template <class Enum>
struct EnumNames;

template <>
struct EnumNames<WindowMode>
{
    static constexpr std::array<std::pair<WindowMode, std::string_view>, 2> names{{
        {WindowMode::Windowed, "windowed"},
        {WindowMode::Borderless, "borderless"},
    }};
};

template <>
struct EnumNames<RenderResolution>
{
    static constexpr std::array<std::pair<RenderResolution, std::string_view>, 3> names{{
        {RenderResolution::MatchWindow, "window"},
        {RenderResolution::NativePs2, "native"},
        {RenderResolution::WindowScale, "scaled"},
    }};
};

template <>
struct EnumNames<UpscaleFilter>
{
    static constexpr std::array<std::pair<UpscaleFilter, std::string_view>, 2> names{{
        {UpscaleFilter::Nearest, "nearest"},
        {UpscaleFilter::Linear, "linear"},
    }};
};

template <>
struct EnumNames<AspectMode>
{
    static constexpr std::array<std::pair<AspectMode, std::string_view>, 6> names{{
        {AspectMode::Auto, "auto"},
        {AspectMode::Ratio4x3, "4:3"},
        {AspectMode::Ratio16x9, "16:9"},
        {AspectMode::Ratio16x10, "16:10"},
        {AspectMode::Ratio21x9, "21:9"},
        {AspectMode::Custom, "custom"},
    }};
};

template <>
struct EnumNames<HdrMode>
{
    static constexpr std::array<std::pair<HdrMode, std::string_view>, 4> names{{
        {HdrMode::Off, "off"},
        {HdrMode::Auto, "auto"},
        {HdrMode::ScRgb, "scrgb"},
        {HdrMode::Hdr10, "hdr10"},
    }};
};

template <>
struct EnumNames<PresentMode>
{
    static constexpr std::array<std::pair<PresentMode, std::string_view>, 3> names{{
        {PresentMode::VSync, "vsync"},
        {PresentMode::Mailbox, "mailbox"},
        {PresentMode::Immediate, "immediate"},
    }};
};

template <class Enum>
std::optional<Enum> enumFromName(std::string_view text)
{
    for (const auto &[value, name] : EnumNames<Enum>::names)
    {
        if (name == text)
            return value;
    }
    return std::nullopt;
}

template <class Enum>
std::string_view enumName(Enum value)
{
    for (const auto &[candidate, name] : EnumNames<Enum>::names)
    {
        if (candidate == value)
            return name;
    }
    return EnumNames<Enum>::names.front().second;
}

// One value to and from its text. A value that does not parse leaves the setting as it is.
template <class T>
void parseValue(std::string_view text, T &value)
{
    if constexpr (std::is_same_v<T, std::string>)
        value = std::string(text);
    else if constexpr (std::is_same_v<T, bool>)
        value = text == "1" || text == "true" || text == "on";
    else if constexpr (std::is_enum_v<T>)
        value = enumFromName<T>(text).value_or(value);
    else
        std::from_chars(text.data(), text.data() + text.size(), value);
}

template <class T>
std::string formatValue(const T &value)
{
    if constexpr (std::is_same_v<T, std::string>)
        return value;
    else if constexpr (std::is_same_v<T, bool>)
        return value ? "1" : "0";
    else if constexpr (std::is_enum_v<T>)
        return std::string(enumName(value));
    else
        return std::format("{}", value);
}

// The settings structs say once what they hold; these two walk them to and from the file.
class IniReader
{
public:
    IniReader(const IniFile &file, std::string_view section) : m_file(file), m_section(section) {}

    template <class T>
    void operator()(std::string_view key, T &value)
    {
        if (const auto text = m_file.find(m_section, key))
            parseValue(*text, value);
    }

private:
    const IniFile &m_file;
    std::string_view m_section;
};

class IniWriter
{
public:
    IniWriter(IniFile &file, std::string_view section) : m_file(file), m_section(section) {}

    template <class T>
    void operator()(std::string_view key, T &value)
    {
        m_file.set(m_section, key, formatValue(value));
    }

private:
    IniFile &m_file;
    std::string_view m_section;
};

template <class Visitor, class Config_>
void visitSections(Config_ &config, IniFile &file)
{
    {
        Visitor visitor(file, "display");
        config.display.serialize(visitor);
    }
    {
        Visitor visitor(file, "paths");
        config.paths.serialize(visitor);
    }
    {
        Visitor visitor(file, "game");
        config.game.serialize(visitor);
    }
    {
        Visitor visitor(file, "audio");
        config.audio.serialize(visitor);
    }
}

} // namespace

template <class Archive>
void DisplaySettings::serialize(Archive &archive)
{
    archive("window_mode", windowMode);
    archive("window_width", windowWidth);
    archive("window_height", windowHeight);
    archive("aspect", aspectMode);
    archive("aspect_ratio", aspectRatio);
    archive("render_mode", renderMode);
    archive("render_scale", renderScale);
    archive("upscale_filter", upscaleFilter);
    archive("present_mode", presentMode);
    archive("smooth_motion", smoothMotion);
    archive("per_pixel_lighting", perPixelLighting);
    archive("hdr", hdrMode);
    archive("hdr_paper_white", hdrPaperWhite);
    archive("hdr_peak", hdrPeak);
    archive("hdr_expansion", hdrExpansion);
    archive("hdr_expansion_knee", hdrExpansionKnee);
    archive("brightness", gradeBrightness);
    archive("contrast", gradeContrast);
    archive("gamma", gradeGamma);
    archive("saturation", gradeSaturation);
}

template <class Archive>
void PathSettings::serialize(Archive &archive)
{
    archive("data_folder", dataFolder);
}

template <class Archive>
void GameSettings::serialize(Archive &archive)
{
    archive("skip_intro", skipIntro);
    archive("game_rate", gameRate);
}

template <class Archive>
void AudioSettings::serialize(Archive &archive)
{
    archive("master", master);
    archive("music", music);
    archive("effects", effects);
}

Config::Config() : m_ini(std::make_unique<IniFile>()) {}
Config::~Config() = default;

void Config::load(const std::filesystem::path &file)
{
    m_file = file;
    const bool firstRun = !m_ini->load(file);
    if (!firstRun)
        visitSections<IniReader>(*this, *m_ini);

    display.windowWidth = std::max(display.windowWidth, 320);
    display.windowHeight = std::max(display.windowHeight, 240);
    display.renderScale = std::clamp(display.renderScale, 0.25f, 8.0f);
    display.aspectRatio = std::clamp(display.aspectRatio, 0.5f, 5.0f);

    if (firstRun)
        save();
    applyEnvironment();
}

bool Config::save() const
{
    if (m_environmentOverride)
        return true;
    visitSections<IniWriter>(const_cast<Config &>(*this), *m_ini);
    if (m_ini->save(m_file))
        return true;
    log::warning("Could not write the settings file");
    return false;
}

void Config::applyEnvironment()
{
    const auto apply = [this]<class T>(const char *name, T &value) {
        if (const char *const text = std::getenv(name))
        {
            parseValue(text, value);
            m_environmentOverride = true;
        }
    };
    apply("YAMIPAN_SKIP_INTRO", game.skipIntro);
    apply("YAMIPAN_GAME_RATE", game.gameRate);
    apply("YAMIPAN_ASPECT", display.aspectMode);
    apply("YAMIPAN_ASPECT_RATIO", display.aspectRatio);
    apply("YAMIPAN_RENDER_MODE", display.renderMode);
    apply("YAMIPAN_RENDER_SCALE", display.renderScale);
    apply("YAMIPAN_WINDOW_WIDTH", display.windowWidth);
    apply("YAMIPAN_WINDOW_HEIGHT", display.windowHeight);
    apply("YAMIPAN_PRESENT_MODE", display.presentMode);
    apply("YAMIPAN_SMOOTH_MOTION", display.smoothMotion);
    apply("YAMIPAN_PER_PIXEL_LIGHTING", display.perPixelLighting);
}

} // namespace yamipan
