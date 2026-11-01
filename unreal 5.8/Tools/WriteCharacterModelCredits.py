"""Generate distributable model attribution from verified acquisition records."""
from pathlib import Path
import json
project=Path(__file__).resolve().parents[1]
config=json.loads((project/'Config/ApprovedCharacterModels.json').read_text(encoding='utf-8-sig'))
runtime=json.loads((project/'Content/CharacterModels/registry.json').read_text(encoding='utf-8-sig'))
lines=['# CULT-ULHU character model credits','',
 'The following third-party models are used under Creative Commons Attribution 4.0.',
 'License: https://creativecommons.org/licenses/by/4.0/','',
 'Changes: converted to Unreal assets; source parts assembled and normalized to the existing gameplay height; selection portraits rendered in Unreal. Dense sculptures have generated reduced render LODs. Nodens omits the horse. Original Mi-Go action PoseLib is exported as Prowl and baked in place using original foot anchors; Yig uses the original FBX timeline, with existing sword transforms baked to rigid bones. Source PBR maps are restored on the FBX rigs. No endorsement by the original artists is implied.','']
for entity in runtime['characters']:
 entry=config['characters'][entity]
 if entry['license']=='ORIGINAL':
  lines.extend([f"## {entry['title']} ({entity})",'',"Original CULT-ULHU procedural art. No third-party model or texture used.",'']); continue
 assert entry['license_verified'] and entry['rights_cleared']
 lines.extend([f"## {entry['title']} ({entity})",'',f"Author: {entry['author']} â€” {entry.get('author_url','')}",f"Source: {entry['source_url']}",f"License: {entry['license']} â€” {entry['license_url']}",''])
if any(entry.get('gameplay_animation_sets') for entry in config['characters'].values()):
 lines.extend(['## Gameplay motion adaptations — 2026-10-30','',
  'Original CULT-ULHU gameplay motions were authored on the approved Mi-Go (DM-913) and Yig (JoePrimal) rigs. Source meshes, skeletons and supplied clips are unchanged. Generated animation assets under Gameplay20261030_v1 adapt those credited rigs; all original attribution and CC-BY-4.0 terms above continue to apply. Mi-Go Prowl and Yig supplied idle clips remain in use.',''])
path=project/'Content/CharacterModels/CREDITS.md'; path.write_text('\n'.join(lines),encoding='utf-8')
print(path)
