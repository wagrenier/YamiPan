// Keyboard and gamepad as one DualShock 2. The sixteen buttons and four stick axes below are the whole
// vocabulary of the layers above, so what a physical control *means* is decided here and nowhere else:
// bindings are data, a button can be held by any of its sources, and libpad only packs the report.
#pragma once

#include <array>
#include <atomic>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>

namespace yamipan
{
class IniFile;
}

namespace yamipan::io
{

enum class PadButton
{
    Up,
    Down,
    Left,
    Right,
    Triangle,
    Cross,
    Square,
    Circle,
    L1,
    L2,
    R1,
    R2,
    Start,
    Select,
    L3,
    R3,
};
inline constexpr std::size_t kPadButtonCount = 16;

enum class PadStick
{
    LeftX,
    LeftY,
    RightX,
    RightY,
};
inline constexpr std::size_t kPadStickCount = 4;

std::string_view padButtonName(PadButton button);
std::string_view padStickName(PadStick stick);

// One physical control.
struct InputSource
{
    enum class Kind
    {
        None,
        Key,    // code: an SDL scancode
        Button, // code: an SDL gamepad button
        Axis,   // code: an SDL gamepad axis; direction says which half counts
    };

    Kind kind = Kind::None;
    int code = 0;
    int direction = 0;
    int threshold = 0; // axis: travel past this reads as held

    bool operator==(const InputSource &) const = default;

    // SDL's own names, so the settings file stays hand-editable: key:<scancode>, pad:<button>,
    // axis+:<axis>, axis-:<axis>, a threshold as @<n>.
    std::string toText() const;
    static std::optional<InputSource> fromText(std::string_view text);
};

inline constexpr std::size_t kSourcesPerButton = 4;
using ButtonBinding = std::array<InputSource, kSourcesPerButton>;

// A full-range analog source plus a digital pair pulling either way. rampFrames is how many reads a held
// digital source takes to reach full deflection: 1 snaps, which movement wants; a camera that jumps to
// the rail on the first frame of a key press is unusable, so the right stick ramps.
struct StickBinding
{
    InputSource analog;
    InputSource negative;
    InputSource positive;
    int rampFrames = 1;

    bool operator==(const StickBinding &) const = default;
};

// One sample of the pad, device-neutral.
struct PadReport
{
    std::uint16_t held = 0;               // bit n: PadButton n
    std::array<std::uint8_t, 4> sticks{}; // PadStick order, 127 centred
};

class InputSystem
{
public:
    InputSystem();
    ~InputSystem();

    InputSystem(const InputSystem &) = delete;
    InputSystem &operator=(const InputSystem &) = delete;

    // On the main thread, for each event: device hot-plug.
    void handleEvent(const SDL_Event &event);

    // The pad now. Advances the stick ramps and a scripted pad, so once per scePadRead.
    PadReport read();

    // While suppressed (the rebinding UI waits for a key, ImGui has the keyboard) the game reads a
    // released pad. It keeps running, it just stops being driven.
    void setSuppressed(bool suppressed) { m_suppressed.store(suppressed); }

    // True when a scripted pad replaces the host's devices (YAMIPAN_AUTOPAD).
    bool scripted() const { return m_scriptEnabled; }

    void rumble(bool largeMotor, std::uint8_t smallStrength);

    // ---- bindings ----------------------------------------------------------------------------
    ButtonBinding buttonBinding(PadButton button) const;
    void setButtonBinding(PadButton button, const ButtonBinding &binding);
    StickBinding stickBinding(PadStick stick) const;
    void setStickBinding(PadStick stick, const StickBinding &binding);
    void resetBindings();
    bool bindingsAreDefault() const;

    void loadBindings(const IniFile &file);
    void saveBindings(IniFile &file) const;

    // ---- devices -----------------------------------------------------------------------------
    struct GamepadInfo
    {
        std::string name;
        std::string guid;
    };
    std::vector<GamepadInfo> gamepads() const;
    std::optional<GamepadInfo> activeGamepad() const;

    // A GUID, not a list position: the list is whatever the OS enumerated this boot. Empty means the
    // first one. A device not plugged in now is honoured the moment it appears.
    void setPreferredGamepad(std::string guid);
    std::string preferredGamepad() const;

private:
    struct ScriptedPress
    {
        unsigned read;
        unsigned hold;
        unsigned control; // a PadButton, or past them a left-stick push
    };

    void parseScript(std::string_view script);
    bool scriptHeld(unsigned control) const;
    PadReport readScript();
    void chooseGamepad();
    int magnitudeOf(const InputSource &source, const bool *keys) const;
    bool isHeld(const InputSource &source, const bool *keys) const;
    std::uint8_t stickValue(std::size_t index, const bool *keys);

    mutable std::mutex m_lock;
    std::array<ButtonBinding, kPadButtonCount> m_buttons;
    std::array<StickBinding, kPadStickCount> m_sticks;
    std::array<float, kPadStickCount> m_ramp{};
    SDL_Gamepad *m_gamepad = nullptr;
    std::string m_preferredGuid;
    std::atomic<bool> m_suppressed{false};

    bool m_scriptEnabled = false;
    std::vector<ScriptedPress> m_script;
    unsigned m_scriptReads = 0;
};

} // namespace yamipan::io
