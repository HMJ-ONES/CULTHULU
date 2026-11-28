"""Stage authorized downloads; isolate the reaper from its unrelated cemetery scene."""
from pathlib import Path
import copy, hashlib, json, shutil, struct

PROJECT = Path(__file__).resolve().parents[1]
assert PROJECT.name == 'unreal 5.8' and (PROJECT/'CultUlhu.uproject').is_file()
OUT = PROJECT/'Saved/HorrorRefresh20261127'
OUT.mkdir(parents=True, exist_ok=True)
for rel in ['Config/ApprovedCharacterModels.json','Content/CharacterModels/registry.json','Content/CharacterModels/CREDITS.md','Config/DefaultGame.ini']:
    target=OUT/'Before'/rel; target.parent.mkdir(parents=True,exist_ok=True)
    if not target.exists(): shutil.copy2(PROJECT/rel,target)

records=[]
for key,filename,uid in [
 ('nug','flesh_blob.glb','b3f5bb51f8144a30b6fa6b22f5d44bf7'),
 ('yeb','living_flesh_blob_-_free_organic_horror_asset.glb','6357bbc094d54cf5ba150ef2951b3750'),
 ('hastur','grim_reaper_low_poly.glb','c4106291b7454917a909afa37affed08')]:
    raw=(Path.home()/'Downloads'/filename).read_bytes()
    assert raw[:4]==b'glTF'
    length=struct.unpack_from('<I',raw,12)[0]
    doc=json.loads(raw[20:20+length]); binary=raw[28+length:]
    credit=doc['asset']['extras']; assert credit['license'].startswith('CC-BY-4.0') and credit['source'].endswith(uid)
    target=PROJECT/f'Saved/CharacterSources/HorrorRefresh20261127/{key}'
    target.mkdir(parents=True,exist_ok=True)
    original=target/'original.glb'
    if original.exists(): assert original.read_bytes()==raw
    else: original.write_bytes(raw)
    if key=='hastur':
        # The download is a complete cemetery. Only the creator's GRIM character
        # belongs in the game; tree rig, graves, grass and sword remain untouched
        # in original.glb. Keep original vertex normals and material assignments.
        chosen=[i for i,n in enumerate(doc['nodes']) if n.get('name')=='GRIM']
        assert len(chosen)==1
        parent={c:i for i,n in enumerate(doc['nodes']) for c in n.get('children',[])}
        keep=set()
        def descend(i):
            keep.add(i)
            for c in doc['nodes'][i].get('children',[]): descend(c)
        descend(chosen[0]); root=chosen[0]
        while root in parent: root=parent[root]; keep.add(root)
        oldnodes=doc['nodes']; remap={old:new for new,old in enumerate(sorted(keep))}
        doc['nodes']=[copy.deepcopy(oldnodes[i]) for i in sorted(keep)]
        for node in doc['nodes']:
            node['children']=[remap[c] for c in node.get('children',[]) if c in keep]
            node.pop('skin',None)
        doc['scenes']=[{'nodes':[remap[root]]}]; doc['scene']=0
        used=sorted({n['mesh'] for n in doc['nodes'] if 'mesh' in n})
        mapping={old:new for new,old in enumerate(used)}
        doc['meshes']=[doc['meshes'][i] for i in used]
        for node in doc['nodes']:
            if 'mesh' in node: node['mesh']=mapping[node['mesh']]
        doc.pop('animations',None); doc.pop('skins',None)
        access=sorted({v for m in doc['meshes'] for p in m['primitives'] for v in [*p['attributes'].values(),p['indices']]})
        am={old:new for new,old in enumerate(access)}
        doc['accessors']=[doc['accessors'][i] for i in access]
        for m in doc['meshes']:
            for p in m['primitives']:
                p['attributes']={k:am[v] for k,v in p['attributes'].items()}; p['indices']=am[p['indices']]
        mats=sorted({p['material'] for m in doc['meshes'] for p in m['primitives']}); mm={old:new for new,old in enumerate(mats)}
        doc['materials']=[doc['materials'][i] for i in mats]
        for m in doc['meshes']:
            for p in m['primitives']: p['material']=mm[p['material']]
        assert not any('Texture' in json.dumps(m) for m in doc['materials']), 'Preserve unexpected texture dependencies'
        doc.pop('images',None); doc.pop('textures',None);doc.pop('samplers',None)
        views=sorted({a['bufferView'] for a in doc['accessors']}); vm={old:new for new,old in enumerate(views)}; packed=bytearray(); newviews=[]
        for i in views:
            v=copy.deepcopy(doc['bufferViews'][i]); start=v.get('byteOffset',0)
            packed.extend(b'\0'*(-len(packed)%4)); v['byteOffset']=len(packed)
            packed.extend(binary[start:start+v['byteLength']]);newviews.append(v)
        for a in doc['accessors']: a['bufferView']=vm[a['bufferView']]
        doc['bufferViews']=newviews; binary=bytes(packed);doc['buffers']=[{'byteLength':len(binary)}]
        triangles=sum(doc['accessors'][p['indices']]['count']//3 for m in doc['meshes'] for p in m['primitives'])
        assert triangles<300000,triangles # Preserve source; UE generates a reduced render LOD.
        print('Isolated reaper',triangles,'triangles',len(binary),'bytes')
    header=json.dumps(doc,separators=(',',':')).encode();header+=b' '*(-len(header)%4);binary+=b'\0'*(-len(binary)%4)
    prepared=struct.pack('<4sIIII',b'glTF',2,28+len(header)+len(binary),len(header),0x4e4f534a)+header+struct.pack('<II',len(binary),0x004e4942)+binary
    path=target/'prepared.glb';path.write_bytes(prepared)
    records.append(dict(key=key,source_file=str(path.relative_to(PROJECT)),source_url=credit['source'],title=credit['title'],author=credit['author'].split(' (')[0],license='CC-BY-4.0',license_url='https://creativecommons.org/licenses/by/4.0/',source_sha256=hashlib.sha256(prepared).hexdigest(),original_sha256=hashlib.sha256(raw).hexdigest(),source_bytes=len(prepared)))
(OUT/'sources.json').write_text(json.dumps(records,indent=2)+'\n')
