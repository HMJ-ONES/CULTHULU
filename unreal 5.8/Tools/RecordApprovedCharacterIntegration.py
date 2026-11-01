"""Produce a source-backed model status table and machine-readable delivery record."""
from pathlib import Path
import hashlib,json,re,subprocess
project=Path(__file__).resolve().parents[1]; root=project.parent
config=json.loads((project/'Config/ApprovedCharacterModels.json').read_text(encoding='utf-8-sig'))
registry=json.loads((project/'Content/CharacterModels/registry.json').read_text(encoding='utf-8-sig'))
report_path=project/'Saved/Reports/CharacterModelImport.json'
imports=json.loads(report_path.read_text(encoding='utf-8-sig')) if report_path.exists() else {'characters':[]}
log_path=project/'Saved/Logs/ApprovedCharactersFinal.log'
log=log_path.read_text(encoding='utf-8-sig',errors='replace') if log_path.exists() else ''
menu_log=project/'Saved/Logs/ApprovedCharacterMenuFinal.log'
if menu_log.exists(): log+='\n'+menu_log.read_text(encoding='utf-8-sig',errors='replace')
results=[dict(result=a,test=b) for a,b in re.findall(r'Test Completed\. Result=\{(\w+)\}.*?Path=\{([^}]+)\}',log)]
results=list({row['test']:row for row in results}.values())
verified=any(r['test']=='CultUlhu.Art.ApprovedCharacterModels' and r['result']=='Success' for r in results)
rows=[]
ids=re.findall(r'CULT_RELEASE_CHARACTER\("([a-z0-9_]+)"\)',(project/'Source/CultUlhu/Public/CultReleaseRoster.inl').read_text())
for entity in ids:
 entry=config['characters'].get(entity,{}); binding=registry['characters'].get(entity,{})
 status=binding.get('status',entry.get('status','NO_APPROVED_MODEL'))
 source=(project/entry.get('source_file','')).resolve()
 source_exists=source.is_file() and source.is_relative_to(project/'Saved/CharacterSources')
 portrait=binding.get('portrait','')
 row=dict(character=entity,source_url=entry.get('source_url',''),acquired='ORIGINAL' if entry.get('procedural') else 'YES' if source_exists else 'NO',license_verified=entry.get('license_verified',False),rights_cleared=entry.get('rights_cleared',False),kind='PROCEDURAL' if entry.get('procedural') else 'ANIMATED' if binding.get('clips') else 'RIGGED' if binding.get('skeletal') else 'STATIC' if binding else 'UNAVAILABLE',status=status,imported=bool(binding),selection_visible=verified and bool(binding),gameplay_verified=verified and bool(binding),portrait=portrait,remaining_art=entry.get('remaining_art',['No final approved model for this task']),source_sha256=binding.get('source_sha256',''),source_triangles=entry.get('source_triangles'),render_lod=binding.get('render_lod',0),clips=binding.get('clips',[]),animation_sets=binding.get('animation_sets',[]))
 rows.append(row)
folders=set()
for binding in registry['characters'].values():
 for asset in binding.get('meshes',[]):
  if '/Imports/' in asset:
   before,after=asset.split('/Imports/',1); folders.add(before+'/Imports/'+after.split('/')[0])
 for key in ['core_material','mist_material','portrait']:
  if binding.get(key): folders.add(binding[key].rsplit('/',1)[0])
assets=sorted({p for folder in folders for p in (project/'Content'/folder.removeprefix('/Game/')).rglob('*.uasset')})
record=dict(canonical_project='unreal 5.8/CultUlhu.uproject',release_choices=len(ids),future_characters_restored=False,results=results,automation_error_count=log.count('LogAutomationController: Error:'),model_assets=len(assets),model_asset_bytes=sum(p.stat().st_size for p in assets),largest_model_asset_bytes=max((p.stat().st_size for p in assets),default=0),source_files_committed=False,storage_policy='Only runtime art, code, provenance, portraits and reports go into Git. No raw download duplicates, packages, Saved, Intermediate or Binaries.',characters=rows)
(project/'Docs/ApprovedCharacterModelResults.json').write_text(json.dumps(record,indent=2)+'\n',encoding='utf-8')
lines=['# Approved character model integration','',
 'Only exact approved sources are used. Current release: sixteen choices; the four previously deferred characters remain excluded. Unavailable models retain the working placeholder and original gameplay kit.','',
 '[Simple next steps and remeshing list](CharacterModels_NextSteps.md) · [Attribution](../Content/CharacterModels/CREDITS.md) · [Machine-readable results](ApprovedCharacterModelResults.json)','',
 '| Character | Source URL | Acquired | License verified | Type | Imported into Unreal | Visible in selection | Gameplay verified | Remaining issue |',
 '| --- | --- | --- | --- | --- | --- | --- | --- | --- |']
for row in rows:
 source=f"[Selected source]({row['source_url']})" if row['source_url'].startswith('https://') else 'Original concept' if row['source_url'] else 'Not approved'
 license='YES' if row['license_verified'] else 'NO'
 if row['license_verified'] and not row['rights_cleared']: license+='; underlying rights BLOCKED'
 issue='; '.join(row['remaining_art']).replace('|','/')
 lines.append(f"| {row['character']} | {source} | {row['acquired']} | {license} | {row['kind']} | {'YES' if row['imported'] else row['status']} | {'YES' if row['selection_visible'] else 'NOT VERIFIED' if row['imported'] else 'Placeholder'} | {'NATIVE CHECK' if row['gameplay_verified'] else 'NOT VERIFIED' if row['imported'] else 'Placeholder'} | {issue} |")
lines.extend(['','## Validation',''])
for result in results: lines.append(f"- {result['test']}: **{result['result']}**")
lines.extend(['',f"Automation errors: {record['automation_error_count']}. Runtime character art: {len(assets)} assets, {record['model_asset_bytes']:,} bytes; largest asset {record['largest_model_asset_bytes']:,} bytes.",'',
 'The native checks inspect actual imported assets, render source models, enter a real match, and verify height, capsule collision, camera visibility and persistent corpse geometry. Screenshots require visual inspection too; a component existing is not proof of a good-looking model. These checks are not a human fun/balance review or low-spec performance certification.','',
 'Dense static sculptures use generated 10% render LODs while retaining LOD0 and original downloads. This is a practical interim reduction, not artist retopology or a promise of final deformation quality. Source clips are retained; missing idle/run/attack clips are not invented. Yig uses the original FBX Scene timeline for idle/preview, with original sword transforms preserved on rigid bones. Mi-Go uses the original Blender PoseLib action exported as Prowl, with visual grounding independent of gameplay collision.','',
 'Only the source-hash-owned imports are edited. Runtime model collision is disabled; canonical capsules, character heights, original kits, damage, movement and physical skill targeting remain in the existing pawn. The new model actor is hidden in human first person and reinstated in third person. Nodens uses the chariot body, wheels and shafts; the separate horse is omitted from the binding.','',
 'Raw source GLBs and local receipts stay in Downloads/Saved. Git LFS is available but this repository currently has no LFS tracking rules; the reviewed runtime asset sizes fit ordinary Git and the binary-safe push protocol. Avoid repeated sculpture revisions in normal Git: migrate large ongoing art work to LFS or a source-asset archive before routine iteration.'])
(project/'Docs/ApprovedCharacterModels.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
print(json.dumps({'verified':verified,'tests':results,'characters':len(rows),'asset_bytes':record['model_asset_bytes']}))
