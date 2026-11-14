# Build and play the canonical UE 5.8 project

Use Unreal Engine **5.8.3** and Visual Studio 2022 with the MSVC v143 C++ workload. The only active project is `unreal 5.8/CultUlhu.uproject`. Existing maps and character assets are already imported. No Blueprint input setup or map regeneration is needed.

From the repository root in PowerShell:

```powershell
$taskEngine = 'C:/Program Files/Epic Games/UE_5.8'
& "$taskEngine/Engine/Binaries/ThirdParty/Python3/Win64/python.exe" 'assets/decode_assets.py'
& "$taskEngine/Engine/Binaries/ThirdParty/Python3/Win64/python.exe" 'unreal 5.8/Tools/ValidateCultUlhu.py'
& './unreal 5.8/Tools/BuildCanonicalCore.ps1' -Test
& "$taskEngine/Engine/Build/BatchFiles/Build.bat" CultUlhuEditor Win64 Development "-Project=$PWD/unreal 5.8/CultUlhu.uproject" -WaitMutex -NoHotReloadFromIDE -NoUBA -MaxParallelActions=1
& "$taskEngine/Engine/Binaries/Win64/UnrealEditor.exe" "$PWD/unreal 5.8/CultUlhu.uproject"
```

Stop if a validation or build fails. The core helper uses the same installed MSVC toolchain and stages the library at `unreal 5.8/lib/Win64/cultulhu.lib`, the path used by `CultUlhuCore.Build.cs`. The separate root CMake project builds all headless suites; it is not a replacement for the canonical library helper.

Press Play from `L_MainMenu`. Free Roam opens its own menu: choose **New World** for character selection and three starting beliefs, **Continue Saved World** to resume, or **Join Co-op** to connect to a host. Esc pauses solo Free Roam and opens save/load, host/join, and save-and-exit controls; autosaves run every 60 unpaused seconds. See [Free Roam saves and co-op](FreeRoamSavesAndCoop.md). Competitive modes go directly to character selection without beliefs. Competitive modes retain their pause menu and return-home controls. Keep `DefaultEngine.ini`'s native game instance and play game mode; do not substitute legacy tutorial GameMode/Blueprint classes.

[Controls](EnhancedInputMapping.md), [artist next steps](CharacterModels_NextSteps.md), and [model status](ApprovedCharacterModels.md) describe the active pipeline. `BuildAll.py` now preserves existing authored maps and opens the main menu. It is not a required regeneration step. It refuses a partially missing map set; restore the missing authored map from Git instead of replacing working art.

For full core verification, decode assets first, configure the root CMake project with Visual Studio 17 2022 x64, build Release, then run `ctest --test-dir <build-directory> -C Release --output-on-failure`. Tests run from the source tree so decoded assets are found even when the build folder is nested. The native editor tests are separate and exercise actual Unreal behavior.
