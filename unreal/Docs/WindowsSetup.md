# Windows setup runbook (do once, then after big C++ changes)

## One-shot setup (PowerShell)

1. In **Git Bash**, sync the repo:
   `cd ~/Documents/CULTHULU && git pull origin game-core`
2. In **PowerShell**, run:
   `powershell -ExecutionPolicy Bypass -File "C:\Users\H\Documents\CULTHULU\unreal\Tools\SetupCultUlhu.ps1"`
   It decodes assets, generates project files, and compiles the C++ module.
   If it fails, send the full error text back — do not fight it.
3. Open the editor with the exact command:
   `"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "C:\Users\H\Documents\CULTHULU\unreal 5.8\CultUlhu.uproject"`
4. In the editor: **Tools -> Execute Python Script -> `unreal/Tools/BuildAll.py`**
   (builds L_MainMenu + menu vista, then the Dark Wilds world, then opens the menu)
5. With **L_MainMenu** open, press **Play**: the home screen appears
   (FREE ROAM / DOMINATION / DEFENSE OF THE GREAT OLD ONES).

## The acceptance run

Launch -> home screen -> FREE ROAM -> pick Cthulhu -> ENTER THE WILDS ->
visible world -> WASD + mouse, Space jump, V camera, LEFT-CLICK smash ->
Esc returns to the menu.

## If the menu is black / buttons missing

- The C++ module did not compile: close the editor, delete the `Binaries`
  and `Intermediate` folders inside `unreal 5.8`, rerun the ps1 script.
- Known fixed 2026-10-15: the widget tree is now built in `RebuildWidget()`
  (building it in `NativeConstruct()` left the Slate widget empty forever).

## What is still placeholder (honest list)

- The player is a shadowy vessel (dark capsule + glowing eyes), not the
  character model — no roster FBX has been dropped yet.
- Domination / DOTGOO navigate to the world with the right spawn, but the
  5v5 rules are not in the UE build yet (headless sim only).
- Destruction is mesh-swap + rubble + debris, not Chaos fracture.
- Beasts assume models face +X; if they walk sideways, report it.
