"""Measure hanging appendages from the reviewed Yog-Sothoth surface graph.

Only reads source geometry; outputs observations under canonical Saved/.
Native multi-angle review places the main eyeballs on the source +X face.
Rotate that face to the +Y authoring forward direction before tracing columns.
"""
from pathlib import Path
import hashlib
import json
import math
import sys
import bpy
import numpy as np
from mathutils import Matrix

assert bpy.app.background
PROJECT=Path(__file__).resolve().parents[1]
ROOT=PROJECT.parent
source=ROOT/'assets/retopology/yog_sothoth_retopology.obj'
digest=hashlib.sha256(source.read_bytes()).hexdigest()
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.wm.obj_import(filepath=str(source),forward_axis='Y',up_axis='Z')
mesh=next(o for o in bpy.context.scene.objects if o.type=='MESH')
rotation=Matrix.Rotation(math.pi*.5,4,'Z')
p=np.array([tuple(rotation@(mesh.matrix_world@v.co)) for v in mesh.data.vertices])
low,high=p.min(axis=0),p.max(axis=0)
origin=np.array([(low[0]+high[0])/2,(low[1]+high[1])/2,low[2]])
scale=2/(high[2]-low[2]);p=(p-origin)*scale
edges=np.array([tuple(e.vertices) for e in mesh.data.edges])
observations=[]
for cut in [.65,.8,.95]:
    selected=p[:,2]<cut
    adjacency=[[] for _ in p]
    for a,b in edges:
        if selected[a] and selected[b]:
            adjacency[a].append(int(b));adjacency[b].append(int(a))
    visited=set();parts=[]
    for v in np.where(selected)[0]:
        if int(v) in visited:
            continue
        visited.add(int(v));queue=[int(v)];component=[]
        while queue:
            i=queue.pop();component.append(i)
            for other in adjacency[i]:
                if other not in visited:
                    visited.add(other);queue.append(other)
        if len(component)>80:
            a=p[component]
            slices=[]
            for z in np.linspace(cut-.03,max(.035,float(a[:,2].min())+.025),6):
                band=a[np.abs(a[:,2]-z)<.06]
                if len(band)>3:
                    center=band.mean(axis=0);center[2]=z
                    slices.append(center.tolist())
            parts.append({'vertices':len(component),'bounds':[a.min(axis=0).tolist(),a.max(axis=0).tolist()],
                'centroid':a.mean(axis=0).tolist(),'centerline':slices})
    observations.append({'cutHeight':cut,'parts':sorted(parts,key=lambda r:-r['vertices'])})
report={'sourceSha256':digest,'yawDegrees':90.,'originAfterRotation':origin.tolist(),'scale':float(scale),
    'frame':'source +X eyeball face rotated to +Y authoring forward; +Z up; height 2m; lowest tip Z=0',
    'upperMassCentroid':p[p[:,2]>1.0].mean(axis=0).tolist(),'observations':observations}
# The sculpt's woven lower surfaces share graph connections. Track eight
# spatial columns through horizontal sections instead of assuming each graph
# component is one independent appendage. These are skeleton guides to review.
center=p[p[:,2]>1.0,:2].mean(axis=0)
band=p[(p[:,2]>.20)&(p[:,2]<.45),:2]
angles=np.arange(8)*math.tau/8
centers=center+np.stack([np.cos(angles),np.sin(angles)],axis=1)*.30
for iteration in range(20):
    labels=np.linalg.norm(band[:,None,:]-centers[None,:,:],axis=2).argmin(axis=1)
    centers=np.array([band[labels==i].mean(axis=0) if (labels==i).any() else centers[i] for i in range(8)])
order=np.argsort(np.arctan2(centers[:,1]-center[1],centers[:,0]-center[0]))
centers=centers[order]
tracks=[[] for _ in range(8)]
for z in [.12,.30,.48,.66,.84,1.02]:
    section=p[np.abs(p[:,2]-z)<.09,:2]
    current=centers.copy()
    for iteration in range(8):
        labels=np.linalg.norm(section[:,None,:]-current[None,:,:],axis=2).argmin(axis=1)
        target=np.array([section[labels==i].mean(axis=0) if (labels==i).any() else current[i] for i in range(8)])
        current=.65*target+.35*current
    centers=.65*current+.35*centers
    for i in range(8):
        tracks[i].append([float(centers[i,0]),float(centers[i,1]),z])
report['tentacleChains']=[list(reversed(chain)) for chain in tracks]
report['guideMethod']='eight continuous spatial columns through the woven lower surface; anatomical review required'
dest=PROJECT/'Saved/CharacterAnimationAudit/yog_anatomy.json'
previous=json.loads(dest.read_text()) if dest.exists() else None
dest.write_text(json.dumps(report,indent=2)+'\n')
if '--write-config' in sys.argv or '--replace-measured-config' in sys.argv:
    config=PROJECT/'Config/CharacterAnatomicalFrames.json'
    frames=json.loads(config.read_text())
    if 'yog_sothoth' in frames:
        assert '--replace-measured-config' in sys.argv and previous is not None
        assert frames['yog_sothoth']=={k:v for k,v in previous.items() if k!='observations'}, 'Preserve artist changes to calibration'
    frames['yog_sothoth']={k:v for k,v in report.items() if k!='observations'}
    config.write_text(json.dumps(frames,indent=2)+'\n')
assert hashlib.sha256(source.read_bytes()).hexdigest()==digest
print('YOG_ANATOMY',str(dest),flush=True)
for row in observations:
    print('CUT',row['cutHeight'],'COMPONENTS',[(r['vertices'],r['centroid']) for r in row['parts']],flush=True)
