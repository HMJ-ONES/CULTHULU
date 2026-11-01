"""Heavy Cthulhu basic motions with wing fans and facial-tentacle follow-through.

Separate background Blender; reviewed retopology stays immutable. No abilities.
"""
from pathlib import Path
import math
import runpy
import sys
import numpy as np
from mathutils import Vector

PROJECT = Path(__file__).resolve().parents[1]
helper = runpy.run_path(str(PROJECT/'Tools/CultMotionStudy.py'))
Study,rot,envelope,smooth = [helper[n] for n in ['Study','rot','envelope','smooth']]
revision = sys.argv[sys.argv.index('--')+1] if '--' in sys.argv else 'v001'
study = Study('cthulhu_avatar','Cthulhu',revision,(.26,.39,.34,1))
bone = study.bone
bone('root',(0,0,0),(0,0,.16),deform=False)
bone('pelvis',(.06,-.14,.77),(.06,-.10,.98),'root')
bone('spine',(.06,-.10,.98),(.06,-.02,1.20),'pelvis')
bone('chest',(.06,-.02,1.20),(.06,.08,1.43),'spine')
bone('neck',(.06,.08,1.43),(.06,.20,1.62),'chest')
bone('head',(.06,.20,1.62),(.06,.23,1.84),'neck')
tail=[(.06,-.22,.76),(.06,-.33,.55),(.04,-.41,.34),(.02,-.34,.18)]
for i,(a,b) in enumerate(zip(tail,tail[1:])):
    bone('tail_%d'%i,a,b,'pelvis' if i==0 else 'tail_%d'%(i-1))
groups={'body':['pelvis','spine','chest','neck'], 'head':['head'], 'tail':['tail_%d'%i for i in range(3)]}
seeds={'body':[(.06,-.1,.87),(.06,-.05,1.12),(.06,.06,1.39),
               (.06,.23,.84),(.06,.22,1.03),(.06,.17,1.23),
               (.22,.16,.78),(-.10,.16,.78),(.25,.16,.98),(-.13,.16,.98),
               (.21,-.19,.87),(-.09,-.19,.87)],
       'head':[(.06,.21,1.73),(.06,.2,1.84)], 'tail':tail[1:]}
for side,sign in [('L',1),('R',-1)]:
    p=lambda x,y,z: (.06+sign*x,y,z)
    bone('clavicle_'+side,p(.03,.06,1.40),p(.28,.07,1.40),'chest')
    elbow=p(.35,.12,1.05) if side=='L' else p(.40,-.015,1.00)
    wrist=p(.41,.25,.72) if side=='L' else p(.43,.025,.64)
    hand_tip=p(.42,.28,.61) if side=='L' else p(.44,.035,.53)
    bone('upperarm_'+side,p(.28,.07,1.40),elbow,'clavicle_'+side)
    bone('forearm_'+side,elbow,wrist,'upperarm_'+side)
    bone('hand_'+side,wrist,hand_tip,'forearm_'+side)
    for i,x in enumerate([.34,.42,.48]):
        a,b=(p(x,.28,.62),p(x,.30,.54)) if side=='L' else (p(x,.035,.55),p(x+.01,.06,.46))
        bone('claw%d_'%i+side,a,b,'hand_'+side)
    bone('thigh_'+side,p(.16,-.15,.78),p(.20,-.07,.40),'pelvis')
    bone('shin_'+side,p(.20,-.07,.40),p(.24,-.17,.105),'thigh_'+side)
    bone('foot_'+side,p(.24,-.17,.105),p(.27,.08,.045),'shin_'+side)
    # The folded membrane is supported by a leading spar and three fan rays.
    bone('wingbase_'+side,p(.15,-.15,1.34),p(.24,-.08,1.66),'chest')
    bone('wingspar_'+side,p(.24,-.08,1.66),p(.29,.13,1.98),'wingbase_'+side)
    for i,end in enumerate([p(.60,-.18,1.08),p(.57,-.18,1.29),p(.49,-.15,1.50)]):
        bone('wingray%d_'%i+side,p(.29,.13,1.98),end,'wingspar_'+side)
    groups['arm_'+side]=['clavicle_'+side,'upperarm_'+side,'forearm_'+side,'hand_'+side]+['claw%d_'%i+side for i in range(3)]
    seeds['arm_'+side]=[p(.32,.09,1.24),elbow,wrist,hand_tip]
    groups['leg_'+side]=[name+'_'+side for name in ['thigh','shin','foot']]
    seeds['leg_'+side]=[p(.18,-.11,.59),p(.22,-.12,.26),p(.25,-.02,.06)]
    groups['wing_'+side]=['wingbase_'+side,'wingspar_'+side]+['wingray%d_'%i+side for i in range(3)]
    seeds['wing_'+side]=[p(.29,-.01,1.83),p(.44,-.09,1.61),p(.53,-.17,1.27),p(.58,-.26,1.03)]
    # Separate long inner and short outer facial tendrils; joints follow their
    # source surface paths rather than treating the face as a rigid beard.
    for kind,chain in [('inner',[p(.045,.34,1.58),p(.065,.40,1.40),p(.08,.37,1.22),p(.04,.32,1.03)]),
                       ('outer',[p(.12,.32,1.58),p(.18,.38,1.45),p(.23,.34,1.34),p(.19,.31,1.27)])]:
        names=[]
        for i,(a,b) in enumerate(zip(chain,chain[1:])):
            name='tendril_%s%d_%s'%(kind,i,side)
            bone(name,a,b,'head' if i==0 else names[-1]);names.append(name)
        groups['tendril_'+kind+'_'+side]=names
        seeds['tendril_'+kind+'_'+side]=chain[1:]
# Geodesic paths alone are ambiguous where the sculpt's resting hands approach
# its thighs. Anatomical envelopes prevent an arm from skinning an entire leg.
points=np.array([tuple(v.co) for v in study.mesh.data.vertices])
x,y,z=points[:,0]-.06,points[:,1],points[:,2]
def ramp(value,a,b):
    t=np.clip((value-a)/(b-a),0,1)
    return t*t*(3-2*t)
masks={}
for side,sign in [('L',1),('R',-1)]:
    masks['arm_'+side]=ramp(z,.30,.38)*ramp(sign*x,.18,.25)*ramp(y,-.28,-.18)
    masks['leg_'+side]=(1-ramp(z,.83,1.0))*ramp(sign*x,-.08,.03)
    masks['wing_'+side]=ramp(z,.86,.98)*ramp(sign*x,.13,.24)
    for kind in ['inner','outer']:
        masks['tendril_'+kind+'_'+side]=ramp(y,.23,.31)*ramp(z,.93,1.03)*(1-ramp(z,1.60,1.69))
masks['head']=ramp(z,1.47,1.61)
masks['tail']=(1-ramp(y,-.18,-.08))*(1-ramp(z,.73,.9))
study.bind(groups,seeds,masks,region_proximity=.25)

def pose(role,t,sole_corrections=None):
    phase=math.tau*t
    q={};shift=Vector((0,0,0))
    feet={s:study.heads['foot_'+s].copy() for s in ['L','R']}
    if role=='idle':
        q['chest']=rot(x=math.sin(phase)*.65)
        q['neck']=rot(x=-math.sin(phase)*.35)
        q['head']=rot(z=math.sin(phase)*.8)
    if role in ['walk','run']:
        running=role=='run';duty=.55 if running else .65
        stride=.60 if running else .40
        shift.x=math.sin(phase)*(.016 if running else .012)
        shift.z=-.028+math.cos(phase*2)*(.018 if running else .009)
        q['pelvis']=rot(z=math.sin(phase)*2.2)
        q['spine']=rot(x=-6 if running else -2)
        q['chest']=rot(z=-math.sin(phase)*3)
        q['head']=rot(x=3 if running else .8)
        for side,sign,offset in [('L',1,0),('R',-1,.5)]:
            u=(t+offset)%1
            if u<duty:
                feet[side].y+=stride*(.5-u/duty)
            else:
                f=(u-duty)/(1-duty)
                feet[side].y+=stride*(-.5+smooth(f))
                feet[side].z+=(.15 if running else .08)*math.sin(math.pi*f)**2
            q['upperarm_'+side]=rot(x=math.sin(phase+offset*math.tau)*(19 if running else 10),z=sign*3)
            q['forearm_'+side]=rot(x=(16 if running else 4))
    if role.startswith('melee_'):
        kind=int(role[-1])
        hit=envelope(t,[(0,0),(.27,-.25),(.52,1),(.7,.72),(1,0)])
        load=envelope(t,[(0,0),(.27,1),(.52,0),(.7,0),(1,0)])
        strike=max(0,hit)
        if kind==1:
            # Broad left backhand, with the other claw held as a guard.
            q['pelvis']=rot(z=6*hit);q['chest']=rot(z=13*hit)
            q['upperarm_L']=rot(x=45*load+60*strike,y=-25*strike,z=40*load-45*strike)
            q['forearm_L']=rot(y=12*hit,x=25*load-10*strike)
            q['upperarm_R']=rot(x=12*strike);q['forearm_R']=rot(x=18*strike)
        elif kind==2:
            # Right diagonal cleave: raised elbow, downward/forward claw path.
            lift=envelope(t,[(0,0),(.28,1),(.50,.15),(.72,0),(1,0)])
            q['pelvis']=rot(z=-5*hit);q['chest']=rot(z=-11*hit,x=4*max(0,hit))
            q['upperarm_R']=rot(x=105*lift+38*strike,y=10*lift,z=-8*lift)
            q['forearm_R']=rot(x=15*lift+25*strike,y=-8*hit)
            q['hand_R']=rot(x=12*max(0,hit))
        else:
            # Two opposed inward rakes form a wider, weightier combo finisher.
            shift.z=-.045*max(0,hit)
            q['chest']=rot(x=-5*hit)
            for side,sign in [('L',1),('R',-1)]:
                q['upperarm_'+side]=rot(x=50*load+75*strike,y=-sign*40*load+sign*15*strike,z=sign*10*strike)
                q['forearm_'+side]=rot(x=12*load-10*strike,y=sign*8*hit)
                q['hand_'+side]=rot(y=-sign*12*hit)
        q['head']=rot(x=-3*hit)
        for side in ['L','R']:
            for i in range(3):
                q['claw%d_'%i+side]=rot(x=8*max(0,hit))
    if role=='jump':
        crouch=envelope(t,[(0,0),(.22,1),(.42,0),(1,0)])
        air=envelope(t,[(0,0),(.26,0),(.55,1),(.8,.8),(1,.55)])
        shift.z=-.09*crouch
        q['spine']=rot(x=-6*crouch)
        for side,sign in [('L',1),('R',-1)]:
            feet[side].z+=air*.21;feet[side].y-=air*.10
            q['upperarm_'+side]=rot(x=22*air,y=-sign*8*air)
            q['forearm_'+side]=rot(x=15*air)
    if role=='dash':
        surge=math.sin(math.pi*t)**2
        shift.z=-.08*surge
        q['spine']=rot(x=-7*surge);q['chest']=rot(x=-8*surge)
        q['head']=rot(x=8*surge)
        for side,sign in [('L',1),('R',-1)]:
            q['upperarm_'+side]=rot(x=-22*surge,y=-sign*5*surge)
            q['forearm_'+side]=rot(x=18*surge)
    if role=='land':
        settle=math.sin(math.pi*t)**2
        shift.z=-.07*settle;q['spine']=rot(x=-6*settle);q['head']=rot(x=3*settle)
    if role=='taunt':
        flare=envelope(t,[(0,0),(.25,.9),(.45,1),(.77,.9),(1,0)])
        q['chest']=rot(x=6*flare);q['head']=rot(x=7*flare,z=math.sin(phase)*3*flare)
        for side,sign in [('L',1),('R',-1)]:
            q['upperarm_'+side]=rot(y=-sign*45*flare,x=30*flare)
            q['forearm_'+side]=rot(x=20*flare,y=-sign*10*flare)
    # Secondary motion follows body movement with a lag; taunt opens the wings
    # without turning a folded sculpt into an invented fully extended wing mesh.
    for side,sign in [('L',1),('R',-1)]:
        amplitude=1.0 if role=='idle' else 2.5
        flare=envelope(t,[(0,0),(.25,1),(.77,.9),(1,0)]) if role=='taunt' else 0
        q['wingbase_'+side]=rot(y=sign*(math.sin(phase-.5)*amplitude+12*flare),z=sign*3*flare)
        q['wingspar_'+side]=rot(y=sign*(math.sin(phase-.9)*amplitude*.6+4*flare))
        for i in range(3):
            q['wingray%d_'%i+side]=rot(x=math.sin(phase-1-i*.25)*.7,y=sign*flare*(i+1))
        for kind in ['inner','outer']:
            for i in range(3):
                q['tendril_%s%d_%s'%(kind,i,side)]=rot(x=math.sin(phase-i*.65)*(.7 if role=='idle' else 1.4),z=sign*math.sin(phase-i*.8)*.7)
    for i in range(3):
        q['tail_%d'%i]=rot(z=math.sin(phase-i*.7)*(1 if role=='idle' else 2))
    world=study.fk(q,shift)
    for side in ['L','R']:
        if sole_corrections:
            feet[side].z+=sole_corrections[side]
        study.leg(world,side,feet[side])
    return world

roles={'idle':4.0,'walk':1.6,'run':.90,'melee_1':.72,'melee_2':.85,'melee_3':.95,'jump':1.05,'dash':.48,'taunt':3.2,'land':.38}
study.bake(roles,pose,stance_fractions={'walk':.65,'run':.55})
for role in ['walk','run']:
    study.actions[role]['CultUlhu.StanceFraction']=.65 if role=='walk' else .55
    study.actions[role]['CultUlhu.StanceTravelMetres']=.40 if role=='walk' else .60
study.save(center=(.06,.0,1.0),scale=2.8)
