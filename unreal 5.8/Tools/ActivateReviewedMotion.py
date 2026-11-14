"""Activate verified staged character bindings; preserve before/after manifests.
Run only after the matching canonical C++ build is compiled. Existing source art
and definitions remain available. The desktop shortcut uses this same project.
"""
from pathlib import Path
import json,hashlib,copy,shutil
PROJECT=Path(__file__).resolve().parents[1]
assert PROJECT.name=='unreal 5.8' and (PROJECT/'CultUlhu.uproject').is_file()
OWNER='CultUlhu.ReviewedMotion.20261106'
registry_path=PROJECT/'Content/CharacterModels/registry.json'
config_path=PROJECT/'Config/ApprovedCharacterModels.json'
ini_path=PROJECT/'Config/DefaultGame.ini'
registry=json.loads(registry_path.read_text(encoding='utf-8-sig'))
config=json.loads(config_path.read_text(encoding='utf-8-sig'))
selection=json.loads((PROJECT/'Config/ReviewedCharacterMotion.json').read_text())['characters']
FOLDERS={k:k+'_'+v['revision'] for k,v in selection.items() if v['kind']!='native_procedural'}
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
backup=PROJECT/'Saved/MotionIntegration/BeforeActivation';backup.mkdir(parents=True,exist_ok=True)
for path in [registry_path,config_path,ini_path]:
    target=backup/path.name
    if not target.exists():shutil.copy2(path,target)
    elif registry.get('reviewed_motion_owner')!=OWNER:assert target.read_bytes()==path.read_bytes(),'Preserve intervening artist/config edits'
directories=set();activated={}
for key,folder in FOLDERS.items():
    report=json.loads((PROJECT/'Saved/MotionIntegration'/folder/'pending_binding.json').read_text())
    assert report['owner']==OWNER
    for path,digest in report['originalAssetsUnchanged'].items():
        file=PROJECT/'Content'/(path.split('.',1)[0].removeprefix('/Game/')+'.uasset')
        assert sha(file)==digest,('Source changed',key,path)
    entry=copy.deepcopy(registry['characters'][key])
    if 'animation_sets' in report:
        sets=report['animation_sets']
    else:
        roles=copy.deepcopy(report['roles']);roles['attack']=roles['melee_1']
        old=next((s['roles'] for s in entry.get('animation_sets',[]) if s['skeleton']==report['skeleton']),{})
        if 'cast' in old:roles['cast']=old['cast'] # preserve existing behavior; no new ability animation
        sets=[{'skeleton':report['skeleton'],'roles':roles}]
        entry['animation_roles']=roles
    for group in sets:
        for role in ['idle','walk','run','melee_1','melee_2','melee_3','jump','dash','land','taunt']:assert role in group['roles'],(key,role)
        for path in group['roles'].values():
            file=PROJECT/'Content'/(path.split('.',1)[0].removeprefix('/Game/')+'.uasset');assert file.is_file(),path
            if '/ReviewedMotion_' in path:directories.add(path.split('.',1)[0].rsplit('/',1)[0])
    entry['animation_sets']=sets
    if 'meshes' in report:
        entry.update(meshes=report['meshes'],skeletal=True,skeleton=report['skeleton'],
                     rotation=report['rotation'],reference_bounds=report['reference_bounds'],
                     import_profile='reviewed_anatomical_'+report['revision'])
        entry['mesh_skeletons']={m:report['skeleton'] for m in report['meshes']}
        for path in report['meshes']:assert (PROJECT/'Content'/(path.split('.',1)[0].removeprefix('/Game/')+'.uasset')).is_file()
    entry['gameplay_motion_owner']=OWNER;entry['melee_step_motion']=key=='cthulhu_avatar'
    entry['melee_contact_phase']={'dagon':.44,'mi_go':.48,'cthulhu_avatar':.52,'nyarlathotep':.48,'yog_sothoth':.5,'shoggoths':.5,'yig':.51}[key]
    registry['characters'][key]=entry
    if key in config['characters']:
        config['characters'][key]['gameplay_animation_sets']=sets
        config['characters'][key]['reviewed_gameplay_binding']=copy.deepcopy(entry)
    activated[key]={'sets':len(sets),'roles':sorted(sets[0]['roles']),'sourceArtPreserved':True}
registry['characters']['sghllor']['procedural_motion_profile']='original_orb_basic_20261107'
registry['reviewed_motion_owner']=OWNER
ini=ini_path.read_text(encoding='utf-8-sig')
start='; BEGIN REVIEWED MOTION COOK';end='; END REVIEWED MOTION COOK'
if start in ini:
    a=ini.index(start);b=ini.index(end,a)+len(end);ini=ini[:a]+ini[b:]
ini=ini.rstrip()+'\n'+start+'\n'+'\n'.join('+DirectoriesToAlwaysCook=(Path="'+p+'")' for p in sorted(directories))+'\n'+end+'\n'
registry_path.write_text(json.dumps(registry,indent=2)+'\n',encoding='utf-8')
config_path.write_text(json.dumps(config,indent=2)+'\n',encoding='utf-8');ini_path.write_text(ini,encoding='utf-8')
(PROJECT/'Saved/MotionIntegration/Activation.json').write_text(json.dumps({'owner':OWNER,'characters':activated,
    'sghllor':'original native core/mist basic motions','nodens':'unchanged; excluded from animation work',
    'registrySha256':sha(registry_path),'desktop':'same canonical project, no duplicate build'},indent=2)+'\n')
print('REVIEWED_MOTION_ACTIVATED',', '.join(activated))
