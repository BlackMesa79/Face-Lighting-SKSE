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
        Check(std::string(noticeNoTarget.Get("zh-cn")) == "面部光照：添加失败，未选中有效 NPC");
        Check(std::string(lightDiagnostics.Get("fr")) == "Record player light-level diagnostics");
        Check(std::string(ambientAutomatic.Get("zh-cn")).find("固定补偿") != std::string::npos);
        Check(std::string(ambientFiltered.Get("zh-cn")).find("实时排除面光") != std::string::npos);
        Check(std::string(ambientFiltered.Get("fr")).find("live exclusion") != std::string::npos);
        Check(std::string(playerTransition.Get("zh-cn")).find("玩家面光渐亮渐暗") != std::string::npos);
        Check(std::string(exclusionDiagnostics.Get("en")).find("restart required") != std::string::npos);
        Check(std::string(ambientObserve.Get("en")).find("Observe compensation only") != std::string::npos);
        Check(std::string(npcLightLimit.Get("zh-cn")).find("同时启用的 NPC 面光数量") != std::string::npos);
        Check(std::string(npcLightLimit.Get("en")).find("Simultaneous NPC face lights") != std::string::npos);
        Check(std::string(npcLightLimitHelp.Get("en")).find("default 4") != std::string::npos);
        Check(std::string(dialogueAmbientMode.Get("zh-cn")).find("对话面光环境控制模式") != std::string::npos);
        Check(std::string(dialogueAmbientFiltered.Get("en")).find("live exclusion") != std::string::npos);
        Check(std::string(ambientPollMode.Get("zh-cn")).find("环境检测频率") != std::string::npos);
        Check(std::string(ambientPollPerformance.Get("en")).find("1 second") != std::string::npos);
        Check(std::string(ambientPollResponsive.Get("zh-cn")).find("0.2 秒") != std::string::npos);
        Load(root / "missing");
        Check(std::string(enabled.Get("fr")) == "Enable player face light###enabled");
        std::cout << "Localization UTF-8, language detection, fallback, invalid files and stable IDs passed.\n";
    } catch (const std::exception& e) { std::cerr << e.what(); return 1; }
}
