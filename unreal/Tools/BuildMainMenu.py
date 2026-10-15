#!/usr/bin/env python3
"""Build the CULT-ULHU menu maps (run once in the UE editor).

Creates:
  /Game/Maps/L_MainMenu  - empty map; the C++ home screen shows on BeginPlay
  /Game/Maps/L_DarkWilds - empty map; BuildDarkWilds.py builds the world into it

Run in the editor: Tools -> Execute Python Script -> BuildMainMenu.py
Idempotent: existing maps are left alone.
"""
import unreal

ellt = unreal.EditorLevelLibrary
eal = unreal.EditorAssetLibrary


def log(kind, msg):
    print("[%s] %s" % (kind, msg), flush=True)


def ensure_map(name):
    path = "/Game/Maps/" + name
    if eal.does_asset_exist(path):
        log("OK", "%s already exists" % path)
        return
    ellt.new_level(path)
    ellt.save_current_level()
    log("OK", "created %s" % path)


def build_menu_maps():
    log("OK", "building menu maps")
    ensure_map("L_MainMenu")
    ensure_map("L_DarkWilds")
    log("OK", "done. Set PIE to start at L_MainMenu (Config/DefaultEngine.ini does this).")


def main():
    build_menu_maps()


if __name__ == "__main__":
    main()
