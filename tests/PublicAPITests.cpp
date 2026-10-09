#include "FaceLightingAPI.h"
#include "FaceLightingAPIV2.h"
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

        // A legacy save can disagree between the two sources. The hotkey must
        // turn off either active source, rather than just flip the selected row.
        selected = {{100, 0}};
        followers.disabled.clear();
        Check(PersonalLightPolicy::Enabled(selected, 100, true, true, true, true), "follower-only light ignored");
        Check(apply(false) == Result::Ok && !PersonalLightPolicy::Enabled(selected, 100, true,
            followers.Enabled(100), true, true), "legacy follower overlap survived off");
        selected[0].enabled = 1;
        Check(PersonalLightPolicy::Enabled(selected, 100, true, false, true, true), "selected-only light ignored");
        Check(apply(false) == Result::Ok && !followers.Enabled(100) && !selected[0].enabled,
            "legacy selected overlap survived off");
        Check(!PersonalLightPolicy::Enabled(selected, 100, true, true, false, false), "disabled groups count as on");
        Check(!PersonalLightPolicy::Enabled(selected, 100, false, true, true, true), "dismissed actor counted as follower");
        Check(apply(true) == Result::Ok && apply(false, false) == Result::Ok && !followers.Enabled(100),
            "dismissed selected actor left a re-recruitment preference enabled");

        // Menu edits remain possible with groups off and without an actor model.
        Check(apply(true) == Result::Ok && apply(false, true, false) == Result::Ok && !selected[0].enabled,
            "groups off blocked explicit personal off");
        selected.clear();
        Check(apply(true) == Result::Ok && selected.empty(), "follower menu added an unnecessary selected row");
        bool commitCalled = false;
        Check(PersonalLightPolicy::Set(selected, 100, false, true, true, true, true, [&](auto id, auto on) {
            return followers.SetWithCommit(id, on, [&] { commitCalled = true; return false; });
        }) == Result::ListFull && commitCalled && selected.empty() && followers.Enabled(100),
            "failed group save published selected or follower changes");
        // Test a staged off as well: a failed write must not insert a disable preference.
        selected = {{100, 1}};
        Check(PersonalLightPolicy::Set(selected, 100, true, true, false, true, true, [&](auto id, auto on) {
            return followers.SetWithCommit(id, on, [] { return false; });
        }) == Result::ListFull && selected[0].enabled == 1 && followers.Enabled(100),
            "failed commit partially disabled an actor");
        followers.disabled.resize(FollowerPreferences::limit);
        for (std::uint32_t i = 0; i < FollowerPreferences::limit; ++i) followers.disabled[i] = 1000 + i;
        commitCalled = false;
        Check(!followers.SetWithCommit(100, false, [&] { commitCalled = true; return true; }) && !commitCalled,
            "settings saved before preference capacity validation");

        wchar_t executable[MAX_PATH]{};
        GetModuleFileNameW(nullptr, executable, MAX_PATH);
        const auto dll = std::filesystem::path(executable).parent_path() / "FaceLighting.dll";
        // Standalone ABI smoke test only. Game clients must use GetModuleHandle.
        auto module = LoadLibraryW(dll.c_str());
        Check(module != nullptr, "cannot load built DLL for ABI smoke test");
        auto getAPI = reinterpret_cast<GetAPI>(GetProcAddress(module, exportName));
        Check(getAPI && !getAPI(0) && !getAPI(3), "export/version negotiation failed");
        const auto api = getAPI(version);
        Check(api && api->structSize == sizeof(Interface) && api->apiVersion == version, "wrong interface layout");
        const auto getV2 = reinterpret_cast<V2::GetAPI>(getAPI);
        const auto apiV2 = getV2(V2::version);
        Check(apiV2 && apiV2->structSize == sizeof(V2::Interface) && apiV2->apiVersion == 2 && apiV2->v1 == api,
            "V2 negotiation replaced or changed V1");
        Check(apiV2->BeginSession(nullptr, nullptr) == Result::InvalidArgument &&
            apiV2->UpdateSession(nullptr) == Result::InvalidArgument && apiV2->EndSession(nullptr) == Result::InvalidArgument,
            "V2 null inputs accepted");
        V2::BeginInfo begin; V2::Session temporary; V2::UpdateInfo update; V2::SessionState sessionState; V2::Environment environment; ActorState v2Actor;
        Check(apiV2->BeginSession(&begin, &temporary) == Result::WrongThread &&
            apiV2->UpdateSession(&update) == Result::WrongThread && apiV2->RenewSession(&temporary) == Result::WrongThread &&
            apiV2->EndSession(&temporary) == Result::WrongThread && apiV2->QuerySession(&temporary, &sessionState) == Result::WrongThread &&
            apiV2->ResolveActor(100, &v2Actor) == Result::WrongThread && apiV2->GetEnvironment(&environment) == Result::WrongThread,
            "V2 accessed engine outside game thread");
        sessionState.structSize = 0; environment.structSize = 0;
        Check(apiV2->QuerySession(&temporary, &sessionState) == Result::InvalidArgument &&
            apiV2->GetEnvironment(&environment) == Result::InvalidArgument, "V2 short outputs accepted");
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
