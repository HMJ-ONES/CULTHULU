#!/usr/bin/env python3
"""CULT-ULHU plug-and-play builder — run this ONE script in the UE editor.

Does everything in the right order:
  1. Creates /Game/Maps/L_MainMenu  (the menu — supposed to look empty,
     the buttons are drawn by code, not placed in the map)
  2. Creates /Game/Maps/L_DarkWilds (the world map shell)
  3. Builds the whole Dark Wilds world into L_DarkWilds: terrain, water,
     props, creatures, lighting, fog, reflections, player starts
  4. Opens L_MainMenu so you can press Play right away

Run: Tools -> Execute Python Script -> BuildAll.py
Safe to re-run after every `git pull` (rebuilds instead of duplicating).

If L_MainMenu plays as a BLACK screen with no buttons, the C++ module
did not compile — reopen the project and accept the rebuild prompt,
then press Play again. See unreal/Docs/MainMenu.md.
"""
import os
import sys
import traceback

_here = os.path.dirname(os.path.abspath(__file__))
if _here not in sys.path:
    sys.path.insert(0, _here)


def log(kind, msg):
    print("[%s] %s" % (kind, msg), flush=True)


def main():
    log("STEP", "1/3 — menu maps")
    import BuildMainMenu
    BuildMainMenu.build_menu_maps()

    log("STEP", "2/3 — dark wilds world (about a minute)")
    import BuildDarkWilds
    BuildDarkWilds.build_wilds()

    log("STEP", "3/3 — opening the menu map")
    try:
        import unreal
        unreal.EditorLevelLibrary.load_level("/Game/Maps/L_MainMenu")
    except Exception as e:
        log("ERROR", "could not open L_MainMenu: %s" % e)
        traceback.print_exc()

    log("OK", "plug-and-play build done")
    print("", flush=True)
    print("WHAT YOU HAVE NOW:", flush=True)
    print("  L_MainMenu  = the menu. Empty on purpose — press Play and the", flush=True)
    print("                HOME screen (3 mode buttons) is drawn by the game code.", flush=True)
    print("  L_DarkWilds = the world. Terrain, props, creatures, lighting,", flush=True)
    print("                and 3 player starts are already placed.", flush=True)
    print("NEXT: with L_MainMenu open, press Play. Click a mode, pick one of", flush=True)
    print("the 20 characters, ENTER THE WILDS. WASD + mouse, Space jump,", flush=True)
    print("V toggles camera, Esc back to menu.", flush=True)


if __name__ == "__main__":
    main()
