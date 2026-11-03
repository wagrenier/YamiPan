#include "yamipan/io/input_system.h"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <format>
#include <ranges>

#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_scancode.h>

#include "yamipan/core/ini_file.h"
#include "yamipan/core/logger.h"

namespace yamipan::io
{
namespace
{

constexpr int kFullTravel = 32767;
constexpr int kTriggerThreshold = 8000;
constexpr int kStickCentre = 127;
constexpr int kStickRange = 128;
constexpr unsigned kScriptDefaultHold = 3;
constexpr int kRumbleMillis = 200;

constexpr std::array<std::string_view, kPadButtonCount> kButtonNames{
    "up", "down", "left", "right", "triangle", "cross", "square", "circle",
    "l1", "l2", "r1", "r2", "start", "select", "l3", "r3",
};
constexpr std::array<std::string_view, kPadStickCount> kStickNames{"left_x", "left_y", "right_x", "right_y"};

// The scripted pad's names, in PadButton order, then the left-stick pushes.
constexpr std::array<std::string_view, kPadButtonCount + 4> kScriptNames{
    "UP", "DOWN", "LEFT", "RIGHT", "TRIANGLE", "CROSS", "SQUARE", "CIRCLE",
    "L1", "L2", "R1", "R2", "START", "SELECT", "L3", "R3",
    "LSUP", "LSDOWN", "LSLEFT", "LSRIGHT",
};

constexpr InputSource key(SDL_Scancode code)
{
    return {InputSource::Kind::Key, code, 0, 0};
}

constexpr InputSource pad(SDL_GamepadButton code)
{
    return {InputSource::Kind::Button, code, 0, 0};
}

constexpr InputSource axis(SDL_GamepadAxis code, int threshold)
{
    return {InputSource::Kind::Axis, code, 1, threshold};
}

// The keyboard rings WASD: Q / E / C and Space are the face buttons, R / F the shoulders, Z / X the triggers,
// V is R3, Left Shift L3 (it clicks the stick WASD drives). IJKL aim: the arrows are the D-pad.
constexpr std::array<ButtonBinding, kPadButtonCount> kDefaultButtons{{
    {key(SDL_SCANCODE_UP), pad(SDL_GAMEPAD_BUTTON_DPAD_UP)},
    {key(SDL_SCANCODE_DOWN), pad(SDL_GAMEPAD_BUTTON_DPAD_DOWN)},
    {key(SDL_SCANCODE_LEFT), pad(SDL_GAMEPAD_BUTTON_DPAD_LEFT)},
    {key(SDL_SCANCODE_RIGHT), pad(SDL_GAMEPAD_BUTTON_DPAD_RIGHT)},
    {key(SDL_SCANCODE_E), pad(SDL_GAMEPAD_BUTTON_NORTH)},
    {key(SDL_SCANCODE_SPACE), pad(SDL_GAMEPAD_BUTTON_SOUTH)},
    {key(SDL_SCANCODE_Q), pad(SDL_GAMEPAD_BUTTON_WEST)},
    {key(SDL_SCANCODE_C), pad(SDL_GAMEPAD_BUTTON_EAST)},
    {key(SDL_SCANCODE_R), pad(SDL_GAMEPAD_BUTTON_LEFT_SHOULDER)},
    {key(SDL_SCANCODE_Z), axis(SDL_GAMEPAD_AXIS_LEFT_TRIGGER, kTriggerThreshold)},
    {key(SDL_SCANCODE_F), pad(SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER)},
    {key(SDL_SCANCODE_X), axis(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, kTriggerThreshold)},
    {key(SDL_SCANCODE_ESCAPE), pad(SDL_GAMEPAD_BUTTON_START)},
    {key(SDL_SCANCODE_TAB), pad(SDL_GAMEPAD_BUTTON_BACK)},
    {key(SDL_SCANCODE_LSHIFT), pad(SDL_GAMEPAD_BUTTON_LEFT_STICK)},
    {key(SDL_SCANCODE_V), pad(SDL_GAMEPAD_BUTTON_RIGHT_STICK)},
}};

constexpr std::array<StickBinding, kPadStickCount> kDefaultSticks{{
    {axis(SDL_GAMEPAD_AXIS_LEFTX, 0), key(SDL_SCANCODE_A), key(SDL_SCANCODE_D), 1},
    {axis(SDL_GAMEPAD_AXIS_LEFTY, 0), key(SDL_SCANCODE_W), key(SDL_SCANCODE_S), 1},
    {axis(SDL_GAMEPAD_AXIS_RIGHTX, 0), key(SDL_SCANCODE_J), key(SDL_SCANCODE_L), 6},
    {axis(SDL_GAMEPAD_AXIS_RIGHTY, 0), key(SDL_SCANCODE_I), key(SDL_SCANCODE_K), 6},
}};

} // namespace

std::string_view padButtonName(PadButton button)
{
    return kButtonNames[static_cast<std::size_t>(button)];
}

std::string_view padStickName(PadStick stick)
{
    return kStickNames[static_cast<std::size_t>(stick)];
}

std::string InputSource::toText() const
{
    std::string text;
    switch (kind)
    {
    case Kind::Key:
        text = std::format("key:{}", SDL_GetScancodeName(static_cast<SDL_Scancode>(code)));
        break;
    case Kind::Button:
        text = std::format("pad:{}", SDL_GetGamepadStringForButton(static_cast<SDL_GamepadButton>(code)));
        break;
    case Kind::Axis:
        text = std::format("axis{}:{}", direction < 0 ? '-' : '+',
                           SDL_GetGamepadStringForAxis(static_cast<SDL_GamepadAxis>(code)));
        break;
    case Kind::None:
        return {};
    }
    if (kind == Kind::Axis && threshold != 0)
        text += std::format("@{}", threshold);
    return text;
}

std::optional<InputSource> InputSource::fromText(std::string_view text)
{
    const auto colon = text.find(':');
    if (colon == std::string_view::npos)
        return std::nullopt;

    const std::string_view prefix = text.substr(0, colon);
    std::string_view body = text.substr(colon + 1);

    InputSource source;
    if (const auto at = body.find('@'); at != std::string_view::npos)
    {
        const std::string_view number = body.substr(at + 1);
        std::from_chars(number.data(), number.data() + number.size(), source.threshold);
        body = body.substr(0, at);
    }

    const std::string name(body);
    if (prefix == "key")
    {
        source.kind = Kind::Key;
        source.code = SDL_GetScancodeFromName(name.c_str());
        if (source.code == SDL_SCANCODE_UNKNOWN)
            return std::nullopt;
    }
    else if (prefix == "pad")
    {
        source.kind = Kind::Button;
        source.code = SDL_GetGamepadButtonFromString(name.c_str());
        if (source.code == SDL_GAMEPAD_BUTTON_INVALID)
            return std::nullopt;
    }
    else if (prefix == "axis+" || prefix == "axis-")
    {
        source.kind = Kind::Axis;
        source.direction = prefix == "axis+" ? 1 : -1;
        source.code = SDL_GetGamepadAxisFromString(name.c_str());
        if (source.code == SDL_GAMEPAD_AXIS_INVALID)
            return std::nullopt;
    }
    else
    {
        return std::nullopt;
    }
    return source;
}

InputSystem::InputSystem() : m_buttons(kDefaultButtons), m_sticks(kDefaultSticks)
{
    if (const char *const script = std::getenv("YAMIPAN_AUTOPAD"))
    {
        m_scriptEnabled = true; // set but empty still masks the host's devices
        parseScript(script);
    }
}

InputSystem::~InputSystem()
{
    if (m_gamepad != nullptr)
        SDL_CloseGamepad(m_gamepad);
}

// "<read>:<BUTTON>[*<reads>],...": hold BUTTON for three reads, or <reads>, from that read on.
void InputSystem::parseScript(std::string_view script)
{
    for (const auto part : script | std::views::split(','))
    {
        const std::string_view token(part.begin(), part.end());
        const auto colon = token.find(':');
        if (colon == std::string_view::npos)
            continue;

        ScriptedPress press{0, kScriptDefaultHold, 0};
        std::from_chars(token.data(), token.data() + colon, press.read);

        std::string_view name = token.substr(colon + 1);
        if (const auto star = name.find('*'); star != std::string_view::npos)
        {
            const std::string_view hold = name.substr(star + 1);
            std::from_chars(hold.data(), hold.data() + hold.size(), press.hold);
            name = name.substr(0, star);
        }

        const auto found = std::ranges::find(kScriptNames, name);
        if (found == kScriptNames.end())
            continue;
        press.control = static_cast<unsigned>(found - kScriptNames.begin());
        m_script.push_back(press);
    }
    log::info("Scripted pad: {} presses", m_script.size());
}

bool InputSystem::scriptHeld(unsigned control) const
{
    return std::ranges::any_of(m_script, [this, control](const ScriptedPress &press) {
        return press.control == control && m_scriptReads >= press.read && m_scriptReads < press.read + press.hold;
    });
}

PadReport InputSystem::readScript()
{
    m_scriptReads++;

    PadReport report;
    report.sticks.fill(kStickCentre);
    for (unsigned button = 0; button < kPadButtonCount; button++)
    {
        if (scriptHeld(button))
            report.held |= static_cast<std::uint16_t>(1u << button);
    }

    constexpr auto leftX = static_cast<std::size_t>(PadStick::LeftX);
    constexpr auto leftY = static_cast<std::size_t>(PadStick::LeftY);
    constexpr auto pushes = static_cast<unsigned>(kPadButtonCount);
    if (scriptHeld(pushes + 0))
        report.sticks[leftY] = 0x00;
    if (scriptHeld(pushes + 1))
        report.sticks[leftY] = 0xff;
    if (scriptHeld(pushes + 2))
        report.sticks[leftX] = 0x00;
    if (scriptHeld(pushes + 3))
        report.sticks[leftX] = 0xff;
    return report;
}

void InputSystem::handleEvent(const SDL_Event &event)
{
    if (event.type == SDL_EVENT_GAMEPAD_ADDED || event.type == SDL_EVENT_GAMEPAD_REMOVED)
    {
        std::scoped_lock lock(m_lock);
        chooseGamepad();
    }
}

void InputSystem::chooseGamepad()
{
    int count = 0;
    SDL_JoystickID *const ids = SDL_GetGamepads(&count);

    SDL_JoystickID chosen = 0;
    for (int index = 0; index < count; index++)
    {
        char guid[33];
        SDL_GUIDToString(SDL_GetGamepadGUIDForID(ids[index]), guid, sizeof(guid));
        if (chosen == 0 || m_preferredGuid == guid)
            chosen = ids[index];
        if (m_preferredGuid == guid)
            break;
    }
    SDL_free(ids);

    if (m_gamepad != nullptr && (chosen == 0 || SDL_GetGamepadID(m_gamepad) != chosen))
    {
        SDL_CloseGamepad(m_gamepad);
        m_gamepad = nullptr;
    }
    if (m_gamepad == nullptr && chosen != 0)
    {
        m_gamepad = SDL_OpenGamepad(chosen);
        if (m_gamepad != nullptr)
            log::info("Gamepad: {}", SDL_GetGamepadName(m_gamepad));
    }
}

int InputSystem::magnitudeOf(const InputSource &source, const bool *keys) const
{
    switch (source.kind)
    {
    case InputSource::Kind::Key:
        return keys != nullptr && source.code > 0 && source.code < SDL_SCANCODE_COUNT && keys[source.code]
                   ? kFullTravel
                   : 0;
    case InputSource::Kind::Button:
        return m_gamepad != nullptr && SDL_GetGamepadButton(m_gamepad, static_cast<SDL_GamepadButton>(source.code))
                   ? kFullTravel
                   : 0;
    case InputSource::Kind::Axis: {
        if (m_gamepad == nullptr)
            return 0;
        const int value = SDL_GetGamepadAxis(m_gamepad, static_cast<SDL_GamepadAxis>(source.code)) * source.direction;
        return std::clamp(value, 0, kFullTravel);
    }
    case InputSource::Kind::None:
        break;
    }
    return 0;
}

bool InputSystem::isHeld(const InputSource &source, const bool *keys) const
{
    return source.kind != InputSource::Kind::None && magnitudeOf(source, keys) > source.threshold;
}

std::uint8_t InputSystem::stickValue(std::size_t index, const bool *keys)
{
    const StickBinding &binding = m_sticks[index];

    float analog = 0.0f;
    if (m_gamepad != nullptr && binding.analog.kind == InputSource::Kind::Axis)
    {
        const int raw = SDL_GetGamepadAxis(m_gamepad, static_cast<SDL_GamepadAxis>(binding.analog.code));
        analog = static_cast<float>(raw * binding.analog.direction) / static_cast<float>(kFullTravel);
    }

    const float pull = (isHeld(binding.positive, keys) ? 1.0f : 0.0f) - (isHeld(binding.negative, keys) ? 1.0f : 0.0f);
    const float step = 1.0f / static_cast<float>(std::max(binding.rampFrames, 1));
    float &ramp = m_ramp[index];
    if (pull == 0.0f)
        ramp = 0.0f;
    else
        ramp = std::clamp(ramp + pull * step, -1.0f, 1.0f);

    // Whichever shows the larger deflection.
    const float value = std::abs(analog) > std::abs(ramp) ? analog : ramp;
    return static_cast<std::uint8_t>(std::clamp(kStickCentre + static_cast<int>(value * kStickRange), 0, 255));
}

PadReport InputSystem::read()
{
    std::scoped_lock lock(m_lock);
    if (m_scriptEnabled)
        return readScript();

    PadReport report;
    report.sticks.fill(kStickCentre);
    if (m_suppressed.load())
        return report;

    const bool *const keys = SDL_GetKeyboardState(nullptr);
    for (std::size_t button = 0; button < kPadButtonCount; button++)
    {
        const bool held =
            std::ranges::any_of(m_buttons[button], [&](const InputSource &source) { return isHeld(source, keys); });
        if (held)
            report.held |= static_cast<std::uint16_t>(1u << button);
    }
    for (std::size_t stick = 0; stick < kPadStickCount; stick++)
        report.sticks[stick] = stickValue(stick, keys);
    return report;
}

void InputSystem::rumble(bool largeMotor, std::uint8_t smallStrength)
{
    std::scoped_lock lock(m_lock);
    if (m_gamepad == nullptr)
        return;
    constexpr int kScale = 257; // 0xff to 0xffff
    SDL_RumbleGamepad(m_gamepad, largeMotor ? 0xffff : 0, static_cast<Uint16>(smallStrength * kScale), kRumbleMillis);
}

ButtonBinding InputSystem::buttonBinding(PadButton button) const
{
    std::scoped_lock lock(m_lock);
    return m_buttons[static_cast<std::size_t>(button)];
}

void InputSystem::setButtonBinding(PadButton button, const ButtonBinding &binding)
{
    std::scoped_lock lock(m_lock);
    m_buttons[static_cast<std::size_t>(button)] = binding;
}

StickBinding InputSystem::stickBinding(PadStick stick) const
{
    std::scoped_lock lock(m_lock);
    return m_sticks[static_cast<std::size_t>(stick)];
}

void InputSystem::setStickBinding(PadStick stick, const StickBinding &binding)
{
    std::scoped_lock lock(m_lock);
    m_sticks[static_cast<std::size_t>(stick)] = binding;
}

void InputSystem::resetBindings()
{
    std::scoped_lock lock(m_lock);
    m_buttons = kDefaultButtons;
    m_sticks = kDefaultSticks;
}

bool InputSystem::bindingsAreDefault() const
{
    std::scoped_lock lock(m_lock);
    return m_buttons == kDefaultButtons && m_sticks == kDefaultSticks;
}

void InputSystem::loadBindings(const IniFile &file)
{
    std::scoped_lock lock(m_lock);
    for (std::size_t button = 0; button < kPadButtonCount; button++)
    {
        const auto text = file.find("controls", kButtonNames[button]);
        if (!text)
            continue;

        ButtonBinding binding{};
        std::size_t slot = 0;
        for (const auto part : *text | std::views::split(','))
        {
            std::string_view token(part.begin(), part.end());
            while (!token.empty() && token.front() == ' ')
                token.remove_prefix(1);
            if (const auto source = InputSource::fromText(token); source && slot < binding.size())
                binding[slot++] = *source;
        }
        if (slot != 0)
            m_buttons[button] = binding;
    }

    for (std::size_t stick = 0; stick < kPadStickCount; stick++)
    {
        StickBinding &binding = m_sticks[stick];
        const std::string name(kStickNames[stick]);
        if (const auto text = file.find("controls", name + "_axis"))
            binding.analog = InputSource::fromText(*text).value_or(binding.analog);
        if (const auto text = file.find("controls", name + "_negative"))
            binding.negative = InputSource::fromText(*text).value_or(binding.negative);
        if (const auto text = file.find("controls", name + "_positive"))
            binding.positive = InputSource::fromText(*text).value_or(binding.positive);
    }

    if (const auto text = file.find("controls", "gamepad"))
        m_preferredGuid = std::string(*text);
}

void InputSystem::saveBindings(IniFile &file) const
{
    std::scoped_lock lock(m_lock);
    for (std::size_t button = 0; button < kPadButtonCount; button++)
    {
        std::string line;
        for (const InputSource &source : m_buttons[button])
        {
            const std::string text = source.toText();
            if (!text.empty())
                line += (line.empty() ? "" : ", ") + text;
        }
        file.set("controls", kButtonNames[button], std::move(line));
    }
    for (std::size_t stick = 0; stick < kPadStickCount; stick++)
    {
        const std::string name(kStickNames[stick]);
        file.set("controls", name + "_axis", m_sticks[stick].analog.toText());
        file.set("controls", name + "_negative", m_sticks[stick].negative.toText());
        file.set("controls", name + "_positive", m_sticks[stick].positive.toText());
    }
    file.set("controls", "gamepad", m_preferredGuid);
}

std::vector<InputSystem::GamepadInfo> InputSystem::gamepads() const
{
    std::vector<GamepadInfo> result;
    int count = 0;
    SDL_JoystickID *const ids = SDL_GetGamepads(&count);
    for (int index = 0; index < count; index++)
    {
        char guid[33];
        SDL_GUIDToString(SDL_GetGamepadGUIDForID(ids[index]), guid, sizeof(guid));
        const char *const name = SDL_GetGamepadNameForID(ids[index]);
        result.push_back({name != nullptr ? name : "Gamepad", guid});
    }
    SDL_free(ids);
    return result;
}

std::optional<InputSystem::GamepadInfo> InputSystem::activeGamepad() const
{
    std::scoped_lock lock(m_lock);
    if (m_gamepad == nullptr)
        return std::nullopt;
    char guid[33];
    SDL_GUIDToString(SDL_GetGamepadGUIDForID(SDL_GetGamepadID(m_gamepad)), guid, sizeof(guid));
    return GamepadInfo{SDL_GetGamepadName(m_gamepad), guid};
}

void InputSystem::setPreferredGamepad(std::string guid)
{
    std::scoped_lock lock(m_lock);
    m_preferredGuid = std::move(guid);
    chooseGamepad();
}

std::string InputSystem::preferredGamepad() const
{
    std::scoped_lock lock(m_lock);
    return m_preferredGuid;
}

} // namespace yamipan::io
