"""Preserve existing map starts/menu camera in cooked runtime metadata.

Edits only PlayerStartTag/Actor.Tags on the two current maps, then saves them.
No actors, transforms, geometry, art or lighting are replaced.
"""
import unreal

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
starts = {"WILD_PlayerStart_CavernRim", "WILD_PlayerStart_Plateau", "WILD_PlayerStart_SwampEdge"}
for level in ("/Game/Maps/L_MainMenu", "/Game/Maps/L_DarkWilds"):
    assert levels.load_level(level), f"Could not load {level}"
    found = set()
    for actor in actors.get_all_level_actors():
        label = actor.get_actor_label()
        if label in starts and isinstance(actor, unreal.PlayerStart):
            actor.set_editor_property("player_start_tag", unreal.Name(label))
            found.add(label)
        elif label != "MENU_VistaCamera" or not isinstance(actor, unreal.CameraActor):
            continue
        tags = list(actor.get_editor_property("tags"))
        if unreal.Name(label) not in tags:
            tags.append(unreal.Name(label))
            actor.set_editor_property("tags", tags)
        unreal.log(f"[CultRuntimeAnchor] {level}: {label} -> runtime tag")
    if level.endswith("L_DarkWilds"):
        assert found == starts, f"Missing named mode starts: {starts - found}"
    assert unreal.EditorLoadingAndSavingUtils.save_current_level(), f"Could not save {level}"
unreal.log("[CultRuntimeAnchor] Existing transforms/art retained; runtime tags saved")
