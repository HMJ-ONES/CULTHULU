"""Adapt the licensed Frost Predator motion for the Bokrug gameplay pawn."""
import hashlib
import json
from pathlib import Path

import unreal

PROJECT = Path(__file__).resolve().parents[1]
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve() == PROJECT
registry_path = PROJECT / "Content/CharacterModels/registry.json"
config_path = PROJECT / "Config/ApprovedCharacterModels.json"
registry = json.loads(registry_path.read_text(encoding="utf-8-sig"))
config = json.loads(config_path.read_text(encoding="utf-8-sig"))
binding = registry["characters"]["bokrug"]
selection = config["characters"]["bokrug"]
assert binding["source_sha256"] == selection["prepared_source_sha256"]
assert selection["rights_cleared"] and binding["source_url"] == selection["source_url"]

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
library = unreal.AnimationLibrary
clips = {
    clip["name"].split("sourcerig_", 1)[-1].lower(): unreal.load_asset(clip["asset"])
    for clip in binding["clips"]
}
skeleton = unreal.load_asset(binding["skeleton"])
bones = library.get_animation_track_names(clips["idle"])
assert bones and all(clip.get_editor_property("skeleton") == skeleton for clip in clips.values())
materials = {
    slot.get_editor_property("material_interface")
    for path in binding["meshes"]
    for slot in unreal.load_asset(path).get_editor_property("materials")
}
for material in materials:
    assert isinstance(material, unreal.MaterialInstanceConstant)
    assert assets.get_metadata_tag(material, "CultCharacterOwner") == "CultUlhu.ImportSelectedCharacterAssets"
    unreal.MaterialEditingLibrary.set_material_usage_override(material, unreal.MaterialUsage.MATUSAGE_MORPH_TARGETS, True, True)
    assert assets.save_loaded_asset(material)
owner = "CultUlhu.BokrugGameplayMotion"
recipe = hashlib.sha256(Path(__file__).read_bytes() + binding["source_sha256"].encode()).hexdigest()
folder = "/Game/CultUlhu/Characters/bokrug/GameplayMotion/v" + recipe[:8]
fps = 30
options = unreal.AnimPoseEvaluationOptions()


def pose(clip, time):
    evaluated = unreal.AnimPoseExtensions.get_anim_pose_at_time(clips[clip], time, options)
    return [unreal.AnimPoseExtensions.get_bone_pose(evaluated, bone, unreal.AnimPoseSpaces.LOCAL)
            for bone in bones]


def interpolate(left, right, alpha):
    a, b = left.rotation, right.rotation
    start = (a.x, a.y, a.z, a.w)
    end = (b.x, b.y, b.z, b.w)
    sign = -1 if sum(x * y for x, y in zip(start, end)) < 0 else 1
    values = [x * (1 - alpha) + sign * y * alpha for x, y in zip(start, end)]
    size = sum(x * x for x in values) ** .5
    rotation = unreal.Quat(*(x / size for x in values))
    def lerp(a, b):
        return unreal.Vector(a.x + (b.x - a.x) * alpha, a.y + (b.y - a.y) * alpha,
                             a.z + (b.z - a.z) * alpha)
    result = unreal.Transform()
    result.translation = lerp(left.translation, right.translation)
    result.rotation = rotation
    result.scale3d = lerp(left.scale3d, right.scale3d)
    return result


def create(role, poses):
    name = "A_Bokrug_" + role + "_v" + recipe[:8]
    path = folder + "/" + name
    if assets.does_asset_exist(path):
        animation = assets.load_asset(path)
        assert assets.get_metadata_tag(animation, "CultOwner") == owner
        return animation.get_path_name()
    factory = unreal.AnimSequenceFactory()
    factory.set_editor_property("target_skeleton", skeleton)
    animation = tools.create_asset(name, folder, unreal.AnimSequence, factory)
    assert animation and all(len(sample) == len(bones) for sample in poses)
    controller = animation.get_editor_property("controller")
    controller.open_bracket("Frost Predator gameplay adaptation", False)
    controller.set_frame_rate(unreal.FrameRate(fps, 1), False)
    controller.set_number_of_frames(unreal.FrameNumber(len(poses) - 1), False)
    for name, track in zip(bones, zip(*poses)):
        controller.add_bone_curve(name, False)
        assert controller.set_bone_track_keys(name, [key.translation for key in track],
                                              [key.rotation for key in track],
                                              [key.scale3d for key in track], False)
    controller.close_bracket(False)
    assets.set_metadata_tag(animation, "CultOwner", owner)
    assert assets.save_loaded_asset(animation)
    return animation.get_path_name()


roles = {role: clips[role].get_path_name() for role in ("idle", "walk")}
roles["preview"] = roles["idle"]
for role, source, end in [("run", "run", .96), ("attack", "attack", 1.04),
                          ("cast", "powerfull_attack", 2.4), ("taunt", "roar", 2.8)]:
    intervals = round(end * fps)
    timeline = [pose(source, index * end / intervals) for index in range(intervals + 1)]
    if role == "run":
        # Crossfade the last strides into the first pose for a continuous sprint.
        for offset in range(5):
            index = intervals - 4 + offset
            blend = (offset + 1) / 5
            timeline[index] = [interpolate(a, b, blend) for a, b in zip(timeline[index], timeline[0])]
    roles[role] = create(role, timeline)

# Transition through poses on the original quadruped skeleton. The pawn still
# handles movement, contact timing, and death. The model only presents the poses.
for role, keys in {
    "hit": [(0, "idle", 0), (.09, "attack", .20), (.27, "idle", 0)],
    "dash": [(0, "run", .08), (.14, "run", .35), (.27, "run", .62)],
    "jump": [(0, "run", .08), (.22, "run", .34), (.50, "run", .46)],
    "land": [(0, "run", .46), (.10, "run", .62), (.24, "idle", 0)],
    "death": [(0, "idle", 0), (.12, "attack", .20), (.48, "sleeping", 0), (.66, "sleeping", .5)],
}.items():
    key_poses = [(round(time * fps), pose(clip, frame)) for time, clip, frame in keys]
    timeline = [key_poses[0][1]]
    for (start, left), (end, right) in zip(key_poses, key_poses[1:]):
        for index in range(start + 1, end + 1):
            t = (index - start) / (end - start)
            smooth = t * t * (3 - 2 * t)
            timeline.append([interpolate(a, b, smooth) for a, b in zip(left, right)])
    roles[role] = create(role, timeline)

binding["animation_roles"] = roles
binding["animation_sets"] = [{"skeleton": binding["skeleton"], "roles": roles}]
binding["upstream_attribution"] = selection["upstream_attribution"]
selection["reviewed_gameplay_binding"] = binding
for path, data in ((registry_path, registry), (config_path, config)):
    path.write_text(json.dumps(data, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
unreal.log("BOKRUG GAMEPLAY MOTION INSTALLED: " + ", ".join(sorted(roles)))
unreal.SystemLibrary.quit_editor()
