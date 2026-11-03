#include "yamipan/core/ini_file.h"

#include <algorithm>
#include <fstream>

namespace yamipan
{
namespace
{

std::string_view trimmed(std::string_view text)
{
    const auto isSpace = [](char c) { return c == ' ' || c == '\t' || c == '\r'; };
    while (!text.empty() && isSpace(text.front()))
        text.remove_prefix(1);
    while (!text.empty() && isSpace(text.back()))
        text.remove_suffix(1);
    return text;
}

} // namespace

bool IniFile::load(const std::filesystem::path &file)
{
    std::ifstream stream(file);
    if (!stream)
        return false;

    m_sections.clear();
    Section *current = nullptr;
    for (std::string raw; std::getline(stream, raw);)
    {
        const std::string_view line = trimmed(raw);
        if (line.empty() || line.front() == ';' || line.front() == '#')
            continue;
        if (line.front() == '[' && line.back() == ']')
        {
            current = &section(line.substr(1, line.size() - 2));
            continue;
        }
        const auto equals = line.find('=');
        if (current != nullptr && equals != std::string_view::npos)
        {
            current->entries.emplace_back(std::string(trimmed(line.substr(0, equals))),
                                          std::string(trimmed(line.substr(equals + 1))));
        }
    }
    return true;
}

bool IniFile::save(const std::filesystem::path &file) const
{
    std::ofstream stream(file, std::ios::out | std::ios::trunc);
    if (!stream)
        return false;

    for (const Section &entry : m_sections)
    {
        stream << '[' << entry.name << "]\n";
        for (const auto &[key, value] : entry.entries)
            stream << key << " = " << value << '\n';
        stream << '\n';
    }
    return stream.good();
}

std::optional<std::string_view> IniFile::find(std::string_view name, std::string_view key) const
{
    const auto found = std::ranges::find(m_sections, name, &Section::name);
    if (found == m_sections.end())
        return std::nullopt;

    const auto entry = std::ranges::find_if(found->entries, [key](const auto &item) { return item.first == key; });
    if (entry == found->entries.end())
        return std::nullopt;
    return entry->second;
}

void IniFile::set(std::string_view name, std::string_view key, std::string value)
{
    Section &target = section(name);
    const auto entry = std::ranges::find_if(target.entries, [key](const auto &item) { return item.first == key; });
    if (entry != target.entries.end())
        entry->second = std::move(value);
    else
        target.entries.emplace_back(std::string(key), std::move(value));
}

IniFile::Section &IniFile::section(std::string_view name)
{
    const auto found = std::ranges::find(m_sections, name, &Section::name);
    if (found != m_sections.end())
        return *found;
    return m_sections.emplace_back(Section{std::string(name), {}});
}

} // namespace yamipan
