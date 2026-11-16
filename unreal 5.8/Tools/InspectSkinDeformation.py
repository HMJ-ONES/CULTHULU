"""Read-only rest/pose edge diagnostics for anatomical weighting mistakes."""
from pathlib import Path
import json
import sys
import bpy
import numpy as np
from mathutils import Vector

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
if '--components' in args:
    selected=rest[:,2]<.65
    adjacency=[[] for _ in rest]
    for a,b in edges:
        if selected[a] and selected[b]:adjacency[a].append(int(b));adjacency[b].append(int(a))
    visited=set();parts=[]
    for start in np.where(selected)[0]:
        if int(start) in visited:continue
        visited.add(int(start));queue=[int(start)];component=[]
        while queue:
            vertex=queue.pop();component.append(vertex)
            for other in adjacency[vertex]:
                if other not in visited:visited.add(other);queue.append(other)
        if len(component)<20:continue
        p=rest[component];slices=[]
        for z in [.12,.35,.55]:
            band=p[np.abs(p[:,2]-z)<.09]
            if len(band):slices.append({'z':z,'centroid':band.mean(axis=0).tolist()})
        parts.append({'vertices':len(component),'centroid':p.mean(axis=0).tolist(),'slices':slices})
    print('LOWER_COMPONENTS',json.dumps(sorted(parts,key=lambda r:-r['vertices']),indent=2),flush=True)
if '--markers' in args:
    rig.data.pose_position='REST'
    selected={e['vertex'] for row in result for e in row['endpoints']}
    for index in selected:
        bpy.ops.mesh.primitive_ico_sphere_add(subdivisions=1,radius=.014,location=rest[index])
        bpy.context.object.color=(1,.08,.03,1)
    center=Vector(np.array([rest[i] for i in selected]).mean(axis=0))
    camera=bpy.context.scene.camera
    camera.location=center+Vector((-2,-3,1.5))
    camera.rotation_euler=(center-camera.location).to_track_quat('-Z','Y').to_euler()
    camera.data.ortho_scale=.8
    target=ROOT/args[0]
    output=target.parent/'skin_diagnostic_rest.png'
    assert not output.exists()
    bpy.context.scene.render.filepath=str(output)
    bpy.ops.render.render(write_still=True)
