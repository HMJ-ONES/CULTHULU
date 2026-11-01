"""Read-only skinned sole contact and stance-slide measurements for biped studies."""
from pathlib import Path
import json
import sys
import bpy
import numpy as np

assert bpy.app.background
ROOT=Path(__file__).resolve().parents[2]
source=ROOT/sys.argv[sys.argv.index('--')+1]
bpy.ops.wm.open_mainfile(filepath=str(source),use_scripts=False)
rig=next(o for o in bpy.context.scene.objects if o.type=='ARMATURE')
mesh=next(o for o in bpy.context.scene.objects if o.type=='MESH')
rest=np.array([tuple(v.co) for v in mesh.data.vertices])
ankles=np.array([tuple(rig.data.bones['foot_'+side].head_local) for side in ['L','R']])
assignment=np.linalg.norm(rest[:,None,:2]-ankles[None,:,:2],axis=2).argmin(axis=1)
soles={side:np.where((rest[:,2]<.14)&(assignment==i))[0] for i,side in enumerate(['L','R'])}
report={}
for role in ['walk','run']:
    action=next(a for a in bpy.data.actions if a.get('CultUlhu.MotionRole')==role)
    rig.animation_data.action=action
    if action.slots:rig.animation_data.action_slot=action.slots[0]
    start,end=action.frame_range
    duty=action['CultUlhu.StanceFraction']
    duration=(end-start)/bpy.context.scene.render.fps
    speed=action['CultUlhu.StanceTravelMetres']/duty/duration
    contacts={s:[] for s in soles}
    for frame in np.arange(start,end+.25,.5):
        phase=(frame-start)/(end-start)
        bpy.context.scene.frame_set(int(frame),subframe=float(frame%1))
        vertices=mesh.evaluated_get(bpy.context.evaluated_depsgraph_get()).data.vertices
        p=np.empty(len(vertices)*3,dtype=np.float32);vertices.foreach_get('co',p);p=p.reshape(-1,3)
        for side,offset in [('L',0),('R',.5)]:
            u=(phase+offset)%1
            if .20*duty<u<.75*duty:
                contact=p[soles[side]]
                contacts[side].append({'time':float(phase*duration),'lowestZ':float(contact[:,2].min()),
                    'worldY':float(contact[:,1].mean()+speed*phase*duration)})
    row={'authoredStanceSpeedMetresPerSecond':speed,'soles':{}}
    for side,samples in contacts.items():
        heights=[r['lowestZ'] for r in samples]
        # Report each contiguous stance independently; cycle wrap changes which
        # patch of terrain an in-place foot is standing on.
        groups=[[]]
        for sample in samples:
            if groups[-1] and sample['time']-groups[-1][-1]['time']>.1:
                groups.append([])
            groups[-1].append(sample)
        row['soles'][side]={'vertices':len(soles[side]),'stanceSamples':len(samples),
            'minimumZ':min(heights),'maximumZ':max(heights),
            'maximumStanceSlideMetres':max(max(p['worldY'] for p in g)-min(p['worldY'] for p in g) for g in groups)}
    report[role]=row
print('SKINNED_FOOT_CONTACTS',json.dumps(report,indent=2),flush=True)
