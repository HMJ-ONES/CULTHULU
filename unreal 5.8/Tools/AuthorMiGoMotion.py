"""Mi-Go scythe combos, springing locomotion and wing threat display.

Supplied geometry/rig/weights and Prowl keys stay unchanged. Prowl is dash.
No casting or ability animations; new actions use the original source time base.
"""
from pathlib import Path
import math
import runpy
import sys
import numpy as np
from mathutils import Matrix, Vector

PROJECT=Path(__file__).resolve().parents[1]
helper=runpy.run_path(str(PROJECT/'Tools/CultSuppliedRigMotion.py'))
SuppliedStudy,rot,smooth,envelope=[helper[n] for n in ['SuppliedStudy','rot','smooth','envelope']]
revision=sys.argv[sys.argv.index('--')+1] if '--' in sys.argv else 'v001'
study=SuppliedStudy('mi_go','MiGo',revision,180.)
mesh=study.meshes[0];assert len(study.meshes)==1
blade_frames={}
reference_points=study.points()
for side in ['l','r']:
    index=mesh.vertex_groups['Claw.'+side].index
    weights=np.array([sum(g.weight for g in v.groups if g.group==index)/max(1e-12,sum(g.weight for g in v.groups)) for v in mesh.data.vertices])
    selected=reference_points[weights>.35];assert len(selected)>5
    centered=selected-selected.mean(axis=0)
    _,vectors=np.linalg.eigh(centered.T@centered)
    axis=Vector(vectors[:,-1]);normal=Vector(vectors[:,0])
    if axis.dot(study.tails['Claw.'+side]-study.heads['Claw.'+side])<0:axis=-axis
    blade_frames[side]=Matrix((axis,normal.cross(axis),normal)).transposed()
study.rig.data.pose_position='REST'
rest_points=study.points()
study.rig.data.pose_position='POSE'
sole_indices={}
feet={}
for side in ['l','r']:
    sign=1 if study.rest_world['toebase.'+side].translation.x>0 else -1
    selected=np.where((rest_points[:,2]<rest_points[:,2].min()+.14)&(rest_points[:,0]*sign>.015))[0]
    assert len(selected)>5
    sole_indices[side]=selected
    offset=study.rest_world['toebase.'+side].translation.z-float(rest_points[selected,2].min())
    y=sum(study.heads['toebase.'+s].y for s in ['l','r'])/2
    feet[side]=Vector((sign*.18,y,offset+.002))

def pose(role,t,corrections=None):
    phase=math.tau*t
    q={};shift=Vector((0,0,-.012))
    targets={s:v.copy() for s,v in feet.items()}
    hands={s:study.heads['Claw.'+s].copy() for s in ['l','r']}
    blade={s:0. for s in ['l','r']}
    if role=='idle':
        q['Torso']=rot(x=math.sin(phase)*.7)
        q['Head']=rot(z=math.sin(phase)*1.1)
    if role in ['walk','run']:
        fast=role=='run';duty=.52 if fast else .64;stride=.50 if fast else .30
        shift.x=math.sin(phase)*(.012 if fast else .008)
        shift.z-=.025 if fast else .008
        shift.z+=math.cos(phase*2)*(.018 if fast else .009)
        q['Spine']=rot(x=-3 if fast else -1.2,z=math.sin(phase)*1.2)
        q['Torso']=rot(z=-math.sin(phase)*2)
        q['Head']=rot(x=2 if fast else .7)
        for side,offset in [('l',0),('r',.5)]:
            u=(t+offset)%1
            if u<duty:targets[side].y+=stride*(.5-u/duty)
            else:
                f=(u-duty)/(1-duty)
                targets[side].y+=stride*(-.5+smooth(f))
                targets[side].z+=(.14 if fast else .075)*math.sin(math.pi*f)**2
            hands[side].y+=math.sin(phase+offset*math.tau)*(.065 if fast else .035)
    if role.startswith('melee_'):
        hit=envelope(t,[(0,0),(.22,-.20),(.48,1),(.68,.7),(1,0)])
        strike=max(0,hit)
        if role=='melee_1':
            hands['l']+=Vector((.45*hit,.42*hit,.12*strike))
            blade['l']=-35*hit
            q['Torso']=rot(z=-8*hit);q['Head']=rot(z=3*hit)
        elif role=='melee_2':
            hands['r']+=Vector((-.27*hit,.40*hit,.34*strike))
            blade['r']=45*hit
            q['Torso']=rot(z=8*hit,x=-2*strike)
        else:
            for side in ['l','r']:
                sign=1 if study.heads['Claw.'+side].x>0 else -1
                hands[side]+=Vector((-sign*.42*hit,.36*hit,(.12 if side=='l' else .05)*strike))
                blade[side]=sign*100*hit
                q['minibicep.'+side]=rot(x=12*strike)
                q['miniforearm.'+side]=rot(x=-8*strike)
            q['Spine']=rot(x=-4*strike)
    if role=='jump':
        load=envelope(t,[(0,0),(.22,1),(.42,0),(1,0)])
        air=envelope(t,[(0,0),(.26,0),(.5,1),(.8,.8),(1,.45)])
        shift.z-=.09*load
        q['Torso']=rot(x=-3*load);q['Head']=rot(x=5*air)
        for side in targets:
            targets[side].z+=.18*air
            targets[side].y-=.08*air
            hands[side].z+=.12*air
    if role=='land':
        settle=math.sin(math.pi*t)**2
        shift.z-=.075*settle;q['Torso']=rot(x=-4*settle)
    flare=envelope(t,[(0,0),(.22,.75),(.45,1),(.76,.9),(1,0)]) if role=='taunt' else 0.
    if role=='taunt':
        q['Torso']=rot(x=6*flare);q['Head']=rot(x=10*flare,z=math.sin(phase)*3*flare)
        for side in hands:
            sign=1 if study.heads['Claw.'+side].x>0 else -1
            hands[side]+=Vector((sign*.13*flare,.10*flare,.24*flare))
            blade[side]=sign*12*flare
            q['Jaw.'+side]=rot(z=sign*8*flare)
            for i in range(1,4):q['Pincer%d.'%i+side]=rot(z=sign*flare*(3+i))
    q['AbdomenBase']=rot(x=math.sin(phase-.5)*1.2,y=math.sin(phase)*1.0)
    q['abdomen']=rot(z=-math.sin(phase-.6)*1.8)
    q['stinger']=rot(z=-math.sin(phase-1)*1.2)
    for side in ['l','r']:
        sign=1 if study.heads['Claw.'+side].x>0 else -1
        beats=4 if role=='run' else 2
        amplitude=3 if role=='run' else .8
        if role=='jump':amplitude=4
        for pair in [1,2]:
            wave=math.sin(phase*beats-(pair-1)*.7)
            q['wingbase%d.'%pair+side]=rot(y=sign*(wave*amplitude+flare*(16 if pair==1 else 10)))
            q['wing%d.'%pair+side]=rot(y=sign*math.sin(phase*beats-.6)*amplitude*.4)
            q['wingtip%d.'%pair+side]=rot(x=math.sin(phase*beats-1)*amplitude*.2)
        if 'minibicep.'+side not in q:
            q['minibicep.'+side]=rot(x=math.sin(phase)*sign*2+flare*8)
        q['miniclaw1.'+side]=rot(z=sign*math.sin(phase-.4)*1.2)
        q['miniclaw2.'+side]=rot(z=-sign*math.sin(phase-.4)*1.2)
    world=study.fk(q,shift);overrides={}
    for side in ['l','r']:
        toe='toebase.'+side
        if corrections:targets[side].z+=corrections[side]
        leg=['Thigh.'+side,'Shin.'+side,'Foot.'+side]
        template=[study.heads[n+'.l']-study.heads['Thigh.l'] for n in ['Thigh','Shin','Foot','toebase']]
        if side=='r':
            for offset in template:offset.x=-offset.x
        preferred=[world[leg[0]].translation+offset for offset in template]
        endpoint=study.chain(world,leg,toe,targets[side],initial_points=preferred)
        for name in leg:overrides[name]=world[name]
        world[toe]=Matrix.Translation(endpoint)@study.rest_world[toe].to_3x3().to_4x4()
        study.descend(world,toe,study.rest_world)
        descendants=[study.rig.data.bones[toe]]+list(study.rig.data.bones[toe].children_recursive)
        for bone in descendants:overrides[bone.name]=world[bone.name]
        arm=['Bicep.'+side,'Forearm.'+side];claw='Claw.'+side
        endpoint=study.chain(world,arm,claw,hands[side])
        for name in arm:overrides[name]=world[name]
        follow=world[arm[-1]]@study.base[arm[-1]].inverted()@study.base[claw]
        if role.startswith('melee_'):
            hit=envelope(t,[(0,0),(.22,-.20),(.48,1),(.68,.7),(1,0)])
            strike=max(0,hit)
            sign=1 if study.heads[claw].x>0 else -1
            direction=Vector((-sign*.7,.7,0)) if role=='melee_3' else Vector((.8,.6,.05)) if side=='l' else Vector((-.3,.8,.45))
            direction.normalize()
            normal=Vector((sign*.15,.4,.8))
            normal=(normal-direction*normal.dot(direction)).normalized()
            desired_frame=Matrix((direction,normal.cross(direction),normal)).transposed()
            delta=desired_frame@blade_frames[side].transposed()
            aimed=Matrix.Translation(endpoint)@delta.to_4x4()@study.base[claw].to_3x3().to_4x4()
            # Hold the cut's intended plane at contact instead of inheriting a
            # rolled forearm frame that turns a crossing blade into a spear.
            blend=strike if role=='melee_3' or (role=='melee_1' and side=='l') or (role=='melee_2' and side=='r') else 0
            rotation=follow.to_quaternion().slerp(aimed.to_quaternion(),blend)
            scale=study.base[claw].to_scale()
            overrides[claw]=Matrix.Translation(endpoint)@rotation.to_matrix().to_4x4()@Matrix.Diagonal((*scale,1))
        else:
            overrides[claw]=Matrix.Translation(endpoint)@rot(z=blade[side]).to_matrix().to_4x4()@follow.to_3x3().to_4x4()
    world=study.fk(q,shift,overrides)
    for side in ['l','r']:
        for controller,target in [('armIK.','Claw.'),('smol armIK.','minipincer.'),('LegIK.','toebase.')]:
            name=controller+side;bone=target+side
            overrides[name]=world[bone]@study.base[bone].inverted()@study.base[name]
        for prefix in ['armPOLE.','smol armPOLE.','LegPOLE.']:
            name=prefix+side
            overrides[name]=world['Torso']@study.base['Torso'].inverted()@study.base[name]
    return study.fk(q,shift,overrides)

def grounded(role,t):
    correction={'l':0.,'r':0.}
    world=pose(role,t,correction)
    for iteration in range(4):
        study.apply(world);points=study.points()
        for side,offset in [('l',0),('r',.5)]:
            minimum=float(points[sole_indices[side],2].min())
            planted=(t+offset)%1<(.52 if role=='run' else .64) if role in ['walk','run'] else t<.26 if role=='jump' else True
            correction[side]-=minimum-.002 if planted else min(0.,minimum-.002)
        world=pose(role,t,correction)
    return world

roles={'idle':3.2,'walk':1.2,'run':.70,'melee_1':.46,'melee_2':.52,'melee_3':.64,'jump':.90,'taunt':2.6,'land':.30}
study.bake(roles,grounded)
for role in ['walk','run']:
    study.actions[role]['CultUlhu.StanceFraction']=.64 if role=='walk' else .52
    study.actions[role]['CultUlhu.StanceTravelMetres']=.30 if role=='walk' else .50
study.save()
