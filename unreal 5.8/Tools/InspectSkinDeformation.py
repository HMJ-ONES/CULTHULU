"""Read-only rest/pose edge diagnostics for anatomical weighting mistakes."""
from pathlib import Path
import json
import sys
import bpy
import numpy as np

assert bpy.app.background
ROOT=Path(__file__).resolve().parents[2]
args=sys.argv[sys.argv.index('--')+1:]
bpy.ops.wm.open_mainfile(filepath=str(ROOT/args[0]),use_scripts=False)
rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
mesh=next(o for o in bpy.context.scene.objects if o.type=='MESH')
action=next(a for a in bpy.data.actions if a.get('CultUlhu.MotionRole')==args[1])
rig.animation_data.action=action
if action.slots:rig.animation_data.action_slot=action.slots[0]
bpy.context.scene.frame_set(round(action.frame_range[0]+float(args[2])*(action.frame_range[1]-action.frame_range[0])))
rest=np.array([tuple(v.co) for v in mesh.data.vertices])
actual=np.array([tuple(v.co) for v in mesh.evaluated_get(bpy.context.evaluated_depsgraph_get()).data.vertices])
edges=np.array([tuple(e.vertices) for e in mesh.data.edges])
lengths=np.linalg.norm(rest[edges[:,0]]-rest[edges[:,1]],axis=1)
ratios=np.linalg.norm(actual[edges[:,0]]-actual[edges[:,1]],axis=1)/np.maximum(lengths,1e-8)
result=[]
for i in np.argsort(ratios)[-15:][::-1]:
    row={'stretch':float(ratios[i]),'restLength':float(lengths[i]),'endpoints':[]}
    for v in edges[i]:
        row['endpoints'].append({'vertex':int(v),'rest':rest[v].tolist(),
            'posed':actual[v].tolist(),'weights':{mesh.vertex_groups[g.group].name:g.weight for g in mesh.data.vertices[int(v)].groups}})
    result.append(row)
print('DEFORMATION_DIAGNOSTIC',json.dumps(result,indent=2),flush=True)
