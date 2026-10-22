"""Cook only active approved import revisions, not superseded diagnostic imports."""
from pathlib import Path
import json,re
project=Path(__file__).resolve().parents[1]
registry=json.loads((project/'Content/CharacterModels/registry.json').read_text(encoding='utf-8-sig'))
folders={'/Engine/BasicShapes'}
for entity,binding in registry['characters'].items():
 for mesh in binding.get('meshes',[]):
  if '/Imports/' in mesh:
   before,after=mesh.split('/Imports/',1); folders.add(before+'/Imports/'+after.split('/')[0])
 for key in ['core_material','mist_material','portrait']:
  if binding.get(key): folders.add(binding[key].rsplit('/',1)[0])
path=project/'Config/DefaultGame.ini'; text=path.read_text(encoding='utf-8-sig')
text=text.replace('+DirectoriesToAlwaysCook=(Path="/Game/CultUlhu/Characters")\n','')
text=re.sub(r'; BEGIN APPROVED CHARACTER COOK\n.*?; END APPROVED CHARACTER COOK\n?','',text,flags=re.S)
block=['; BEGIN APPROVED CHARACTER COOK','[/Script/UnrealEd.ProjectPackagingSettings]']
block += ['+DirectoriesToAlwaysCook=(Path="'+folder+'")' for folder in sorted(folders)]
block += ['; END APPROVED CHARACTER COOK','']
path.write_text(text.rstrip()+'\n\n'+'\n'.join(block),encoding='utf-8')
print('\n'.join(sorted(folders)))
