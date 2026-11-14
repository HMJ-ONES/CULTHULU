"""Record verified source-page acquisition blockers, without substituting other models."""
from pathlib import Path
import json
project=Path(__file__).resolve().parents[1]
path=project/'Config/ApprovedCharacterModels.json'
document=json.loads(path.read_text(encoding='utf-8-sig'))
entries=[
 ('bokrug','https://sketchfab.com/3d-models/lizard-fe08fb17e7104a339433c689b1e91af5','Lizard','3DRT.com','BLOCKED_ACQUISITION','Source page offers no download; obtain creator-authorized file and game redistribution license'),
 ('hastur','https://sketchfab.com/3d-models/the-king-in-yellow-hastur-0d568df99cc64396877641b303e2c882','The King In Yellow (Hastur)','M.O.T.H.','BLOCKED_ACQUISITION','Source page offers no download; obtain creator-authorized file and game redistribution license'),
 ('nug_and_yeb','https://sketchfab.com/3d-models/floating-horror-54a063d8810540feb1906b27316a6d53','Floating Horror','Stilesstoddard','BLOCKED_ACQUISITION','Source page offers no download; obtain creator-authorized file and game redistribution license, then create two differentiated floating instances'),
 ('rhan_tegoth','https://sketchfab.com/3d-models/maw-of-the-void-free-creature-asset-402848d2da654330bc9c3f23ec06cf8d','Maw of the Void - Free Creature Asset','Tim0','BLOCKED_RIGHTS','CC-BY uploader identifies this as fan art based on Darksiders Genesis concept art; underlying rights need clearance'),
 ('shub_niggurath','https://sketchfab.com/3d-models/dark-young-overlord-rigged-animated-3c4975d1bf604c0f9fe91623634a4f5c','Dark Young - Overlord (Rigged & Animated)','Hank657','BLOCKED_RIGHTS','CC-BY uploader identifies the creature as the Overlord anime Dark Young from season 3; underlying rights need clearance'),
]
for entity,url,title,author,status,reason in entries:
 if entity not in document['characters']:
  document['characters'][entity]=dict(approved=True,source_url=url,title=title,author=author,status=status,license='CC-BY-4.0' if status=='BLOCKED_RIGHTS' else 'NOT_VERIFIED',license_verified=status=='BLOCKED_RIGHTS',rights_cleared=False,verified_date='2026-10-22',source_file=f'Saved/CharacterSources/{entity}/source.glb',remaining_art=[reason])
path.write_text(json.dumps(document,indent=2)+'\n',encoding='utf-8')
