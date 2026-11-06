"""Independently reimport approved source and compare a saved supplied-rig study.

Checks raw geometry/UV/skin data, rest bones, every supplied key and original
motion duration. Also compares evaluated Prowl surfaces after removing only the
new authoring-frame wrapper. Never saves either source or artist Blender file.
"""
from pathlib import Path
import json
import runpy
import sys
import bpy
import numpy as np

assert bpy.app.background
PROJECT=Path(__file__).resolve().parents[1];ROOT=PROJECT.parent
api=runpy.run_path(str(PROJECT/'Tools/CultSuppliedRigMotion.py'))
sha,rig_signature,mesh_signature,action_signature=[api[n] for n in ['sha','rig_signature','mesh_signature','action_signature']]
target=(ROOT/sys.argv[sys.argv.index('--')+1]).resolve();assert target.is_relative_to(ROOT)
record=json.loads((target.parent/'report.json').read_text());key=record['character']
assert key in ['mi_go','yig']
output=target.parent/'source_preservation.json';assert not output.exists()
before=sha(target);source=api['approved_source'](key);source_sha=sha(source)
assert source_sha==record['sourceSha256']
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.import_scene.fbx(filepath=str(source),use_anim=True)
scene=bpy.context.scene;fps=(scene.render.fps,scene.render.fps_base)
rigs=[o for o in scene.objects if o.type=='ARMATURE'];meshes=[o for o in scene.objects if o.type=='MESH']
rig_signatures={o.name:rig_signature(o) for o in rigs}
mesh_signatures={o.name:mesh_signature(o) for o in meshes}
action_signatures={a.name:action_signature(a) for a in bpy.data.actions}
active={o.name:o.animation_data.action.name for o in rigs if o.animation_data and o.animation_data.action}
primary=bpy.data.actions[record['sourceAction']];start,end=map(float,primary.frame_range)
frames=np.linspace(start,end,5)
def points(objects,unwrap=None):
    result={};graph=bpy.context.evaluated_depsgraph_get()
    for obj in objects:
        vertices=obj.evaluated_get(graph).data.vertices
        p=np.empty(len(vertices)*3,dtype=np.float32);vertices.foreach_get('co',p)
        matrix=obj.matrix_world if unwrap is None else unwrap@obj.matrix_world
        result[obj.name]=np.einsum('ij,nj->ni',np.array(matrix.to_3x3()),p.reshape(-1,3),optimize=False)+np.array(matrix.translation)
    return result
samples=[]
for frame in frames:
    scene.frame_set(int(frame),subframe=float(frame%1));samples.append(points(meshes))
height=max(float(np.ptp(p[:,2])) for sample in samples for p in sample.values())
bpy.ops.wm.open_mainfile(filepath=str(target),use_scripts=False)
assert fps==(bpy.context.scene.render.fps,bpy.context.scene.render.fps_base),'Keep original motion time base'
for name,digest in rig_signatures.items():assert rig_signature(bpy.data.objects[name])==digest,(name,'rest skeleton changed')
for name,digest in mesh_signatures.items():assert mesh_signature(bpy.data.objects[name])==digest,(name,'geometry, skin or UV data changed')
for name,digest in action_signatures.items():assert action_signature(bpy.data.actions[name])==digest,(name,'supplied keys changed')
for name,action in active.items():
    rig=bpy.data.objects[name];rig.animation_data.action=bpy.data.actions[action]
    if rig.animation_data.action.slots:rig.animation_data.action_slot=rig.animation_data.action.slots[0]
unwrap=bpy.data.objects[record['normalizationObject']].matrix_world.inverted()
errors=[]
for frame,original in zip(frames,samples):
    bpy.context.scene.frame_set(int(frame),subframe=float(frame%1))
    actual=points([bpy.data.objects[n] for n in mesh_signatures],unwrap)
    errors.append(max(float(np.linalg.norm(actual[n]-original[n],axis=1).max()) for n in original))
assert max(errors)/height<.00001,('Supplied evaluated motion changed',errors,height)
assert sha(source)==source_sha and sha(target)==before
result={'character':key,'blendSha256':before,'source':source.relative_to(ROOT).as_posix(),'sourceSha256':source_sha,
    'sourceTimeBase':{'fps':fps[0],'fpsBase':fps[1]},'sourceActionSeconds':(end-start)/(fps[0]/fps[1]),
    'sourceFrames':list(map(float,frames)),'evaluatedMotionMaximumVertexErrorsSourceUnits':errors,
    'maximumErrorFractionOfSourceHeight':max(errors)/height,'restSkeletonsUnchanged':True,
    'geometryUVsMaterialSlotsAndRawWeightsUnchanged':True,'allSuppliedKeysAndHandlesUnchanged':True,
    'rigSignatures':rig_signatures,'meshSignatures':mesh_signatures,'actionSignatures':action_signatures,
    'scope':'source preservation only; artistic quality and game integration remain unverified'}
output.write_text(json.dumps(result,indent=2)+'\n')
print('SUPPLIED_RIG_PRESERVATION_VERIFIED',json.dumps(result),flush=True)
