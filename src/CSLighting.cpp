#include <SKSE/SKSE.h>
#include <Windows.h>
#include <atomic>
#include <vector>
#include <filesystem>
#include <mutex>
#include "CSLighting.h"
#include "Localization.h"

namespace {
    enum class Support { pending, absent, loaded };
    std::atomic<Support> support = Support::pending;
    std::mutex diagnosticMutex;
    std::string diagnostic, dllVersion, islMetadata;
}

void CSLighting::Detect() {
    std::scoped_lock lock(diagnosticMutex);
    const auto module = GetModuleHandleW(L"CommunityShaders.dll");
    if (!module) {
        support = Support::absent;
        diagnostic = "CommunityShaders.dll: not loaded";
        SKSE::log::info("Community Shaders not loaded; using regular face light");
        return;
    }
    support = Support::loaded;
    diagnostic = "CommunityShaders.dll: loaded; version information unavailable";
    wchar_t path[32768]{};
    const auto length = GetModuleFileNameW(module, path, 32768);
    if (!length || length >= 32768) return;
    DWORD ignored{};
    const auto bytes = GetFileVersionInfoSizeW(path, &ignored);
    if (!bytes) return;
    std::vector<std::byte> buffer(bytes);
    if (!GetFileVersionInfoW(path, 0, bytes, buffer.data())) return;
    VS_FIXEDFILEINFO* version = nullptr;
    UINT size{};
    if (!VerQueryValueW(buffer.data(), L"\\", reinterpret_cast<void**>(&version), &size) ||
        size < sizeof(VS_FIXEDFILEINFO) || version->dwSignature != 0xFEEF04BD) return;
    const auto major = HIWORD(version->dwFileVersionMS);
    const auto minor = LOWORD(version->dwFileVersionMS);
    const auto patch = HIWORD(version->dwFileVersionLS);
    const auto build = LOWORD(version->dwFileVersionLS);
    // Feature files identify the data convention, not whether the user enabled a shader.
    // A version string alone cannot establish an experimental build's provenance.
    char islVersion[64]{};
    GetPrivateProfileStringA("Info", "Version", "", islVersion, sizeof(islVersion),
        ".\\Data\\Shaders\\Features\\InverseSquareLighting.ini");
    dllVersion = std::format("{}.{}.{}.{}", major, minor, patch, build);
    islMetadata = islVersion;
    SKSE::log::info("Community Shaders loaded: DLL={}, ISL={}; integration follows the user's switch, not a version whitelist",
        dllVersion, islMetadata.empty() ? "missing" : islMetadata);

}

bool CSLighting::Available(int mode) {
    const auto value = support.load();
    return ModeEnabled(mode, value == Support::loaded);
}

std::string CSLighting::Diagnostics(std::string_view language) {
    std::scoped_lock lock(diagnosticMutex);
    const auto tr = [&](const Localization::Text& text) { return text.Get(language); };
    if (support.load() == Support::pending) return tr(Localization::csPending);
    if (support.load() == Support::absent) return tr(Localization::diagNotLoaded);
    if (dllVersion.empty()) return tr(Localization::diagUnavailable);
    return std::format("DLL: {} | ISL: {}", dllVersion,
        islMetadata.empty() ? tr(Localization::diagMissing) : islMetadata.c_str());
}

const char* CSLighting::Status(std::string_view language, int mode) {
    if (mode == 2) return Localization::csOff.Get(language);
    if (Available(mode)) return Localization::csManual.Get(language);
    if (support.load() == Support::absent) return Localization::csAbsent.Get(language);
    return Localization::csPending.Get(language);
}
