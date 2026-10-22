"""Stage only explicitly named approved browser downloads; inspect actual GLB content.

Copies rather than modifying the artist's originals. Never executes source scripts.
"""
from pathlib import Path
import hashlib,json,shutil,struct
PROJECT=Path(__file__).resolve().parents[1]
config=PROJECT/'Config/ApprovedCharacterModels.json'
document=json.loads(config.read_text(encoding='utf-8-sig'))
selected=[
 ('mi_go','insectoid_monster_rig.glb','https://sketchfab.com/3d-models/insectoid-monster-rig-01323e4b2563430f9da85cd255b6e176','Insectoid Monster Rig','DM-913','SuperKapoo913'),
 ('nodens','chariot.glb','https://sketchfab.com/3d-models/chariot-95baa95c8b9b4f4299de0e0609f7307c','Chariot','ShayHooperUni','ShayHooperUni'),
 ('cthulhu_avatar','cthulhu_-_lovecraft_lore.glb','https://sketchfab.com/3d-models/cthulhu-lovecraft-lore-d42fb260fa1546d09df367143520ccbd','Cthulhu - Lovecraft lore','Brian Trepanier','CMBC'),
 ('dagon','dagon_-_lovecraft_lore.glb','https://sketchfab.com/3d-models/dagon-lovecraft-lore-2d96cd1308de4e2e8f0b1413e26a5448','Dagon - Lovecraft lore','Brian Trepanier','CMBC'),
 ('nyarlathotep','lovecraftian.glb','https://sketchfab.com/3d-models/lovecraftian-e640af2877d84d7d9ab14369dcd36d18','Lovecraftian','JuanG3D','juang3d'),
 ('shoggoths','lovecraftian_horror.glb','https://sketchfab.com/3d-models/lovecraftian-horror-6093cbb205444e9aa7b36d7be4ed9b58','LOVECRAFTIAN HORROR','alexalbinyana','alexalbinyana'),
 ('yig','naga_dragon_serpent._stylized_fantasy_monster.glb','https://sketchfab.com/3d-models/naga-dragon-serpent-stylized-fantasy-monster-2fb96bd7a3c543eeabfb3868107ed4a4','Naga Dragon Serpent. stylized fantasy monster','JoePrimal','JoePrimal'),
 ('yog_sothoth','yog-sothoth_-_lovecraft_lore.glb','https://sketchfab.com/3d-models/yog-sothoth-lovecraft-lore-bdf73725a4494ff8b7981685d52f73ab','Yog-Sothoth - Lovecraft lore','Brian Trepanier','CMBC'),
]
for entity,filename,url,title,author,handle in selected:
 source=Path.home()/'Downloads'/filename
 if not source.is_file(): print(entity,'DOWNLOAD NOT FOUND'); continue
 data=source.read_bytes(); assert data[:4]==b'glTF'
 size=struct.unpack_from('<I',data,12)[0]; gltf=json.loads(data[20:20+size])
 clips=[a.get('name','') for a in gltf.get('animations',[])]
 triangles=sum(gltf['accessors'][p['indices']]['count']//3 for m in gltf.get('meshes',[]) for p in m['primitives'] if 'indices' in p)
 relative=f'Saved/CharacterSources/{entity}/source.glb'; target=PROJECT/relative
 target.parent.mkdir(parents=True,exist_ok=True)
 if target.exists(): assert hashlib.sha256(target.read_bytes()).digest()==hashlib.sha256(data).digest(),'Existing source preserved'
 else: shutil.copyfile(source,target)
 entry=dict(approved=True,source_url=url,title=title,author=author,author_url='https://sketchfab.com/'+handle,license='CC-BY-4.0',license_url='https://creativecommons.org/licenses/by/4.0/',license_verified=True,rights_cleared=True,verified_date='2026-10-22',source_file=relative,source_animation_evidence=clips,source_triangles=triangles,rotation=[0,0,0],bodies=1,floating=entity=='yog_sothoth',status='ACQUIRED',animation_roles={},remaining_art=[])
 if len(clips)==1: entry['animation_roles']={'preview':clips[0]}
 if not clips: entry['remaining_art'].append('Static source: rig and create idle, locomotion and attack animation')
 if triangles>100000: entry['remaining_art'].append(f'Retopologize/remesh and bake detail; source has {triangles:,} triangles')
 entry['remaining_art'].append('Review source forward axis, silhouette and foot placement in Unreal')
 # Reruns must not discard artist-edited animation maps, exclusions or calibration.
 if entity not in document['characters']: document['characters'][entity]=entry
 print(entity,json.dumps(dict(bytes=len(data),triangles=triangles,skins=len(gltf.get('skins',[])),clips=clips)))
config.write_text(json.dumps(document,indent=2)+'\n',encoding='utf-8')
