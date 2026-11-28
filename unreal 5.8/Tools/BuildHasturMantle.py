"""Original folded mantle, torn hem, void mask and exposed tendrils for Hastur."""
from pathlib import Path
import math
import unreal
PROJECT=Path(__file__).resolve().parents[1]
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()==PROJECT
DEST='/Game/CultUlhu/Characters/HorrorRefresh20261127/hastur'
OWNER='CultUlhu.OriginalHasturMantle20261127'
assets=unreal.EditorAssetLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools()
out=PROJECT/'Saved/HorrorRefresh20261127/OriginalHastur';out.mkdir(parents=True,exist_ok=True)
for name,mask in [('SM_KingMantle',False),('SM_KingVoidMask',True)]:
    path=DEST+'/'+name
    if assets.does_asset_exist(path):
        mesh=unreal.load_asset(path)
        recorded=assets.get_metadata_tag(mesh,'CultOwner')
        assert recorded==OWNER or (not recorded and [Path(p).resolve() for p in mesh.get_editor_property('asset_import_data').extract_filenames()]==[(out/(name+'.obj')).resolve()]),'Preserve unrelated art'
        mesh.set_material(0,unreal.load_asset(DEST+('/M_KingShroud_1' if mask else '/M_KingShroud_0')))
        assets.set_metadata_tag(mesh,'CultOwner',OWNER);assets.save_loaded_asset(mesh)
        continue
    verts=[];faces=[]
    if mask:
        for ring in range(9):
            lat=math.pi*ring/8
            for side in range(16):
                a=math.tau*side/16
                verts.append((22+3*math.cos(lat),15*math.sin(lat)*math.cos(a),159+19*math.sin(lat)*math.sin(a)))
                if ring<8:
                    i=ring*16+side;j=ring*16+(side+1)%16;faces.extend([(i,j,i+16),(j,j+16,i+16)])
    else:
        for ring in range(13):
            t=ring/12;z=12+132*t
            radius=25+31*math.sin((1-t)*math.pi*.62)
            for side in range(32):
                a=math.tau*side/32
                fold=1+.13*math.sin(a*9+t*.65)+.045*math.sin(a*17-t)
                hem=(1-t)**6*(8*math.sin(a*7)+4*math.cos(a*11))
                verts.append((-9+radius*fold*.78*math.cos(a),radius*fold*math.sin(a),z+hem))
                if ring<12:
                    i=ring*32+side;j=ring*32+(side+1)%32;faces.extend([(i,j,i+32),(j,j+32,i+32)])
        for limb in range(5):
            offset=len(verts);phase=limb*math.tau/5
            for ring in range(9):
                t=ring/8;center=(9+31*t,math.sin(phase)*(13+23*t),22*(1-t)+7*math.sin(t*math.pi+phase))
                radius=5.5*(1-t)+.35
                for side in range(6):
                    a=side*math.tau/6;verts.append((center[0],center[1]+radius*math.cos(a),center[2]+radius*math.sin(a)))
                    if ring<8:
                        i=offset+ring*6+side;j=offset+ring*6+(side+1)%6;faces.extend([(i,j,i+6),(j,j+6,i+6)])
    obj=out/(name+'.obj');obj.write_text('\n'.join(['# Original CULT-ULHU art','s 1']+['v '+' '.join(map(str,v)) for v in verts]+['vt 0 0']+['f '+' '.join(str(i+1)+'/1' for i in f) for f in faces])+'\n')
    task=unreal.AssetImportTask();task.filename=str(obj);task.destination_path=DEST;task.destination_name=name;task.automated=True;task.replace_existing=False;task.save=True
    tools.import_asset_tasks([task]);mesh=unreal.load_asset(path);assert isinstance(mesh,unreal.StaticMesh)
    material=unreal.load_asset(DEST+('/M_KingShroud_1' if mask else '/M_KingShroud_0'));assert material
    mesh.set_material(0,material)
    assets.set_metadata_tag(mesh,'CultOwner',OWNER);assets.save_loaded_asset(mesh)
unreal.log('CULT_HASTUR_MANTLE_READY')
unreal.SystemLibrary.quit_editor()
