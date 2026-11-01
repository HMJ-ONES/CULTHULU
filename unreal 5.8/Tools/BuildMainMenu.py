#!/usr/bin/env python3
"""Build the CULT-ULHU menu maps (run once in the UE editor).

Creates:
  /Game/Maps/L_MainMenu  - empty map; the C++ home screen shows on BeginPlay
  /Game/Maps/L_DarkWilds - empty map; BuildDarkWilds.py builds the world into it

Run in the editor: Tools -> Execute Python Script -> BuildMainMenu.py
Idempotent: existing maps are left alone.
"""
import unreal
from pathlib import Path

project = Path(__file__).resolve().parents[1]
assert project.name == 'unreal 5.8' and (project/'CultUlhu.uproject').is_file()
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve() == project

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


def build_menu_vista():
    """Diorama behind the main menu (runs AFTER BuildDarkWilds.build_wilds,
    so imported meshes + glow materials exist).

    A ring of glowing rune-spikes under a moon, wrapped in fog. The C++
    game instance points the menu camera (MENU_VistaCamera) at it, and the
    home screen's background is slightly translucent so the vista breathes
    through. Idempotent: clears previous MENU_* actors first.
    """
    ellt.load_level("/Game/Maps/L_MainMenu")

    # clear previous vista
    for a in list(ellt.get_all_level_actors()):
        try:
            if a.get_actor_label().startswith("MENU_"):
                ellt.destroy_actor(a)
        except Exception:
            pass

    def spawn(cls, label, loc=(0, 0, 0), rot=(0, 0, 0)):
        a = ellt.spawn_actor_from_class(
            cls, unreal.Vector(*loc), unreal.Rotator(*rot))
        a.set_actor_label(label)
        return a

    # camera: the game instance sets this as the view target
    cam = spawn(unreal.CameraActor, "MENU_VistaCamera",
                loc=(0, -1500, 400), rot=(-15, 90, 0))
    try:
        cam.camera_component.set_editor_property("field_of_view", 55.0)
    except Exception:
        pass

    # dim night rig (the menu is UI-first; this just lifts the diorama)
    try:
        moon = spawn(unreal.DirectionalLight, "MENU_MoonLight", rot=(-50, -30, 0))
        moon.light_component.set_intensity(1.2)
        moon.light_component.set_light_color(unreal.LinearColor(0.55, 0.68, 1.0, 1.0))
        sk = spawn(unreal.SkyLight, "MENU_SkyLight")
        sk.light_component.set_intensity(0.5)
        fog = spawn(unreal.ExponentialHeightFog, "MENU_Fog")
        fog.component.set_editor_property("fog_density", 0.016)
    except Exception:
        pass

    # ground disc
    try:
        plane = eal.load_asset("/Engine/BasicShapes/Plane")
        if plane:
            g = ellt.spawn_actor_from_object(
                plane, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
            g.set_actor_label("MENU_Ground")
            g.set_actor_scale3d(unreal.Vector(40, 40, 1))
            gm = eal.load_asset("/Game/CultUlhu/Materials/M_Ground_DarkWilds")
            if gm:
                g.static_mesh_component.set_material(0, gm)
    except Exception:
        pass

    # rune circle: 12 glowing spikes + scattered glowcaps
    import math
    try:
        spike = eal.load_asset("/Game/CultUlhu/World/SM_rune-spike")
        glow_rune = eal.load_asset("/Game/CultUlhu/Materials/M_Glow_Rune")
        if spike:
            for i in range(12):
                a_ = i / 12.0 * 2 * math.pi
                x, y = math.cos(a_) * 600, math.sin(a_) * 600
                s = ellt.spawn_actor_from_object(
                    spike, unreal.Vector(x, y, 0), unreal.Rotator(0, -i * 30, 0))
                s.set_actor_label("MENU_Rune_%02d" % i)
                s.set_actor_scale3d(unreal.Vector(1.6, 1.6, 2.2))
                if glow_rune:
                    try:
                        s.static_mesh_component.set_material(0, glow_rune)
                    except Exception:
                        pass
        cap = eal.load_asset("/Game/CultUlhu/World/SM_glowcap-ring")
        glow_cap = eal.load_asset("/Game/CultUlhu/Materials/M_Glow_Cap")
        if cap:
            for i, (x, y) in enumerate([(900, 300), (-800, 500), (400, -900),
                                        (-500, -700), (1100, -400)]):
                c = ellt.spawn_actor_from_object(
                    cap, unreal.Vector(x, y, 0), unreal.Rotator(0, 0, 0))
                c.set_actor_label("MENU_Glowcap_%d" % i)
                c.set_actor_scale3d(unreal.Vector(2.0, 2.0, 2.0))
                if glow_cap:
                    try:
                        c.static_mesh_component.set_material(0, glow_cap)
                    except Exception:
                        pass
        # moon in the sky
        moon_mat = eal.load_asset("/Game/CultUlhu/Materials/M_Glow_Moon")
        sphere = eal.load_asset("/Engine/BasicShapes/Sphere")
        if moon_mat and sphere:
            m = ellt.spawn_actor_from_object(
                sphere, unreal.Vector(-20000, 26000, 15000),
                unreal.Rotator(0, 0, 0))
            m.set_actor_label("MENU_Moon")
            m.set_actor_scale3d(unreal.Vector(14, 14, 14))
            m.static_mesh_component.set_material(0, moon_mat)
    except Exception as e:
        log("SKIP", "vista props: %s" % e)

    ellt.save_current_level()
    log("OK", "menu vista built in L_MainMenu")


def main():
    build_menu_maps()


if __name__ == "__main__":
    main()
