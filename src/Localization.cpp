#include "Localization.h"
#include <Windows.h>
#include <algorithm>
#include <fstream>
#include <map>
#include <iostream>
#include <sstream>

namespace {
    using Strings = std::map<std::string, std::string, std::less<>>;
    std::map<std::string, Strings, std::less<>> catalogs;
    std::vector<Localization::Language> languages;
    std::string systemLocale = "en";
    std::string Trim(std::string value) {
        const auto first = value.find_first_not_of(" \t\r");
        if (first == std::string::npos) return {};
        return value.substr(first, value.find_last_not_of(" \t\r") - first + 1);
    }
    bool ValidValue(const Localization::Text& text, const std::string& value) {
        if (value.empty() && std::string_view(text.key) != "empty") return false;
        if (value.find('#') != std::string::npos) return false;
        const std::string_view key(text.key);
        if ((key == "sectionName" || key == "basicEntry" || key == "playerEntry" || key == "npcEntry") &&
            value.find('/') != std::string::npos) return false;
        return true;
    }
}

std::string Localization::NormalizeCode(std::string_view code) {
    if (code.empty() || code.size() > 31) return "auto";
    std::string result;
    bool separator = true;
    for (unsigned char c : code) {
        if (c == '-') { if (separator) return "auto"; separator = true; }
        else if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) separator = false;
        else return "auto";
        result += c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : static_cast<char>(c);
    }
    return separator ? "auto" : result;
}

void Localization::Load(const std::filesystem::path& directory) {
    // Called once at startup, before menu registration. Catalog pointers remain stable thereafter.
    catalogs.clear(); languages.clear();
    wchar_t locale[LOCALE_NAME_MAX_LENGTH]{};
    if (LCIDToLocaleName(MAKELCID(GetUserDefaultUILanguage(), SORT_DEFAULT), locale, LOCALE_NAME_MAX_LENGTH, 0)) {
        std::string ascii;
        for (const wchar_t c : std::wstring_view(locale)) ascii += static_cast<char>(c);
        systemLocale = NormalizeCode(ascii);
    }
    std::map<std::string, std::string> names{{"en", "English"}};
    auto& english = catalogs["en"];
    for (auto text : catalog) english[text->key] = text->fallback;
    std::error_code error;
    std::vector<std::filesystem::path> files;
    for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end; it.increment(error)) {
        if (it->path().extension() == ".ini") files.push_back(it->path());
    }
    std::sort(files.begin(), files.end());
    for (const auto& path : files) {
        const auto stem = path.stem().u8string();
        const auto code = NormalizeCode(std::string(stem.begin(), stem.end()));
        if (code == "auto") continue;
        std::error_code sizeError;
        const auto bytes = std::filesystem::file_size(path, sizeError);
        if (sizeError || bytes > 256 * 1024) continue;
        std::ifstream file(path, std::ios::binary);
        std::string data((std::istreambuf_iterator<char>(file)), {});
        if (!file || data.size() > 256 * 1024 || data.find('\0') != std::string::npos ||
            !MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, data.data(), static_cast<int>(data.size()), nullptr, 0)) continue;
        if (data.starts_with("\xEF\xBB\xBF")) data.erase(0, 3);
        Strings translated;
        std::string name, section;
        std::istringstream input(data);
        std::string line;
        bool valid = true;
        while (std::getline(input, line)) {
            line = Trim(line);
            if (line.empty() || line[0] == ';') continue;
            if (line.size() > 4096) { valid = false; break; }
            if (line.front() == '[' && line.back() == ']') { section = line.substr(1, line.size() - 2); continue; }
            const auto split = line.find('=');
            if (split == std::string::npos) { valid = false; break; }
            auto key = Trim(line.substr(0, split)), value = Trim(line.substr(split + 1));
            if (section == "Language" && key == "Name") name = value;
            if (section != "Strings") continue;
            for (auto text : catalog) if (key == text->key && ValidValue(*text, value)) {
                const std::string_view fallback(text->fallback);
                const auto suffix = fallback.find("###");
                if (suffix != std::string_view::npos) value += fallback.substr(suffix);
                if (key == "range") value += ' ';
                translated[key] = value;
                break;
            }
        }
        if (!valid || name.empty() || name.find('#') != std::string::npos) continue;
        auto& target = catalogs[code];
        for (auto& [key, value] : translated) target[key] = std::move(value);
        names[code] = name;
    }
    for (const auto& [code, name] : names) languages.push_back({code, name});
}

const std::vector<Localization::Language>& Localization::Languages() { return languages; }

std::string Localization::Resolve(std::string_view requested, std::string_view system) {
    auto code = NormalizeCode(requested);
    if (code == "auto") code = NormalizeCode(system);
    if (catalogs.contains(code)) return code;
    const auto split = code.find('-');
    const auto base = code.substr(0, split);
    if (catalogs.contains(base)) return base;
    if (base == "zh" && catalogs.contains("zh-cn")) return "zh-cn";
    return "en";
}

const char* Localization::Text::Get(std::string_view language) const {
    const auto selected = catalogs.find(Resolve(language, systemLocale));
    if (selected != catalogs.end()) {
        const auto found = selected->second.find(key);
        if (found != selected->second.end()) return found->second.c_str();
    }
    if (const auto en = catalogs.find("en"); en != catalogs.end()) {
        if (const auto found = en->second.find(key); found != en->second.end()) return found->second.c_str();
    }
    return fallback;
}
