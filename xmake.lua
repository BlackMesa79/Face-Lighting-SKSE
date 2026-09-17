set_project("FaceLighting")
set_version("0.8.2")
set_languages("cxx23")
set_encodings("utf-8")

set_config("skyrim_se", true)
set_config("skyrim_ae", true)
set_config("skyrim_vr", false)
set_config("tests", false)

add_rules("mode.debug", "mode.releasedbg")
includes("extern/CommonLibVR")

option("deploy_dir")
    set_default("")
    set_showmenu(true)
    set_description("Optional local SKSE/Plugins directory for development deployment")
option_end()

target("FaceLighting")
    set_kind("shared")
    add_rules("commonlibsse-ng.plugin", {
        name = "FaceLighting",
        author = "BlackMesa79",
        description = "Face Lighting"
    })
    add_files("src/*.cpp")
    add_deps("commonlibsse-ng")
    add_includedirs("include", "extern/SKSEMenuFrameworkAPI")
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN", "_CRT_SECURE_NO_WARNINGS")
    -- Keep the full package local; deploy the DLL to the development mod separately.
    set_installdir("build/package/FaceLighting")
    add_installfiles("FaceLighting.ini", {prefixdir = "SKSE/Plugins"})
    add_installfiles("README.md")
    add_installfiles("LICENSE", "THIRD_PARTY_NOTICES.md")
    add_installfiles("languages/*.ini", {prefixdir = "SKSE/Plugins/FaceLighting/Languages"})
    add_installfiles("docs/CS-compatibility.md", {prefixdir = "docs"})
    add_installfiles("docs/LOCALIZATION.md", {prefixdir = "docs"})
    add_installfiles("extern/CommonLibVR/LICENSE", {prefixdir = "licenses/CommonLibVR"})
    add_installfiles("extern/SKSEMenuFrameworkAPI/LICENSE", {prefixdir = "licenses/SKSEMenuFrameworkAPI"})
    after_build(function(target)
        local pluginsdir = get_config("deploy_dir")
        if not pluginsdir or pluginsdir == "" then return end
        os.mkdir(pluginsdir)
        os.cp(target:targetfile(), path.join(pluginsdir, "FaceLighting.dll"))
        os.mkdir(path.join(pluginsdir, "FaceLighting/Languages"))
        os.cp("languages/*.ini", path.join(pluginsdir, "FaceLighting/Languages"))
        cprint("FaceLighting.dll deployed to %s", pluginsdir)
    end)

target("SettingsTests")
    set_kind("binary")
    set_default(false)
    add_includedirs("include")
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")
    add_deps("commonlibsse-ng")
    add_files("tests/SettingsTests.cpp", "src/Settings.cpp", "src/Localization.cpp")

target("LightPlacementTests")
    set_kind("binary")
    set_default(false)
    add_includedirs("include")
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")
    add_deps("commonlibsse-ng")
    add_files("tests/LightPlacementTests.cpp")

target("CSLightingTests")
    set_kind("binary")
    set_default(false)
    add_includedirs("include")
    add_files("tests/CSLightingTests.cpp")




target("LocalizationTests")
    set_kind("binary")
    set_default(false)
    add_includedirs("include")
    add_defines("NOMINMAX", "WIN32_LEAN_AND_MEAN")
    add_files("tests/LocalizationTests.cpp", "src/Localization.cpp")



target("PlayerDialogueTests")
    set_kind("binary")
    set_default(false)
    add_includedirs("include")
    add_files("tests/PlayerDialogueTests.cpp")

target("NpcLightManagerTests")
    set_kind("binary")
    set_default(false)
    add_includedirs("include")
    add_files("tests/NpcLightManagerTests.cpp")

target("SelectedNPCRecordTests")
    set_kind("binary")
    set_default(false)
    add_includedirs("include")
    add_files("tests/SelectedNPCRecordTests.cpp")
