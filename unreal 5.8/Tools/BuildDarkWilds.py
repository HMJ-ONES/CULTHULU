# BuildDarkWilds.py — CULT-ULHU dark wilds one-click world builder.
#
# RUN IT: in the UE5 editor, open Tools > Execute Python Script (or the
# Python console: `exec(open(r"<repo>/unreal 5.8/Tools/BuildDarkWilds.py").read())`)
# with the persistent level open. It is IDEMPOTENT — rerun it after every
# `git pull` and the level rebuilds from the current repo data.
#
# WHAT IT DOES:
#   1. Imports the terrain OBJ + detail texture + prop/creature GLBs from the
#      repo (run `python assets/decode_assets.py` after cloning first).
#   2. Builds the ground material (detail texture x vertex colors) and a
#      swamp-water material, then places the terrain mesh + water plane.
#   3. Reads assets/maps/dark_wilds.map and scatters every placement as an
#      instanced static mesh (one actor per model), plus feral-beast spawns.
#   4. Builds the night lighting rig: moonlight, skylight, sky atmosphere,
#      exponential height fog, and 4 reflection captures (this is what makes
#      the ground and water actually reflect).
#
# UNITS: the .map files are meters, Y-up. UE is centimeters, Z-up.
#   UE = (x_m * 100, z_m * 100, y_m * 100).
#
# If any section prints [ERROR], paste the message back — the script keeps
# going so one bad asset never blocks the rest of the world.

import os
import traceback

REPO = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                     "..", ".."))
MAP_PATH = os.path.join(REPO, "assets", "maps", "dark_wilds.map")
TERRAIN_OBJ = os.path.join(REPO, "assets", "world", "terrain",
                           "dark_wilds_ground.obj")
DETAIL_TEX = os.path.join(REPO, "assets", "world", "textures",
                          "ground-detail-dark.png")

DEST_TERRAIN = "/Game/CultUlhu/Terrain"
DEST_WORLD = "/Game/CultUlhu/World"
DEST_CREATURES = "/Game/CultUlhu/Creatures"
DEST_MAT = "/Game/CultUlhu/Materials"

# Razed-city structures that become ACultDestructible actors (per-building
# HP) instead of instanced statics. Basenames of the .map model paths.
DESTRUCTIBLE_MODELS = {
    "stone-wall-damaged",
    "ruined-column-b",
    "wall-broken",
    "crypt-ruined",
    "arch-dungeon",
    "tower-square-mid",
    "tower-square-base",
    "broken-obelisk",
}

CM = 100.0


def log(tag, msg):
    print("[%s] %s" % (tag, msg))


def section(name, fn):
    try:
        fn()
        log("OK", name)
    except Exception as e:  # noqa: BLE001 — keep building the rest
        log("ERROR", "%s: %s" % (name, e))
        traceback.print_exc()


# --------------------------------------------------------------------------
# cleanup: make every rerun a true rebuild, not a duplicate pile.
# Without this, rerunning after a `git pull` would spawn a second moon,
# a second fog, 36 more prop anchors, and keep appending orphaned
# expression nodes to the two materials.
# --------------------------------------------------------------------------

def cleanup_previous():
    import unreal
    ell = unreal.EditorLevelLibrary
    eal = unreal.EditorAssetLibrary
    prefixes = ("WILD_", "SM_DarkWilds_Terrain", "SM_SwampWater")
    removed = 0
    try:
        actors = ell.get_all_level_actors()
    except Exception:
        actors = []
    for a in actors:
        try:
            label = a.get_actor_label()
        except Exception:
            continue
        if label.startswith(prefixes):
            try:
                a.destroy_actor()
                removed += 1
            except Exception:
                pass
    for m in (DEST_MAT + "/M_Ground_DarkWilds",
              DEST_MAT + "/M_SwampWater"):
        try:
            if eal.does_asset_exist(m):
                eal.delete_asset(m)
        except Exception:
            pass
    log("OK", "cleared previous run (%d actors)" % removed)


# --------------------------------------------------------------------------
# .map parsing (place + spawn lines only)
# --------------------------------------------------------------------------

def parse_map(path):
    places, spawns = [], []
    with open(path, "r", encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            t = line.split()
            if t[0] == "place" and len(t) >= 8:
                places.append({
                    "model": t[1],
                    "x": float(t[2]), "y": float(t[3]), "z": float(t[4]),
                    "rot": float(t[5]), "scale": float(t[6]),
                    "zone": t[7],
                })
            elif t[0] == "spawn" and len(t) >= 7:
                spawns.append({
                    "species": t[1],
                    "x": float(t[2]), "y": float(t[3]), "z": float(t[4]),
                    "count": int(float(t[5])),
                    "zone": t[6],
                })
    return places, spawns


def to_ue(x, y, z):
    """Map (x, y-up, z) meters -> UE (x, y, z-up) centimeters."""
    return (x * CM, z * CM, y * CM)


# --------------------------------------------------------------------------
# asset import
# --------------------------------------------------------------------------

def import_one(asset_tools, os_path, dest):
    import unreal
    if not os.path.exists(os_path):
        log("SKIP", "missing on disk (run assets/decode_assets.py): %s"
            % os_path)
        return None
    task = unreal.AssetImportTask()
    task.filename = os_path
    task.destination_path = dest
    task.save = True
    task.automated = True
    task.replace_existing = True
    asset_tools.import_asset_tasks([task])
    for p in task.imported_object_paths:
        log("OK", "imported %s" % p)
        return p
    log("SKIP", "import produced nothing: %s" % os_path)
    return None


def import_assets():
    import unreal
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    imported = {}

    imported["terrain"] = import_one(tools, TERRAIN_OBJ, DEST_TERRAIN)
    imported["detail"] = import_one(tools, DETAIL_TEX, DEST_TERRAIN)

    places, spawns = parse_map(MAP_PATH)
    models = sorted({p["model"] for p in places})
    for m in models:
        os_path = os.path.join(REPO, *m.split("/"))
        name = os.path.splitext(os.path.basename(m))[0]
        dest_name = "/Game/CultUlhu/World/SM_" + name
        if unreal.EditorAssetLibrary.does_asset_exist(dest_name):
            imported[m] = dest_name
            continue
        p = import_one(tools, os_path, DEST_WORLD)
        imported[m] = p or dest_name

    for s in spawns:
        key = "/Game/CultUlhu/Creatures/SK_" + s["species"]
        if unreal.EditorAssetLibrary.does_asset_exist(key):
            continue
        # creature GLBs live at assets/creatures/<species>.glb
        os_path = os.path.join(REPO, "assets", "creatures",
                               s["species"] + ".glb")
        import_one(tools, os_path, DEST_CREATURES)

    # stash for later sections
    import_assets.places = places
    import_assets.spawns = spawns
    import_assets.imported = imported
    log("OK", "%d placements, %d spawn entries parsed" %
        (len(places), len(spawns)))


# --------------------------------------------------------------------------
# materials
# --------------------------------------------------------------------------

def build_materials():
    import unreal
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mel = unreal.MaterialEditingLibrary

    def get_or_create(name):
        path = DEST_MAT + "/" + name
        mat = unreal.EditorAssetLibrary.load_asset(path)
        if mat:
            return mat, False
        return (tools.create_asset(name, DEST_MAT, unreal.Material,
                                   unreal.MaterialFactoryNew()), True)

    # --- ground: detail texture x vertex colors, rough dark soil ---
    # VERTEX-COLOR SAFETY: the tint multiplies the detail texture. If the
    # OBJ importer ever drops the vertex colors (or bakes them black), the
    # ground would render pitch black. The clamp floors the tint at 0.30 so
    # the detail texture always keeps the terrain visible.
    g, g_fresh = get_or_create("M_Ground_DarkWilds")
    if g_fresh:
        tex_path = DEST_TERRAIN + "/ground-detail-dark"
        tex = unreal.EditorAssetLibrary.load_asset(tex_path)
        if tex:
            ts = mel.create_material_expression(
                g, unreal.MaterialExpressionTextureSample, -400, 100)
            ts.texture = tex
            # tile the 512px detail every ~60m: texture coordinate scale
            tc = mel.create_material_expression(
                g, unreal.MaterialExpressionTextureCoordinate, -600, 100)
            mel.connect_material_expressions(tc, "UV", ts, "UV")
            vc = mel.create_material_expression(
                g, unreal.MaterialExpressionVertexColor, -400, -100)
            try:
                cl = mel.create_material_expression(
                    g, unreal.MaterialExpressionClamp, -275, -100)
                cl.set_editor_property("min_default", 0.30)
                cl.set_editor_property("max_default", 1.0)
                mel.connect_material_expressions(vc, "RGB", cl, "Input")
                tint_out, tint_pin = cl, ""
            except Exception:
                tint_out, tint_pin = vc, "RGB"  # clamp unsupported: raw tint
            mul = mel.create_material_expression(
                g, unreal.MaterialExpressionMultiply, -150, 0)
            mel.connect_material_expressions(ts, "RGB", mul, "A")
            mel.connect_material_expressions(tint_out, tint_pin, mul, "B")
            mel.connect_material_property(mul, "", unreal.MaterialProperty.MP_BASE_COLOR)
        rough = mel.create_material_expression(
            g, unreal.MaterialExpressionScalarParameter, -150, 200)
        rough.set_editor_property("parameter_name", "Roughness")
        rough.set_editor_property("default_value", 0.88)
        mel.connect_material_property(rough, "",
                                      unreal.MaterialProperty.MP_ROUGHNESS)
        unreal.EditorAssetLibrary.save_asset(DEST_MAT + "/M_Ground_DarkWilds")
    build_materials.ground = DEST_MAT + "/M_Ground_DarkWilds"

    # --- swamp water: dark, translucent, mirror-smooth, slow shimmer ---
    w, w_fresh = get_or_create("M_SwampWater")
    if w_fresh:
        w.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        bc = mel.create_material_expression(
            w, unreal.MaterialExpressionVectorParameter, -300, 0)
        bc.set_editor_property("parameter_name", "WaterColor")
        bc.set_editor_property("default_value", unreal.LinearColor(0.03, 0.07, 0.05, 0.88))
        try:
            # drifting shimmer: pan the ground detail texture slowly and
            # fold a whisper of it into the base color
            tex = unreal.EditorAssetLibrary.load_asset(
                DEST_TERRAIN + "/ground-detail-dark")
            if tex:
                tc = mel.create_material_expression(
                    w, unreal.MaterialExpressionTextureCoordinate, -700, 100)
                pan = mel.create_material_expression(
                    w, unreal.MaterialExpressionPanner, -500, -30)
                pan.set_editor_property("speed_x", 0.008)
                pan.set_editor_property("speed_y", 0.013)
                mel.connect_material_expressions(tc, "UV", pan, "UV")
                ts = mel.create_material_expression(
                    w, unreal.MaterialExpressionTextureSample, -350, -30)
                ts.texture = tex
                mel.connect_material_expressions(pan, "", ts, "UV")
                shim = mel.create_material_expression(
                    w, unreal.MaterialExpressionMultiply, -150, -30)
                mel.connect_material_expressions(ts, "RGB", shim, "A")
                mel.connect_material_expressions(bc, "RGB", shim, "B")
                mel.connect_material_property(shim, "",
                                              unreal.MaterialProperty.MP_BASE_COLOR)
            else:
                raise RuntimeError("no detail tex")
        except Exception:
            mel.connect_material_property(bc, "",
                                          unreal.MaterialProperty.MP_BASE_COLOR)
        wr = mel.create_material_expression(
            w, unreal.MaterialExpressionScalarParameter, -300, 200)
        wr.set_editor_property("parameter_name", "Roughness")
        wr.set_editor_property("default_value", 0.06)
        mel.connect_material_property(wr, "", unreal.MaterialProperty.MP_ROUGHNESS)
        unreal.EditorAssetLibrary.save_asset(DEST_MAT + "/M_SwampWater")
    build_materials.water = DEST_MAT + "/M_SwampWater"


# --------------------------------------------------------------------------
# terrain + water actors
# --------------------------------------------------------------------------

def place_terrain():
    import unreal
    ell = unreal.EditorLevelLibrary
    eal = unreal.EditorAssetLibrary

    terrain_asset = eal.load_asset(DEST_TERRAIN + "/dark_wilds_ground")
    if not terrain_asset:
        raise RuntimeError("terrain static mesh not imported: %s/dark_wilds_ground"
                           % DEST_TERRAIN)
    actor = ell.spawn_actor_from_object(
        terrain_asset, unreal.Vector(0, 0, 0),
        unreal.Rotator(0, 0, 0))
    actor.set_actor_label("SM_DarkWilds_Terrain")
    ground_mat = eal.load_asset(build_materials.ground)
    if ground_mat:
        comp = actor.static_mesh_component
        comp.set_material(0, ground_mat)

    # swamp water sheet: basin sits ~-3.5m, water at -1.0m
    plane = eal.load_asset("/Engine/BasicShapes/Plane")
    water_mat = eal.load_asset(build_materials.water)
    if plane:
        wactor = ell.spawn_actor_from_object(
            plane, unreal.Vector(150 * CM, -425 * CM, -1.0 * CM),
            unreal.Rotator(0, 0, 0))
        wactor.set_actor_label("SM_SwampWater")
        wactor.set_actor_scale3d(unreal.Vector(400, 250, 1))
        if water_mat:
            wactor.static_mesh_component.set_material(0, water_mat)


# --------------------------------------------------------------------------
# FX materials (unlit, for the C++ blast/debris effects)
# --------------------------------------------------------------------------

def build_fx_materials():
    import unreal
    tools = unreal.AssetToolsHelpers.get_asset_tools()
    mel = unreal.MaterialEditingLibrary

    def make_unlit(name, color):
        path = DEST_MAT + "/" + name
        mat = unreal.EditorAssetLibrary.load_asset(path)
        if mat:
            return
        mat = tools.create_asset(name, DEST_MAT, unreal.Material,
                                 unreal.MaterialFactoryNew())
        mat.set_editor_property("shading_model",
                                unreal.MaterialShadingModel.MSM_UNLIT)
        c = mel.create_material_expression(
            mat, unreal.MaterialExpressionConstant3Vector, -400, 0)
        c.set_editor_property("constant", unreal.LinearColor(*color))
        mel.connect_material_property(c, "",
                                      unreal.MaterialProperty.MP_BASE_COLOR)
        unreal.EditorAssetLibrary.save_asset(path)
        log("OK", "fx material %s" % name)

    # pale dust grey, warm flash white — C++ fades them by shrinking
    make_unlit("M_FX_Dust", (0.45, 0.42, 0.37, 1.0))
    make_unlit("M_FX_Flash", (1.0, 0.82, 0.50, 1.0))

    # --- glow accents: HDR unlit colors (>1 blooms through the post-process)
    # applied to small accent props so the night world has light sources ---
    make_unlit("M_Glow_Rune", (1.2, 0.35, 2.2, 1.0))      # rune-spike violet
    make_unlit("M_Glow_Cap", (0.35, 1.8, 1.0, 1.0))       # glowcap teal-green
    make_unlit("M_Glow_Crystal", (0.5, 1.6, 2.0, 1.0))    # dark crystal cyan
    make_unlit("M_Glow_Eye", (1.6, 2.2, 0.5, 1.0))        # beast eyes
    make_unlit("M_Glow_Moon", (2.5, 2.6, 3.0, 1.0))       # moon disc
    make_unlit("M_Glow_Star", (3.0, 3.0, 3.2, 1.0))        # starfield points
    # scorch mark: near-black unlit disc left where buildings fall
    make_unlit("M_Decal_Scorch", (0.02, 0.015, 0.025, 1.0))
    # player vessel: shadowy placeholder body until the roster FBX drops
    make_unlit("M_PawnVessel", (0.09, 0.06, 0.16, 1.0))
    log("OK", "glow + scorch materials ready")


# Models whose whole mesh reads as a light source at night: override the
# imported glTF material with the glow material on the instanced mesh.
GLOW_MODELS = {
    "glowcap-ring": "M_Glow_Cap",
    "rune-spike": "M_Glow_Rune",
    "dark-crystal-cluster": "M_Glow_Crystal",
}


# --------------------------------------------------------------------------
# prop scattering (one instanced-mesh actor per model)
# --------------------------------------------------------------------------

def scatter_props():
    import unreal
    ell = unreal.EditorLevelLibrary
    eal = unreal.EditorAssetLibrary

    by_model = {}
    for p in import_assets.places:
        by_model.setdefault(p["model"], []).append(p)

    # Destructible city structures become ACultDestructible actors (they
    # need per-building HP), not instanced statics. If the C++ module is
    # not compiled yet the class won't exist — then we leave them instanced
    # so the world still looks complete; rerun after compiling.
    cult_class = None
    try:
        cult_class = unreal.load_class(None, "/Script/CultUlhu.CultDestructible")
    except Exception:
        cult_class = None
    if cult_class is not None:
        dcount = 0
        for model in sorted(list(by_model.keys())):
            name = os.path.splitext(os.path.basename(model))[0]
            if name not in DESTRUCTIBLE_MODELS:
                continue
            items = by_model.pop(model)
            mesh = eal.load_asset("/Game/CultUlhu/World/SM_" + name)
            if not mesh:
                mesh = eal.load_asset(DEST_WORLD + "/" + name)
            if not mesh:
                log("SKIP", "no static mesh for destructible %s" % model)
                continue
            for i, p in enumerate(items):
                x, y, z = to_ue(p["x"], p["y"], p["z"])
                a = ell.spawn_actor_from_class(
                    cult_class, unreal.Vector(x, y, z),
                    unreal.Rotator(0, -p["rot"], 0))
                if not a:
                    log("SKIP", "spawn failed for destructible %s" % name)
                    break
                a.set_actor_label("WILD_Destructible_%s_%d" % (name, i))
                a.set_actor_scale3d(
                    unreal.Vector(p["scale"], p["scale"], p["scale"]))
                try:
                    a.set_mesh(mesh)
                except Exception as e:  # noqa: BLE001
                    log("ERROR", "set_mesh failed on %s: %s" % (name, e))
                dcount += 1
        log("OK", "destructible buildings: %d" % dcount)
    elif any(os.path.splitext(os.path.basename(m))[0] in DESTRUCTIBLE_MODELS
             for m in by_model):
        log("SKIP", "CultDestructible class not found — compile the C++ "
                    "module first, then rerun this script")

    use_instancing = True
    for model, items in sorted(by_model.items()):
        name = os.path.splitext(os.path.basename(model))[0]
        mesh = eal.load_asset("/Game/CultUlhu/World/SM_" + name)
        if not mesh:
            # maybe the import named it after the file directly
            mesh = eal.load_asset(DEST_WORLD + "/" + name)
        if not mesh:
            log("SKIP", "no static mesh for %s" % model)
            continue
        label = "WILD_" + name
        placed = 0
        if use_instancing:
            try:
                anchor = ell.spawn_actor_from_class(
                    unreal.Actor, unreal.Vector(0, 0, 0),
                    unreal.Rotator(0, 0, 0))
                anchor.set_actor_label(label)
                ism = unreal.InstancedStaticMeshComponent()
                anchor.add_instance_component(ism)
                ism.set_static_mesh(mesh)
                # glow accents: whole-mesh emissive override (see GLOW_MODELS)
                glow_name = GLOW_MODELS.get(name)
                if glow_name:
                    glow_mat = eal.load_asset(DEST_MAT + "/" + glow_name)
                    if glow_mat:
                        try:
                            ism.set_material(0, glow_mat)
                        except Exception:
                            pass
                for p in items:
                    x, y, z = to_ue(p["x"], p["y"], p["z"])
                    t = unreal.Transform(
                        unreal.Vector(x, y, z),
                        unreal.Rotator(0, -p["rot"], 0),
                        unreal.Vector(p["scale"], p["scale"], p["scale"]))
                    ism.add_instance(t)
                    placed += 1
            except Exception as e:  # noqa: BLE001 — fall back below
                log("SKIP", "instancing failed (%s), using single actors" % e)
                use_instancing = False
                try:
                    anchor.destroy_actor()
                except Exception:
                    pass
        if not use_instancing:
            glow_name = GLOW_MODELS.get(name)
            glow_mat = (eal.load_asset(DEST_MAT + "/" + glow_name)
                        if glow_name else None)
            for p in items:
                x, y, z = to_ue(p["x"], p["y"], p["z"])
                a = ell.spawn_actor_from_object(
                    mesh, unreal.Vector(x, y, z),
                    unreal.Rotator(0, -p["rot"], 0))
                a.set_actor_scale3d(
                    unreal.Vector(p["scale"], p["scale"], p["scale"]))
                a.set_actor_label(label)
                if glow_mat:
                    try:
                        a.static_mesh_component.set_material(0, glow_mat)
                    except Exception:
                        pass
                placed += 1
        log("OK", "%s: %d instances" % (name, placed))

    # feral-beast spawns: live ACultFeralBeast actors when the C++ module is
    # compiled, static display meshes otherwise (rerun after compiling).
    beast_class = None
    try:
        beast_class = unreal.load_class(None, "/Script/CultUlhu.CultFeralBeast")
    except Exception:
        beast_class = None
    if beast_class is None:
        log("SKIP", "CultFeralBeast class not found — compile the C++ module, "
                    "then rerun; using static displays")
    by_species = {}
    for s in import_assets.spawns:
        by_species.setdefault(s["species"], []).append(s)
    for species, items in sorted(by_species.items()):
        mesh = eal.load_asset(DEST_CREATURES + "/SK_" + species)
        if not mesh:
            mesh = eal.load_asset(DEST_CREATURES + "/" + species)
        if not mesh:
            log("SKIP", "no creature mesh for %s" % species)
            continue
        if beast_class is not None:
            n = 0
            for s in items:
                for k in range(s["count"]):
                    # deterministic small spread so packs don't stack
                    ox = ((k * 173) % 7 - 3) * 120.0
                    oy = ((k * 311) % 5 - 2) * 120.0
                    x, y, z = to_ue(s["x"], s["y"], s["z"])
                    a = ell.spawn_actor_from_class(
                        beast_class, unreal.Vector(x + ox, y + oy, z),
                        unreal.Rotator(0, 0, 0))
                    if not a:
                        continue
                    a.set_actor_label("WILD_Beast_%s_%d" % (species, n))
                    try:
                        a.set_mesh(mesh)
                        a.set_species(species)
                    except Exception as e:  # noqa: BLE001
                        log("ERROR", "beast setup failed (%s): %s" % (species, e))
                    n += 1
            log("OK", "%s: %d live beasts" % (species, n))
            continue
        anchor = ell.spawn_actor_from_class(
            unreal.Actor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        anchor.set_actor_label("WILD_SPAWN_" + species)
        try:
            ism = unreal.InstancedStaticMeshComponent()
            anchor.add_instance_component(ism)
            ism.set_static_mesh(mesh)
            for s in items:
                for _ in range(s["count"]):
                    x, y, z = to_ue(s["x"], s["y"], s["z"])
                    ism.add_instance(unreal.Transform(
                        unreal.Vector(x, y, z), unreal.Rotator(0, 0, 0),
                        unreal.Vector(1, 1, 1)))
            log("OK", "%s: %d spawned (static display)" %
                (species, sum(s["count"] for s in items)))
        except Exception as e:  # noqa: BLE001
            log("ERROR", "spawn instancing failed for %s: %s" % (species, e))


# --------------------------------------------------------------------------
# night lighting rig (this is what makes the ground reflect)
# --------------------------------------------------------------------------

def build_lighting():
    import unreal
    ell = unreal.EditorLevelLibrary

    def spawn(cls, label, loc=(0, 0, 0), rot=(0, 0, 0)):
        a = ell.spawn_actor_from_class(
            cls, unreal.Vector(*loc), unreal.Rotator(*rot))
        a.set_actor_label(label)
        return a

    # cold moonlight — calibrated for a readable night (was 0.55: too dim)
    sun = spawn(unreal.DirectionalLight, "WILD_MoonLight",
                rot=(-52, -30, 0))
    try:
        sun.light_component.set_intensity(2.4)
        sun.light_component.set_light_color(
            unreal.LinearColor(0.55, 0.68, 1.0, 1.0))
    except Exception:
        pass

    # blue skylight so shadowed ground is never pitch black (was 0.35)
    sky = spawn(unreal.SkyLight, "WILD_SkyLight")
    try:
        sky.light_component.set_intensity(0.9)
        sky.light_component.set_light_color(
            unreal.LinearColor(0.25, 0.30, 0.55, 1.0))
    except Exception:
        pass

    spawn(unreal.SkyAtmosphere, "WILD_SkyAtmosphere")

    # post-process: keep auto-exposure from hunting at night, gentle vignette
    try:
        ppv = spawn(unreal.PostProcessVolume, "WILD_PostProcess")
        ppv.set_actor_scale3d(unreal.Vector(60, 60, 30))
        ppv.set_editor_property("b_unbound", True)
        s = ppv.get_editor_property("settings")
        s.set_editor_property("b_override_auto_exposure_min_brightness", True)
        s.set_editor_property("auto_exposure_min_brightness", 0.5)
        s.set_editor_property("b_override_auto_exposure_max_brightness", True)
        s.set_editor_property("auto_exposure_max_brightness", 1.75)
        s.set_editor_property("b_override_vignette_intensity", True)
        s.set_editor_property("vignette_intensity", 0.35)
        # bloom: only HDR emissives (>1.0 luminance) glow — the M_Glow_*
        # materials above; everything else is unaffected
        s.set_editor_property("b_override_bloom_intensity", True)
        s.set_editor_property("bloom_intensity", 0.45)
        s.set_editor_property("b_override_bloom_threshold", True)
        s.set_editor_property("bloom_threshold", 1.0)
        ppv.set_editor_property("settings", s)
    except Exception:
        pass

    # violet-black height fog ("beautiful but very dark")
    fog = spawn(unreal.ExponentialHeightFog, "WILD_HeightFog")
    try:
        fog.component.set_editor_property("fog_density", 0.012)
        fog.component.set_editor_property(
            "fog_inscattering_color",
            unreal.LinearColor(0.039, 0.024, 0.094, 1.0))  # #0a0618
    except Exception:
        pass

    # reflection captures — WITHOUT these nothing reflects
    for label, cx, cy in [("Highlands", -375, 0), ("Cavern", -300, -400),
                          ("RazedCity", 250, 0), ("Swamp", 150, -425)]:
        cap = spawn(unreal.SphereReflectionCapture,
                    "WILD_Reflection_%s" % label,
                    loc=(cx * CM, cy * CM, 8 * CM))
        try:
            cap.capture_component.set_editor_property(
                "influence_radius", 450 * CM)
        except Exception:
            pass

    # moon disc: HDR unlit sphere parked far away in the moonlight's
    # direction, so the night sky has a visible moon that blooms
    try:
        eal = unreal.EditorAssetLibrary
        moon_mat = eal.load_asset(DEST_MAT + "/M_Glow_Moon")
        sphere = eal.load_asset("/Engine/BasicShapes/Sphere")
        if moon_mat and sphere:
            moon = ell.spawn_actor_from_object(
                sphere, unreal.Vector(-52000, -30000, 38000),
                unreal.Rotator(0, 0, 0))
            moon.set_actor_label("WILD_Moon")
            moon.set_actor_scale3d(unreal.Vector(28, 28, 28))
            moon.static_mesh_component.set_material(0, moon_mat)
    except Exception:
        pass

    # stars: one instanced-mesh actor, 260 HDR points on a dome —
    # a single draw call, visible everywhere except under the cavern bowl
    try:
        eal = unreal.EditorAssetLibrary
        star_mat = eal.load_asset(DEST_MAT + "/M_Glow_Star")
        sphere = eal.load_asset("/Engine/BasicShapes/Sphere")
        if star_mat and sphere:
            anchor = spawn(unreal.Actor, "WILD_Stars")
            ism = unreal.InstancedStaticMeshComponent()
            anchor.add_instance_component(ism)
            ism.set_static_mesh(sphere)
            ism.set_material(0, star_mat)
            import math as _math
            import random as _random
            rng = _random.Random(137)
            R = 60000.0
            for _ in range(260):
                az = rng.uniform(0, 2 * _math.pi)
                el = rng.uniform(0.12, 1.35)
                x = R * _math.cos(el) * _math.cos(az)
                y = R * _math.cos(el) * _math.sin(az)
                z = R * _math.sin(el)
                sc = rng.uniform(2.2, 5.0)
                ism.add_instance(unreal.Transform(
                    unreal.Vector(x, y, z), unreal.Rotator(0, 0, 0),
                    unreal.Vector(sc, sc, sc)))
            log("OK", "starfield: 260 instances")
    except Exception as e:  # noqa: BLE001
        log("SKIP", "starfield failed: %s" % e)


# --------------------------------------------------------------------------
# player starts (so there's somewhere to spawn and walk from)
# --------------------------------------------------------------------------

def place_player_starts():
    import unreal
    ell = unreal.EditorLevelLibrary
    # (label, x_m, y_m, z_m, yaw_deg) — y_m = ground height + 2m hover,
    # measured from the height field (generate_dark_wilds.height)
    starts = [
        ("WILD_PlayerStart_Plateau", 150, 6.8, 60, -90),      # razed-city forum
        ("WILD_PlayerStart_CavernRim", -300, 15.6, -180, 180),  # cavern overlook
        ("WILD_PlayerStart_SwampEdge", 150, 2.7, -280, 0),    # swamp edge
    ]
    for label, xm, ym, zm, yaw in starts:
        x, y, z = to_ue(xm, ym, zm)
        a = ell.spawn_actor_from_class(
            unreal.PlayerStart, unreal.Vector(x, y, z),
            unreal.Rotator(0, yaw, 0))
        a.set_actor_label(label)
    log("OK", "%d player starts placed" % len(starts))


def ensure_wilds_level():
    # The playable world lives in /Game/Maps/L_DarkWilds (created by
    # BuildMainMenu.py). Make sure it is the open level before building.
    import unreal
    ell = unreal.EditorLevelLibrary
    eal = unreal.EditorAssetLibrary
    try:
        cur = ell.get_editor_world().get_full_name()
    except Exception:
        cur = ""
    if "L_DarkWilds" in cur:
        return
    path = "/Game/Maps/L_DarkWilds"
    if eal.does_asset_exist(path):
        ell.load_level(path)
    else:
        ell.new_level(path)
    log("OK", "building in %s" % path)


def main():
    log("OK", "repo: %s" % REPO)
    if not os.path.exists(MAP_PATH):
        raise RuntimeError("map not found: %s (git pull?)" % MAP_PATH)
    ensure_wilds_level()
    section("clear previous run", cleanup_previous)
    section("import assets", import_assets)
    section("build materials", build_materials)
    section("build FX materials", build_fx_materials)
    section("place terrain + water", place_terrain)
    section("scatter props", scatter_props)
    section("build lighting rig", build_lighting)
    section("place player starts", place_player_starts)
    try:
        import unreal
        unreal.EditorLevelLibrary.save_current_level()
        log("OK", "level saved")
    except Exception as e:
        log("ERROR", "save failed: %s" % e)
    log("OK", "done — PIE to walk the wilds. Rerun after every git pull.")


def build_wilds():
    """Entry point for BuildAll.py (import-safe)."""
    main()


if __name__ == "__main__":
    main()
