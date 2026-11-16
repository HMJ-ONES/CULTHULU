#!/usr/bin/env python3
"""Open the canonical menu while preserving existing authored maps.

Run in the UE 5.8 editor. A complete existing map set is validated and opened,
never regenerated. A partially missing set requires restoration from Git.
The original initial-authoring sequence runs only when neither map exists.
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
    # Existing authored maps are the production input, not disposable output.
    from pathlib import Path
    import unreal
    project=Path(__file__).resolve().parents[1]
    assert project.name=='unreal 5.8' and (project/'CultUlhu.uproject').is_file()
    assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()==project
    maps=[unreal.EditorAssetLibrary.does_asset_exist('/Game/Maps/'+name) for name in ['L_MainMenu','L_DarkWilds']]
    if all(maps):
        log('OK','Authored maps already present; preserve all map contents and open the menu.')
        unreal.EditorLevelLibrary.load_level('/Game/Maps/L_MainMenu')
        return
    if any(maps):
        raise RuntimeError('A required authored map is missing. Restore it from Git; existing maps were preserved.')
    log("STEP", "1/4 — menu maps")
    import BuildMainMenu
    BuildMainMenu.build_menu_maps()

    log("STEP", "2/4 — dark wilds world (about a minute)")
    import BuildDarkWilds
    BuildDarkWilds.build_wilds()

    log("STEP", "3/4 — menu vista (the scene behind the title)")
    BuildMainMenu.build_menu_vista()

    log("STEP", "4/4 — opening the menu map")
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
    print("the 16 release characters, ENTER THE WILDS. WASD + mouse, Space jump,", flush=True)
    print("V toggles camera, Esc back to menu.", flush=True)
    print("DESTRUCTION: walk to the razed city and LEFT-CLICK to smash —", flush=True)
    print("columns, walls, towers, crypts, arches and obelisks break apart.", flush=True)
    print("BEASTS: feral beasts now roam the wilds — they chase and swipe.", flush=True)
    print("Smash them too. (If a beast walks sideways, tell me and I'll", flush=True)
    print("fix its facing — I can't see the models from here.)", flush=True)


if __name__ == "__main__":
    main()
