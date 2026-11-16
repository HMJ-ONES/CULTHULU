"""Retain original LOD buffers needed for one shared runtime humanoid merge.

Only the CPU-access flag changes. The 18 original assets were byte-compared
against GitHub checkpoint df222ec before this migration; no art is replaced.
"""
from pathlib import Path
import unreal

project = Path(__file__).resolve().parents[1]
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve() == project
saved = 0
for skin in ('civilian_villager', 'cultist_hooded', 'cultist_magus'):
    prefix = 'Mage_' if skin == 'cultist_magus' else 'Rogue_'
    for piece in ('Body', 'Head_Hooded' if skin == 'cultist_hooded' else 'Head', 'ArmLeft', 'ArmRight', 'LegLeft', 'LegRight'):
        path = f'/Game/CultUlhu/People/{skin}/{skin}/SkeletalMeshes/{prefix}{piece}'
        mesh = unreal.load_asset(path)
        assert mesh, path
        unreal.SkeletalMeshEditorSubsystem.set_allow_cpu_access(mesh, True)
        assert unreal.EditorAssetLibrary.save_loaded_asset(mesh, False), path
        saved += 1
unreal.log(f'[CultHumanMesh] CPU source LOD data retained on {saved} original meshes')
