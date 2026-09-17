#include <SKSE/SKSE.h>
#include <Windows.h>
#include <atomic>
#include <vector>
#include <filesystem>
#include <mutex>
#include "CSLighting.h"
#include "Localization.h"

namespace {
    enum class Support { pending, absent, unsupported, supported, experimental };
    std::atomic<Support> support = Support::pending;
    std::mutex diagnosticMutex;
    std::string diagnostic, dllVersion, islMetadata;
    bool versionMatch = false, metadataMatch = false;
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
    support = Support::unsupported;
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
    const bool metadataMatches = std::string_view(islVersion) == "1-3-0";
    dllVersion = std::format("{}.{}.{}.{}", major, minor, patch, build);
    islMetadata = islVersion;
    versionMatch = SupportsVersion(major, minor, patch);
    metadataMatch = metadataMatches;
    diagnostic = std::format("DLL: {}.{}.{}.{} | ISL: {} | Version match: {} | Metadata match: {}",
        major, minor, patch, build, islVersion[0] ? islVersion : "missing",
        SupportsVersion(major, minor, patch), metadataMatches);
    std::error_code error;
    const bool experimentalFiles = std::filesystem::exists(
        "Data/Shaders/Features/PostProcessing.ini", error);
    if (SupportsVersion(major, minor, patch) && metadataMatches) {
        support = experimentalFiles ? Support::experimental : Support::supported;
    }
    SKSE::log::info("Community Shaders {}.{}.{}.{}: {}", major, minor, patch, build, Status());
    SKSE::log::info("CS metadata: ISL={}, PostProcessing files={}; this is protocol detection, not proof of enabled features or exact source commit",
        islVersion, experimentalFiles);
}

bool CSLighting::Available(int mode) {
    const auto value = support.load();
    return ModeEnabled(mode, value != Support::pending && value != Support::absent,
        value == Support::supported || value == Support::experimental);
}

std::string CSLighting::Diagnostics(std::string_view language) {
    std::scoped_lock lock(diagnosticMutex);
    const auto tr = [&](const Localization::Text& text) { return text.Get(language); };
    if (support.load() == Support::pending) return tr(Localization::csPending);
    if (support.load() == Support::absent) return tr(Localization::diagNotLoaded);
    if (dllVersion.empty()) return tr(Localization::diagUnavailable);
    return std::format("DLL: {} | ISL: {} | {}: {} | {}: {}", dllVersion,
        islMetadata.empty() ? tr(Localization::diagMissing) : islMetadata.c_str(),
        tr(Localization::diagVersion), tr(versionMatch ? Localization::yes : Localization::no),
        tr(Localization::diagMetadata), tr(metadataMatch ? Localization::yes : Localization::no));
}

const char* CSLighting::Status(std::string_view language, int mode) {
    if (mode == 2) return Localization::csOff.Get(language);
    if (mode == 1 && Available(mode)) return Localization::csManual.Get(language);
    switch (support.load()) {
    case Support::supported: return Localization::csSupported.Get(language);
    case Support::experimental: return Localization::csExperimental.Get(language);
    case Support::absent: return Localization::csAbsent.Get(language);
    case Support::unsupported: return Localization::csUnsupported.Get(language);
    default: return Localization::csPending.Get(language);
    }
}


