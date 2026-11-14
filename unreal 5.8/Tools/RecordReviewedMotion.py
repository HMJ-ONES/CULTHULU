"""Record activated motion, archive the two new studies, and list owned assets.

Does not push, change active bindings, delete files, or include build caches.
Run after native verification. Paths resolve from this canonical script.
"""
from pathlib import Path
import base64, hashlib, json, re

PROJECT=Path(__file__).resolve().parents[1]
ROOT=PROJECT.parent
assert PROJECT.name=='unreal 5.8' and (PROJECT/'CultUlhu.uproject').is_file()
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
selection=json.loads((PROJECT/'Config/ReviewedCharacterMotion.json').read_text())['characters']
activation=json.loads((PROJECT/'Saved/MotionIntegration/Activation.json').read_text())
registry=json.loads((PROJECT/'Content/CharacterModels/registry.json').read_text())
assert activation['registrySha256']==sha(PROJECT/'Content/CharacterModels/registry.json')
log=(PROJECT/'Saved/ReviewedMotionNative.log').read_text(encoding='utf-8-sig',errors='replace')
result=re.findall(r'Test Completed\. Result=\{(\w+)\}.*?Path=\{([^}]+)\}',log)
assert ('Success','CultUlhu.Art.ReviewedCharacterMotion') in result,result
archive=ROOT/'assets/character_animation/archive'
archive.mkdir(exist_ok=True)
sources=[]
for key in ['dagon','mi_go']:
    item=selection[key];folder=ROOT/f"assets/character_animation/{key}/{item['revision']}"
    source=folder/(item['title']+'_BasicMotion.blend')
    review=json.loads((folder/'review/verification.json').read_text())
    assert review['sourceSha256']==sha(source)
    target=archive/(item['title']+'_BasicMotion_'+item['revision']+'.blend.b64')
    encoded=base64.encodebytes(source.read_bytes())
    if target.exists():assert target.read_bytes()==encoded,'Preserve previous archive'
    else:target.write_bytes(encoded)
    assert base64.b64decode(target.read_bytes())==source.read_bytes()
    sources.append({'character':key,'revision':item['revision'],'sha256':sha(source),'archive':str(target.relative_to(ROOT)).replace('\\','/')})

source_archive=ROOT/'assets/character_animation/source_archive'
source_manifest=source_archive/'manifest.json'
source_index=json.loads(source_manifest.read_text())
approved=json.loads((PROJECT/'Config/ApprovedCharacterModels.json').read_text())['characters']['yig']
yig_source=PROJECT/approved['source_file']
yig_target=source_archive/'Yig.fbx.b64'
encoded=base64.encodebytes(yig_source.read_bytes())
if yig_target.exists():assert yig_target.read_bytes()==encoded,'Preserve previous source archive'
else:yig_target.write_bytes(encoded)
assert base64.b64decode(yig_target.read_bytes())==yig_source.read_bytes()
source_index['yig']={'decodedFile':'Yig.fbx','sha256':sha(yig_source),'source':approved['source_url'],
                     'author':approved['author'],'license':approved['license'],
                     'provenance':'Approved prepared_v3 FBX; original armor, weapon and tail rigs. See ../CREDITS.md.'}
source_manifest.write_text(json.dumps(source_index,indent=2)+'\n')

directories=set();pending={}
for key,item in selection.items():
    if item['kind']=='native_procedural':continue
    report=json.loads((PROJECT/f"Saved/MotionIntegration/{key}_{item['revision']}/pending_binding.json").read_text())
    pending[key]=report
    binding=registry['characters'][key]
    for group in binding['animation_sets']:
        for asset in group['roles'].values():
            if '/ReviewedMotion_' in asset:directories.add(asset.split('.',1)[0].rsplit('/',1)[0])
assets=sorted({p for d in directories for p in (PROJECT/'Content'/d.removeprefix('/Game/')).rglob('*.uasset')})
assert assets
network=[json.loads((PROJECT/f'Saved/Automation/Room2_{role}.json').read_text(encoding='utf-8-sig')) for role in ['host','guest']]
assert all(row['ok'] for row in network)
record={'canonicalProject':'unreal 5.8/CultUlhu.uproject','selection':selection,
        'activation':activation,'nativePoseTest':result,'sourceArchives':sources,
        'standaloneDotgooHostGuest':network,
        'yigRecipeVerification':json.loads((PROJECT/f"Saved/MotionIntegration/yig_{selection['yig']['revision']}/recipe_verification.json").read_text()),
        'pendingBindings':pending,'runtimeAssets':[{'path':str(p.relative_to(ROOT)).replace('\\','/'),'bytes':p.stat().st_size,'sha256':sha(p)} for p in assets],
        'limitations':['Initial gameplay motion requires continued artistic review.','Shoggoths knuckle sliding and slight authoring subframe penetration remain polish items.','Projected UVs on retopologized sculptures need artist inspection.','No new ability animations; Nodens unchanged.']}
(PROJECT/'Docs/ReviewedMotionIntegration.json').write_text(json.dumps(record,indent=2)+'\n')
(PROJECT/'Saved/MotionIntegration/RuntimeAssetManifest.json').write_text(json.dumps([r['path'] for r in record['runtimeAssets']],indent=2)+'\n')
print(json.dumps({'assets':len(assets),'bytes':sum(p.stat().st_size for p in assets),'archives':sources},indent=2))
