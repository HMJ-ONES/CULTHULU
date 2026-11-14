"""Measure the lightweight original Shoggoths mesh without losing its UVs/art.

Read-only source import. Mouths face source -Y; rotate to +Y authoring forward.
The original already has suitable density and does not need another remesh.
"""
from pathlib import Path
import hashlib
import json
import math
import sys
import bpy
import numpy as np
from mathutils import Matrix, Vector

assert bpy.app.background
PROJECT=Path(__file__).resolve().parents[1]
ROOT=PROJECT.parent
source=PROJECT/'Saved/CharacterSources/shoggoths/source.glb'
before=hashlib.sha256(source.read_bytes()).hexdigest()
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.gltf(filepath=str(source),import_pack_images=True)
meshes=[o for o in bpy.context.scene.objects if o.type=='MESH']
rotation=Matrix.Rotation(math.pi,4,'Z')
raw=[np.array([tuple(rotation@(o.matrix_world@v.co)) for v in o.data.vertices]) for o in meshes]
p=np.concatenate(raw);low,high=p.min(axis=0),p.max(axis=0)
origin=np.array([(low[0]+high[0])/2,(low[1]+high[1])/2,low[2]])
scale=2/(high[2]-low[2]);p=(p-origin)*scale
parts=[]
for obj,vertices in zip(meshes,raw):
    normalized=(vertices-origin)*scale
    parts.append({'name':obj.name,'vertices':len(normalized),'bounds':[normalized.min(axis=0).tolist(),normalized.max(axis=0).tolist()],
        'uvLayers':len(obj.data.uv_layers),'materials':len(obj.data.materials)})
    obj.parent=None
    for vertex,xyz in zip(obj.data.vertices,normalized):vertex.co=xyz
    obj.matrix_world=Matrix.Identity(4);obj.color=(.34,.44,.39,1)
slices=[]
for z in np.arange(.1,1.91,.1):
    a=p[np.abs(p[:,2]-z)<.035]
    slices.append({'z':round(float(z),2),'bounds':[a.min(axis=0).tolist(),a.max(axis=0).tolist()]})
guides={}
for side,sign in [('L',1),('R',-1)]:
    guides[side]=[]
    for z in [.12,.40,.70,1.00,1.20]:
        a=p[(np.abs(p[:,2]-z)<.09)&(p[:,0]*sign>.55)]
        if len(a):guides[side].append({'z':z,'centroid':a.mean(axis=0).tolist(),'count':len(a)})
record={'sourceSha256':before,'yawDegrees':180.,'originAfterRotation':origin.tolist(),'scale':float(scale),
    'frame':'source -Y mouth face rotated to +Y authoring forward; +Z up; height 2m; lowest point Z=0',
    'sourceGeometry':'lightweight original GLB, retain source UVs and materials',
    'parts':parts,'slices':slices,'forelimbSurfaceGuides':guides}
dest=PROJECT/'Saved/CharacterAnimationAudit'
(dest/'shogg_anatomy.json').write_text(json.dumps(record,indent=2)+'\n')
if '--write-config' in sys.argv:
    config=PROJECT/'Config/CharacterAnatomicalFrames.json'
    frames=json.loads(config.read_text())
    assert 'shoggoths' not in frames,'Preserve artist calibration'
    frames['shoggoths']={k:v for k,v in record.items() if k not in ['slices','parts']}
    config.write_text(json.dumps(frames,indent=2)+'\n')
scene=bpy.context.scene;scene.world=bpy.data.worlds.new('Shogg Anatomy');scene.world.color=(.035,.045,.055)
scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.color_type='OBJECT';scene.display.shading.show_cavity=True
scene.display.shading.background_type='WORLD';scene.render.resolution_x=scene.render.resolution_y=800;scene.render.resolution_percentage=100
data=bpy.data.cameras.new('Anatomy Reference');data.type='ORTHO';data.ortho_scale=2.9
camera=bpy.data.objects.new('Anatomy Reference',data);scene.collection.objects.link(camera);scene.camera=camera
for label,location in [('front',(0,5,1)),('side',(5,0,1))]:
    camera.location=location;camera.rotation_euler=(Vector((0,0,1))-camera.location).to_track_quat('-Z','Y').to_euler()
    scene.render.filepath=str(dest/('shoggoths_canonical_'+label+'.png'));bpy.ops.render.render(write_still=True)
assert hashlib.sha256(source.read_bytes()).hexdigest()==before
print('SHOGG_ANATOMY',json.dumps({'parts':parts,'forelimbSurfaceGuides':guides}),flush=True)
