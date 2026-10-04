#include "FaceLightingAPI.h"
#include "PublicAPIPolicy.h"
#include "PersonalLightPolicy.h"
#include "FollowerPreferences.h"
#include <Windows.h>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
}
int main() {
    using namespace FaceLightingAPI;
    try {
        Check(PublicAPIPolicy::ValidateToken({9, 1, 2}, 10) == Result::StaleSession, "old save token accepted");
        Check(PublicAPIPolicy::ValidateToken({10, 0, 2}, 10) == Result::InvalidTarget, "missing handle accepted");
        Check(PublicAPIPolicy::ValidateToken({10, 1, 2}, 10) == Result::Ok, "valid token rejected");
        char name[256]{};
        PublicAPIPolicy::CopyName(name, std::string(254, 'x') + "\xE4\xB8\xAD");
        Check(std::strlen(name) == 254, "UTF-8 truncation split a character");
        PublicAPIPolicy::CopyName(name, std::string(253, 'x') + "\xC3\xA9");
        Check(std::strlen(name) == 255, "complete UTF-8 suffix lost");
        PublicAPIPolicy::CopyName(name, ""); Check(name[0] == 0, "empty name retained old data");
        std::vector<SelectedNPCRecord::Record> selected{{100, 1}};
        FollowerPreferences followers;
        auto apply = [&](bool enabled, bool teammate = true, bool group = true) {
            return PersonalLightPolicy::Set(selected, 100, teammate, followers.Enabled(100), enabled, group, group,
                [&](auto id, auto value) { return followers.Set(id, value); });
        };
        Check(apply(false) == Result::Ok && !followers.Enabled(100) && selected[0].enabled == 0, "dual source off failed");
        Check(apply(false) == Result::NoChange, "explicit off is not idempotent");
        Check(apply(true, true, false) == Result::SourceDisabled && !followers.Enabled(100), "group off bypassed");
        Check(apply(true) == Result::Ok && followers.Enabled(100) && selected[0].enabled == 1, "dual source restore failed");
        Check(PersonalLightPolicy::Set(selected, 100, true, true, false, true, true,
            [](auto, auto) { return false; }) == Result::ListFull && selected[0].enabled == 1, "partial mutation after failure");
        selected.clear();
        Check(apply(false, false) == Result::NoChange && selected.empty(), "off added an unregistered actor");
        Check(followers.Set(100, false), "failed to stage a former follower preference");
        Check(apply(true, false) == Result::Ok && selected.size() == 1 && followers.Enabled(100),
            "new NPC did not synchronize former follower preference");
        selected.clear();
        Check(apply(true) == Result::NoChange && selected.empty(), "teammate unnecessarily added to selected list");
        for (std::uint32_t i = 1; i <= SelectedNPCRecord::limit; ++i) selected.push_back({i + 1000, 1});
        Check(apply(true, false) == Result::ListFull && selected.size() == SelectedNPCRecord::limit, "selected budget bypassed");
        Check(apply(false) == Result::Ok && !followers.Enabled(100), "full selected roster blocked follower switch");
        selected = {{100, 1}};
        followers.disabled.clear();
        for (std::uint32_t i = 0; i < FollowerPreferences::limit; ++i) followers.disabled.push_back(i + 1000);
        Check(apply(false) == Result::ListFull && selected[0].enabled == 1 && followers.Enabled(100),
            "full follower preferences partially disabled selected source");
        Check(followers.Set(1000, true) && apply(false) == Result::Ok, "preference capacity did not recover");

        wchar_t executable[MAX_PATH]{};
        GetModuleFileNameW(nullptr, executable, MAX_PATH);
        const auto dll = std::filesystem::path(executable).parent_path() / "FaceLighting.dll";
        // Standalone ABI smoke test only. Game clients must use GetModuleHandle.
        auto module = LoadLibraryW(dll.c_str());
        Check(module != nullptr, "cannot load built DLL for ABI smoke test");
        auto getAPI = reinterpret_cast<GetAPI>(GetProcAddress(module, exportName));
        Check(getAPI && !getAPI(0) && !getAPI(2), "export/version negotiation failed");
        const auto api = getAPI(version);
        Check(api && api->structSize == sizeof(Interface) && api->apiVersion == version, "wrong interface layout");
        Check(api->Execute(nullptr) == Result::InvalidArgument && api->QueryPlayer(nullptr) == Result::InvalidArgument,
            "invalid output/request not rejected");
        Context context; ActorState state; Token token{1, 1, 100}; FollowerPage page;
        Check(api->GetContext(&context) == Result::WrongThread, "uninitialized game thread accepted");
        Check(api->CaptureTarget(&state) == Result::WrongThread && api->QueryActor(&token, &state) == Result::WrongThread,
            "game data accessed outside game thread");
        Check(api->EnumerateFollowers(&page) == Result::WrongThread, "roster accessed outside game thread");
        Request request; request.target.session = 1; request.revision = 1;
        Check(api->Execute(&request) == Result::WrongThread, "mutation accepted outside game thread");
        context.structSize = 0;
        Check(api->GetContext(&context) == Result::InvalidArgument, "short ABI buffer accepted");
        // Keep the DLL until process exit: engine resources have no standalone shutdown API.
        std::cout << "Public API ABI, exported version negotiation, thread guards, UTF-8, tokens and atomic preferences passed.\n";
    } catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
