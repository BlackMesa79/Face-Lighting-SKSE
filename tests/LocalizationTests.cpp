#include "Localization.h"
#include <fstream>
#include <iostream>
#include <stdexcept>
void Check(bool value) { if (!value) throw std::runtime_error("Localization check failed"); }
int main() {
    try {
        using namespace Localization;
        const auto root = std::filesystem::current_path() / "build/localization-test";
        std::filesystem::create_directories(root);
        std::filesystem::copy_file("languages/en.ini", root / "en.ini", std::filesystem::copy_options::overwrite_existing);
        std::filesystem::copy_file("languages/zh-CN.ini", root / "zh-CN.ini", std::filesystem::copy_options::overwrite_existing);
        { std::ofstream f(root / "fr.ini"); f << "[Language]\nName=Français\n[Strings]\nenabled=Éclairage 100%\nplayerEntry=Bad/Path\nsaveAll=Bad###id\n"; }
        { std::ofstream f(root / "de.ini"); f << "[Language]\nName=Deutsch\n[Strings]\nenabled=Test\nbroken line\n"; }
        { std::ofstream f(root / "ru.ini", std::ios::binary); f << "[Language]\nName=\xff\n"; }
        Load(root);
        Check(Languages().size() == 3);
        Check(Resolve("auto", "fr-FR") == "fr");
        Check(Resolve("auto", "zh-TW") == "zh-cn");
        Check(Resolve("auto", "ja-JP") == "en");
        Check(Resolve("en", "zh-CN") == "en");
        Check(Resolve("zh-CN", "en-US") == "zh-cn");
        Check(NormalizeCode("../fr") == "auto");
        Check(std::string(enabled.Get("fr")) == "Éclairage 100%###enabled");
        Check(std::string(playerEntry.Get("fr")) == "Player face light");
        Check(std::string(saveAll.Get("fr")) == "Save all settings###save");
        Check(std::string(playerEntry.Get("zh-cn")) == "玩家面光");
        Check(std::string(padKey.Get("zh-cn")) == "手柄快捷键###padKey");
        Load(root / "missing");
        Check(std::string(enabled.Get("fr")) == "Enable player face light###enabled");
        std::cout << "Localization UTF-8, language detection, fallback, invalid files and stable IDs passed.\n";
    } catch (const std::exception& e) { std::cerr << e.what(); return 1; }
}
