"""Prepare an explicit binary-safe backup list; does not commit or push anything."""
from pathlib import Path
import json
project=Path(__file__).resolve().parents[1]; root=project.parent
files=[
 'Config/ApprovedCharacterModels.json','Config/DefaultGame.ini',
 'Content/CharacterModels/registry.json','Content/CharacterModels/CREDITS.md','README.md',
 'Docs/CharacterPipeline.md','Docs/CharacterModels_NextSteps.md','Docs/ApprovedCharacterModels.md',
 'Docs/ApprovedCharacterModelResults.json','Docs/ApprovedModelBuild.json',
 'Source/CultUlhu/CultUlhu.Build.cs','Source/CultUlhu/Private/CultCharacterModel.cpp',
 'Source/CultUlhu/Public/CultCharacterModel.h','Source/CultUlhu/Private/CultCharacterModelTest.cpp',
 'Source/CultUlhu/Private/CultCharacterSelectionLayoutTest.cpp',
 'Source/CultUlhu/Private/CultCorpse.cpp','Source/CultUlhu/Private/CultUlhuPlayerPawn.cpp',
 'Source/CultUlhu/Private/UI/CultCharacterSelectWidget.cpp','Source/CultUlhu/Public/UI/CultCharacterSelectWidget.h',
 'Source/CultUlhu/Private/UI/CultCharacterCardWidget.cpp','Source/CultUlhu/Private/UI/CultCharacterPreviewWidget.cpp',
 'Source/CultUlhu/Public/UI/CultCharacterPreviewWidget.h',
]
files += ['Tools/'+name for name in [
 'ImportSelectedCharacterAssets.py','ImportSelectedCharacterAssets.ps1','StageApprovedCharacterDownloads.py',
 'RecordCharacterAcquisitionGaps.py','OptimizeImportedCharacterMeshes.py','BuildSghllorAppearance.py',
 'ImportCharacterPortraits.py','WriteCharacterModelCredits.py','ApplyCharacterCalibration.py',
 'GenerateCharacterCookPaths.py','RecordApprovedCharacterIntegration.py','PrepareOriginalMiGoFBX.py',
 'PrepareOriginalYigFBX.py','RestoreApprovedRigMaterials.py','BuildApprovedModelBackupManifest.py',
]]
registry=json.loads((project/'Content/CharacterModels/registry.json').read_text())
folders=set()
for binding in registry['characters'].values():
 for mesh in binding.get('meshes',[]):
  if '/Imports/' in mesh:
   before,after=mesh.split('/Imports/',1); folders.add(before+'/Imports/'+after.split('/')[0])
 for key in ['core_material','mist_material','portrait']:
  if binding.get(key): folders.add(binding[key].rsplit('/',1)[0])
for folder in folders:
 directory=project/'Content'/folder.removeprefix('/Game/')
 files += [str(p.relative_to(project)).replace('\\','/') for p in directory.rglob('*') if p.is_file() and p.suffix in ['.uasset','.uexp','.ubulk']]
files += [str(p.relative_to(project)).replace('\\','/') for p in (project/'Docs/ApprovedCharacterScreenshots').glob('*.png')]
paths=sorted({str((project/f).relative_to(root)).replace('\\','/') for f in files})
assert paths and all((root/p).is_file() for p in paths),'Incomplete delivery; generate results first'
assert not any(set(Path(p).parts)&{'Saved','Intermediate','Binaries','DerivedDataCache'} for p in paths)
target=project/'Saved/Reports/ApprovedModelBackupManifest.json'; target.parent.mkdir(exist_ok=True,parents=True)
target.write_text(json.dumps(paths,indent=2)+'\n',encoding='utf-8')
print(json.dumps(dict(manifest=str(target),files=len(paths),bytes=sum((root/p).stat().st_size for p in paths))))
