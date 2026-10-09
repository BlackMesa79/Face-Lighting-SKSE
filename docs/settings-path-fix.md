# Configuration path failure investigation

Release update (2026-10-08): included in 0.9.6 alongside the unified personal
light switches, which passed the user's local gameplay test. The original
configuration-save report still needs affected-user confirmation; 0.9.5 archives
remain unchanged. The test-build references below describe the earlier investigation.

2026-10-07. User report: Face Lighting 0.9.5 on Skyrim 1.6.1170 loads normally,
registers its menu and update hook, but repeatedly reports Windows error 3 on save.
Error 3 is ERROR_PATH_NOT_FOUND. It identifies a failed settings write, not an
engine light-count, Community Shaders or runtime layout failure.

Previous settings code used `.\Data\SKSE\Plugins\FaceLighting.ini` with ANSI
Windows Profile APIs and no parent-directory creation. The process working directory
can differ from the game root or change after startup. A missing INI alone normally
does not cause error 3 if its parent exists. The supplied log does not expose the
resolved path, so it cannot establish which condition caused this user's failure.

The local test build resolves the executable directory through GetModuleFileNameW,
anchors the settings path beneath its Data/SKSE/Plugins, uses Unicode Profile APIs,
and creates missing parent directories before both full-section and Enabled-key
writes. The directory comes from the process executable, not the plugin's physical
MO2 mod directory; existing mod-manager virtual paths remain subject to the manager's
normal interception. User mod-manager behavior still needs in-game confirmation.

Startup logs the absolute settings path. Write failures include that path, current
working directory and captured native error code. Failed writes retain saved settings;
full-save failure rolls back preview. Settings::Load does not create or overwrite INI.

Regression checks cover full settings round trip, a Unicode path with missing parents,
working-directory changes and an unrelated decoy INI, first Enabled-key save, and
parent blocked by a regular file with failed-save rollback. Test-only path injection
is compiled only in SettingsTests. Public 0.9.5 ZIPs are retained; local DLL logs
`0.9.5-settings-path-test.1` and keeps the 0.9.5.0 file resource version.

For affected-user validation, replace the DLL, save a visibly changed setting, exit
and restart to check persistence, then provide the new path/error lines if it fails.
Do not delete existing configuration or reintroduce a bundled defaults INI.
