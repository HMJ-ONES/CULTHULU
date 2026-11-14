"""Floating Yog-Sothoth basic motion studies on its measured woven anatomy.

No humanoid limbs, spell casting or new abilities. Original retopology preserved.
Eight blended lower columns follow the measured surface and share soft motion.
"""
from pathlib import Path
import json
import math
import runpy
import sys
import numpy as np
from mathutils import Vector

PROJECT=Path(__file__).resolve().parents[1]
helper=runpy.run_path(str(PROJECT/'Tools/CultMotionStudy.py'))
Study,rot,envelope=[helper[n] for n in ['Study','rot','envelope']]
revision=sys.argv[sys.argv.index('--')+1] if '--' in sys.argv else 'v001'
study=Study('yog_sothoth','YogSothoth',revision,(.36,.34,.43,1))
anatomy=json.loads((PROJECT/'Config/CharacterAnatomicalFrames.json').read_text())['yog_sothoth']
center=Vector(anatomy['upperMassCentroid']);center.z=1.05
bone=study.bone
bone('root',(center.x,center.y,0),(center.x,center.y,.16),deform=False)
bone('pelvis',center,(center.x,center.y,1.25),'root')
bone('core_mid',(center.x,center.y,1.25),(center.x,center.y,1.57),'pelvis')
bone('core_top',(center.x,center.y,1.57),(center.x,center.y,1.87),'core_mid')
points=np.array([tuple(v.co) for v in study.mesh.data.vertices])
upper=points[points[:,2]>1.03]
# Eight local upper-mass anchors allow small organic follow-through without
# detaching orbs or assigning invented articulated arms to the source sculpture.
orb_centers=np.array([[center.x+math.cos(a)*.23,center.y+math.sin(a)*.23,z]
    for z in [1.22,1.65] for a in np.arange(4)*math.tau/4])
for iteration in range(16):
    labels=np.linalg.norm(upper[:,None,:]-orb_centers[None,:,:],axis=2).argmin(axis=1)
    orb_centers=np.array([upper[labels==i].mean(axis=0) if (labels==i).any() else orb_centers[i] for i in range(8)])
for i,c in enumerate(orb_centers):
    bone('orb_%02d'%i,c,c+np.array([0,0,.09]),'core_top' if c[2]>1.55 else 'core_mid')
groups={'core':['pelvis','core_mid','core_top']+['orb_%02d'%i for i in range(8)]}
seeds={'core':[tuple(c) for c in orb_centers]}
chains={}
angles={}
for i,path in enumerate(anatomy['tentacleChains']):
    names=[]
    for j,(a,b) in enumerate(zip(path,path[1:])):
        name='tendril_%02d_%02d'%(i,j)
        bone(name,a,b,'pelvis' if j==0 else names[-1]);names.append(name)
    chains[i]=names
    groups['column_%02d'%i]=names
    seeds['column_%02d'%i]=path[1:]
    root=Vector(path[0])-center
    angles[i]=math.atan2(root.y,root.x)
z=points[:,2]
def ramp(value,a,b):
    t=np.clip((value-a)/(b-a),0,1)
    return t*t*(3-2*t)
masks={'core':ramp(z,.72,1.03)}
for i in chains:
    masks['column_%02d'%i]=1-ramp(z,.9,1.17)
study.bind(groups,seeds,masks,region_proximity=.50)
study.rig['CultUlhu.Anatomy']='floating orb cluster and woven hanging appendages; eight blended columns'
left_weights={i:max(0,math.cos(a))*(.25+.75*max(0,math.sin(a))) for i,a in angles.items()}
right_weights={i:max(0,-math.cos(a))*(.25+.75*max(0,math.sin(a))) for i,a in angles.items()}
left_peak=max(left_weights.values());right_peak=max(right_weights.values())

def pose(role,t):
    phase=math.tau*t
    shift=Vector((0,0,.045+.012*math.sin(phase)))
    q={'pelvis':rot(y=math.sin(phase)*.55),
       'core_mid':rot(x=math.sin(phase-.5)*.6),
       'core_top':rot(z=math.sin(phase-.9)*.8)}
    hit=envelope(t,[(0,0),(.26,-.22),(.50,1),(.70,.68),(1,0)])
    load=envelope(t,[(0,0),(.26,1),(.50,0),(.70,0),(1,0)])
    strike=max(0,hit)
    flare=envelope(t,[(0,0),(.25,.85),(.48,1),(.76,.9),(1,0)])
    air=envelope(t,[(0,0),(.23,0),(.55,1),(.82,.8),(1,.5)])
    if role in ['walk','run']:
        q['pelvis']=rot(x=-3 if role=='walk' else -6,y=math.sin(phase)*.9)
        q['core_top']=rot(x=1 if role=='walk' else 2,z=math.sin(phase-.6)*1.2)
    if role.startswith('melee_'):
        q['pelvis']=rot(x=-3*strike,z=(3 if role=='melee_1' else -2 if role=='melee_2' else 0)*hit)
        q['core_top']=rot(x=2*strike)
    if role=='dash':
        q['pelvis']=rot(x=-8*math.sin(math.pi*t)**2)
        shift.z+=.01*math.sin(math.pi*t)**2
    if role=='jump':
        shift.z-=.025*load
        q['core_mid']=rot(x=-3*load)
    if role=='land':
        shift.z-=.025*math.sin(math.pi*t)**2
    if role=='taunt':
        shift.z+=.035*flare
        q['core_mid']=rot(x=math.sin(phase)*1.4*flare)
        q['core_top']=rot(z=math.sin(phase-.6)*2*flare)
    for i,names in chains.items():
        theta=angles[i]
        front=max(0,math.sin(theta))
        left=max(0,math.cos(theta))
        right=max(0,-math.cos(theta))
        for j,name in enumerate(names):
            lag=phase-j*.65+theta
            x=math.sin(lag)*1.8
            y=math.cos(lag)*1.8
            if role in ['walk','run']:
                fast=role=='run'
                x+=-(3.0 if not fast else 7.0)*(1+j*.06)+math.sin(lag)*(1.2 if fast else .5)
            elif role=='melee_1':
                weight=left_weights[i]/left_peak
                x+=10*hit*weight*(1-j*.08)
                y+=11*hit*weight*(1-j*.08)
            elif role=='melee_2':
                weight=right_weights[i]/right_peak
                x+=20*hit*weight*(1-j*.08)
                y-=3*hit*weight
            elif role=='melee_3':
                weight=.18+.82*front
                x+=14*strike*weight*(1-j*.06)
                y+=math.cos(theta)*weight*(-9*load+8*strike)
            elif role=='jump':
                x-=math.sin(theta)*(6+j)*air
                y+=math.cos(theta)*(6+j)*air
            elif role=='dash':
                x-=(8+j*.8)*math.sin(math.pi*t)**2
            elif role=='taunt':
                x+=math.sin(theta)*(7+j*.5)*flare
                y-=math.cos(theta)*(7+j*.5)*flare
            elif role=='land':
                x-=3*math.sin(math.pi*t)**2
            q[name]=rot(x=x,y=y)
    for i in range(8):
        q['orb_%02d'%i]=rot(x=math.sin(phase-i*.7)*.65,y=math.cos(phase-i*.7)*.65)
    world=study.fk(q,shift)
    for i,c in enumerate(orb_centers):
        angle=math.atan2(c[1]-center.y,c[0]-center.x)
        pulse=math.sin(phase-i*.7)*(.004 if role!='taunt' else .007)
        world['orb_%02d'%i].translation+=Vector((math.cos(angle)*pulse,math.sin(angle)*pulse,pulse*.35))
    return world

roles={'idle':4.4,'walk':1.8,'run':1.1,'melee_1':.75,'melee_2':.85,'melee_3':1.0,'jump':1.1,'dash':.52,'taunt':3.4,'land':.42}
study.bake(roles,pose)
for role in ['walk','run']:
    study.actions[role]['CultUlhu.Locomotion']='floating glide with delayed appendage drag'
    study.actions[role]['CultUlhu.CycleTravelMetres']=.7 if role=='walk' else 1.35
study.save(center=(center.x,center.y,1.04),scale=3.0)
