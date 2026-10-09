#include <SKSE/SKSE.h>
#include <RE/Skyrim.h>
#include <Windows.h>
#include <array>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
    void Check(bool value, const char* message) { if (!value) throw std::runtime_error(message); }
    template <class T> void Write(std::ofstream& out, const T& value) {
        out.write(reinterpret_cast<const char*>(&value), sizeof(value));
    }
    void WriteV5(const std::filesystem::path& path, REL::Version version) {
        std::ofstream out(path, std::ios::binary);
        const std::int32_t format = 5, pointerSize = 8, dataFormat = 0;
        const std::array<std::uint32_t, 4> parts{version[0], version[1], version[2], version[3]};
        std::array<char, 64> name{};
        std::memcpy(name.data(), "SkyrimSE.exe", 12);
        const std::int32_t count = 390952;
        Write(out, format); Write(out, parts); Write(out, name);
        Write(out, pointerSize); Write(out, dataFormat); Write(out, count);
        std::vector<std::uint32_t> offsets(count);
        offsets[106326] = 0x102030; // Synthetic AddLight AE ID.
        offsets[390951] = 0x203040; // Synthetic shader state AE ID.
        out.write(reinterpret_cast<const char*>(offsets.data()), offsets.size() * sizeof(offsets[0]));
        Check(bool(out), "could not write synthetic format-5 fixture");
    }
    bool updateCalled = false;
    void Update(RE::Actor*, float delta) { updateCalled = delta == 0.25f; }
    void WrongUpdate(RE::Actor*, float) { throw std::runtime_error("wrong actor update virtual slot"); }
    __declspec(noinline) void InvokeUpdate(RE::Actor* actor) { actor->Update(0.25f); }
}

int main() {
    const auto fixture = std::filesystem::temp_directory_path() /
        ("FaceLighting-v5-" + std::to_string(GetCurrentProcessId()) + ".bin");
    try {
        struct Layout { REL::Version version; std::size_t actor, player, control; };
        const Layout layouts[] = {
            {{1, 5, 97, 0}, 0xE0, 0x3D8, 0xE8},
            {{1, 6, 353, 0}, 0xE0, 0x3D8, 0xE8},
            {{1, 6, 629, 0}, 0xE8, 0x3E0, 0xE8},
            {{1, 6, 1170, 0}, 0xE8, 0x3E0, 0xF0},
            {{1, 7, 99, 0}, 0xE8, 0x3E8, 0xF0},
            {{1, 7, 104, 0}, 0xE8, 0x3E8, 0xF0}
        };
        for (const auto& layout : layouts) {
            Check(REL::Module::mock(layout.version), "runtime mock failed");
            Check(REL::Module::IsAE() == (layout.version.minor() >= 6), "1.7 runtime misclassified as SE");
            alignas(16) std::array<std::byte, 0x2000> storage{};
            auto* actor = reinterpret_cast<RE::Actor*>(storage.data());
            auto* player = reinterpret_cast<RE::PlayerCharacter*>(storage.data());
            auto* controls = reinterpret_cast<RE::ControlMap*>(storage.data());
            Check(reinterpret_cast<const std::byte*>(&actor->GetActorRuntimeData()) == storage.data() + layout.actor,
                "actor runtime block offset incorrect");
            Check(reinterpret_cast<const std::byte*>(&player->GetPlayerRuntimeData()) == storage.data() + layout.player,
                "player 1.7 base shift missing");
            Check(reinterpret_cast<const std::byte*>(&controls->GetRuntimeData()) == storage.data() + layout.control,
                "control-map runtime block offset incorrect");
            const auto* point = reinterpret_cast<RE::NiPointLight*>(storage.data());
            Check(reinterpret_cast<const std::byte*>(&point->GetLightRuntimeData()) == storage.data() + 0x110,
                "flat light data offset changed");
            Check(reinterpret_cast<const std::byte*>(&point->GetPointLightRuntimeData()) == storage.data() + 0x140,
                "flat point attenuation offset changed");
            // Poison the neighboring pointer slots; the correct currentProcess field is +0x10.
            const std::uintptr_t marker = 0x12345678;
            std::memset(storage.data() + 0xE0, 0xFF, 0x40);
            std::memcpy(storage.data() + layout.actor + 0x10, &marker, sizeof(marker));
            Check(reinterpret_cast<std::uintptr_t>(actor->GetActorRuntimeData().currentProcess) == marker,
                "currentProcess reads the wrong native offset");
            std::array<std::uintptr_t, 0x200> vtable;
            vtable.fill(reinterpret_cast<std::uintptr_t>(&WrongUpdate));
            vtable[0xAD] = reinterpret_cast<std::uintptr_t>(&Update);
            auto* table = vtable.data();
            std::memcpy(storage.data(), &table, sizeof(table));
            updateCalled = false; InvokeUpdate(actor);
            Check(updateCalled, "actor Update slot no longer 0xAD");
        }
        Check(REL::IDDB::inject(L"extern/CommonLibVR/tests/REL/version-1-5-97-0.bin", REL::Version(1, 5, 97, 0)),
            "legacy SE address library stopped loading");
        Check(REL::IDDB::inject(L"extern/CommonLibVR/tests/REL/versionlib-1-6-1170-0.bin", REL::Version(1, 6, 1170, 0)),
            "legacy AE address library stopped loading");
        Check(REL::IDDB::get().id2offset(11483) == 0x14FCD0, "legacy AE address lookup changed");
        for (const auto version : {SKSE::RUNTIME_SSE_1_7_99, SKSE::RUNTIME_SSE_1_7_104}) {
            Check(REL::Module::mock(version), "1.7 runtime mock failed");
            WriteV5(fixture, version);
            Check(REL::IDDB::inject(fixture.wstring(), version), "format-5 auto-detection failed");
            Check(REL::RelocationID(99692, 106326).address() == 0x102030, "wrong AddLight ID branch on 1.7");
            Check(REL::RelocationID(513211, 390951).address() == 0x203040, "wrong shader state ID branch on 1.7");
            REL::IDDB::reset();
        }
        const auto dllPath = std::filesystem::absolute("build/windows/x64/release/FaceLighting.dll");
        const auto dll = LoadLibraryExW(dllPath.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
        Check(dll != nullptr, "could not load experimental plugin");
        const auto* info = reinterpret_cast<const SKSE::PluginVersionData*>(GetProcAddress(dll, "SKSEPlugin_Version"));
        Check(info && info->GetPluginName() == "FaceLighting", "plugin metadata missing");
        Check(info->versionIndependenceEx & SKSE::PluginVersionData::kVersionIndependentEx_AddressLibraryV5,
            "SKSE metadata lacks Address Library v5 compatibility flag");
        Check(info->versionIndependence & SKSE::PluginVersionData::kVersionIndependent_AddressLibraryPostAE,
            "SKSE metadata lost existing AE compatibility flag");
        Check(GetProcAddress(dll, "FaceLighting_GetAPI") != nullptr, "public API export lost");
        FreeLibrary(dll);
        REL::IDDB::reset(); REL::Module::reset();
        std::filesystem::remove(fixture);
        std::cout << "SE/AE/1.7 native runtime blocks, actor Update dispatch, address-library formats 1/2/5 and plugin metadata passed.\n";
    } catch (const std::exception& e) {
        REL::IDDB::reset();
        std::error_code ec; std::filesystem::remove(fixture, ec);
        std::cerr << e.what() << '\n'; return 1;
    }
}
