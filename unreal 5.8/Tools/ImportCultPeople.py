"""Import the original CC0 NPC packages already shipped in the repository."""
import base64
import json
from pathlib import Path
import struct
import unreal

project = Path(unreal.Paths.project_dir()).resolve()
repo = project.parent
staging = project / "Saved" / "SocietyImport"
staging.mkdir(parents=True, exist_ok=True)
manifest = {}
for role in ("cultist_hooded", "cultist_magus", "civilian_villager"):
    raw = base64.b64decode((repo / "assets" / "characters" / role / "model.glb.b64").read_text())
    size = struct.unpack_from("<I", raw, 12)[0]
    data = json.loads(raw[20:20 + size])
    for node in data["nodes"]:
        if node.get("name") in ("Knife_Offhand", "1H_Crossbow", "2H_Crossbow", "Knife", "Throwable", "Spellbook", "Spellbook_open", "1H_Wand"):
            node.pop("mesh", None)
    encoded = json.dumps(data, separators=(",", ":")).encode()
    encoded += b" " * (-len(encoded) % 4)
    rest = raw[20 + size:]
    decoded = staging / (role + ".glb")
    decoded.write_bytes(struct.pack("<III", 0x46546C67, 2, 20 + len(encoded) + len(rest)) + struct.pack("<II", len(encoded), 0x4E4F534A) + encoded + rest)
    destination = "/Game/CultUlhu/People/" + role
    task = unreal.AssetImportTask()
    task.filename = str(decoded)
    task.destination_path = destination
    task.automated = True
    task.replace_existing = False
    task.save = True
    existing = unreal.EditorAssetLibrary.list_assets(destination, recursive=True, include_folder=False) if unreal.EditorAssetLibrary.does_directory_exist(destination) else []
    if not any(isinstance(unreal.load_asset(path), unreal.SkeletalMesh) for path in existing):
        unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    paths = unreal.EditorAssetLibrary.list_assets(destination, recursive=True, include_folder=False)
    manifest[role] = []
    for path in paths:
        asset = unreal.load_asset(path)
        if isinstance(asset, unreal.SkeletalMesh):
            manifest[role].append(path)
    unreal.log("[SocietyImport] " + role + ": " + str(manifest[role]))
    if not manifest[role]:
        raise RuntimeError("No original NPC skeletal mesh imported for " + role)
(staging / "manifest.json").write_text(json.dumps(manifest, indent=2))
unreal.EditorAssetLibrary.save_directory("/Game/CultUlhu/People", only_if_is_dirty=True, recursive=True)
