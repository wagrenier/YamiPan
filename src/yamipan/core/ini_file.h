// A hand-editable settings file: [sections] of key = value, written back in the order it was read, so a
// player's layout survives a save. Comments are not kept: the file is regenerated from the settings.
#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace yamipan
{

class IniFile
{
public:
    bool load(const std::filesystem::path &file);
    bool save(const std::filesystem::path &file) const;

    std::optional<std::string_view> find(std::string_view section, std::string_view key) const;
    void set(std::string_view section, std::string_view key, std::string value);

private:
    struct Section
    {
        std::string name;
        std::vector<std::pair<std::string, std::string>> entries;
    };

    Section &section(std::string_view name);

    std::vector<Section> m_sections;
};

} // namespace yamipan
