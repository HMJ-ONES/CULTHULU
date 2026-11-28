"""Original low-poly projectile art and wet materials; no external dependencies."""
from pathlib import Path
import math
import unreal

PROJECT=Path(__file__).resolve().parents[1]
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()==PROJECT
root='/Game/CultUlhu/Projectiles/Visceral20261127';owner='CultUlhu.OriginalVisceral20261127'
assets=unreal.EditorAssetLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools();mel=unreal.MaterialEditingLibrary
folder=PROJECT/'Saved/HorrorRefresh20261127/OriginalProjectiles';folder.mkdir(parents=True,exist_ok=True)
for name,spike in [('SM_AcidGlob',False),('SM_ChitinDart',True)]:
    path=root+'/'+name
    if assets.does_asset_exist(path):
        assert assets.get_metadata_tag(unreal.load_asset(path),'CultOwner')==owner
        continue
    vertices=[];faces=[]
    def bulb(center,scale,rings=10,sides=16,seed=0):
        start=len(vertices)
        for ring in range(rings+1):
            lat=math.pi*ring/rings
            for side in range(sides):
                phi=math.tau*side/sides
                lump=1+.11*math.sin(phi*3+lat*5+seed)+.065*math.cos(phi*5-lat*3)
                unit=(math.cos(lat),math.sin(lat)*math.cos(phi),math.sin(lat)*math.sin(phi))
                vertices.append(tuple(center[k]+scale[k]*unit[k]*lump for k in range(3)))
                if ring<rings:
                    a=start+ring*sides+side;b=start+ring*sides+(side+1)%sides
                    faces.extend([(a,b,a+sides),(b,b+sides,a+sides)])
    if spike:
        bulb((0,0,0),(62,18,18),8,12)
        for side in [-1,1]: bulb((-22,side*17,0),(25,5,7),4,8,side)
    else:
        bulb((0,0,0),(49,42,42))
        for index in range(4):
            angle=index*2.4;bulb((-38-index*8,math.cos(angle)*21,math.sin(angle)*21-7),(11-index,7,9),4,8,index)
    obj=folder/(name+'.obj')
    obj.write_text('\n'.join(['# CULT-ULHU original procedural art']+['v '+' '.join(map(str,v)) for v in vertices]+['vt 0 0']+['f '+' '.join(str(i+1)+'/1' for i in f) for f in faces])+'\n')
    task=unreal.AssetImportTask();task.filename=str(obj);task.destination_path=root;task.destination_name=name
    task.automated=True;task.replace_existing=False;task.save=True;tools.import_asset_tasks([task])
    mesh=unreal.load_asset(path);assert isinstance(mesh,unreal.StaticMesh),path
    assets.set_metadata_tag(mesh,'CultOwner',owner);assets.save_loaded_asset(mesh)
for name,color in [('M_VisceralGlob',(.16,.26,.018)),('M_VisceralOrgan',(.24,.042,.035))]:
    path='/Game/CultUlhu/Materials/'+name
    if assets.does_asset_exist(path):
        assert assets.get_metadata_tag(unreal.load_asset(path),'CultOwner')==owner
        continue
    mat=tools.create_asset(name,'/Game/CultUlhu/Materials',unreal.Material,unreal.MaterialFactoryNew())
    tint=mel.create_material_expression(mat,unreal.MaterialExpressionVectorParameter);tint.set_editor_property('parameter_name','Color');tint.set_editor_property('default_value',unreal.LinearColor(*color,1))
    noise=mel.create_material_expression(mat,unreal.MaterialExpressionNoise);noise.set_editor_property('scale',.085);noise.set_editor_property('levels',2)
    dark=mel.create_material_expression(mat,unreal.MaterialExpressionMultiply);mel.connect_material_expressions(tint,'',dark,'A');mel.connect_material_expressions(noise,'',dark,'B')
    mel.connect_material_property(dark,'',unreal.MaterialProperty.MP_BASE_COLOR)
    rough=mel.create_material_expression(mat,unreal.MaterialExpressionConstant);rough.r=.22;mel.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
    mat.set_editor_property('two_sided',True);mel.recompile_material(mat);assets.set_metadata_tag(mat,'CultOwner',owner);assets.save_loaded_asset(mat)
unreal.log('CULT_VISCERAL_ASSETS_READY')
unreal.SystemLibrary.quit_editor()
