"""Read-only skinned contact and stance-slide measurements; --quadruped adds paws."""
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
quadruped='--quadruped' in sys.argv
limbs=[('hand_L','hand_L',0),('hand_R','hand_R',.5),('foot_L','foot_L',.5),('foot_R','foot_R',0)] if quadruped else [('L','foot_L',0),('R','foot_R',.5)]
ankles=np.array([tuple(rig.data.bones[bone].head_local) for _,bone,_ in limbs])
assignment=np.linalg.norm(rest[:,None,:2]-ankles[None,:,:2],axis=2).argmin(axis=1)
soles={}
for i,(name,_,_) in enumerate(limbs):
    # Measure the support patch, not the lower half of a leg. The asymmetric
    # quadruped has differently shaped knuckles and rounded hind-foot caps.
    height=float(rest[assignment==i,2].min())+.06 if quadruped else .14
    soles[name]=np.where((rest[:,2]<height)&(assignment==i))[0]
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
        for side,_,offset in limbs:
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
