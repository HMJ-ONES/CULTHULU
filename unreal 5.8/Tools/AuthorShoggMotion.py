"""Heavy quadrupedal Shoggoths motion on the original UV-mapped static model.

Asymmetric curved forelimbs use measured paths and length-preserving IK.
Three physical combos, four contact regions, no ability animations.
"""
from pathlib import Path
import math
import runpy
import sys
import numpy as np
from mathutils import Matrix, Vector

PROJECT=Path(__file__).resolve().parents[1]
helper=runpy.run_path(str(PROJECT/'Tools/CultMotionStudy.py'))
Study,rot,envelope,smooth=[helper[n] for n in ['Study','rot','envelope','smooth']]
revision=sys.argv[sys.argv.index('--')+1] if '--' in sys.argv else 'v001'
study=Study('shoggoths','Shoggoths',revision,(.36,.42,.34,1),
    source_relative='unreal 5.8/Saved/CharacterSources/shoggoths/source.glb',weld_seams=True)
points=np.array([tuple(v.co) for v in study.mesh.data.vertices])
# Below the trunk, the actual welded surface separates into four independent
# limbs. Use that topology to keep the curled paw and nearby hind leg distinct.
edges=np.array([tuple(e.vertices) for e in study.mesh.data.edges])
lower=points[:,2]<.65;adjacency=[[] for _ in points]
for a,b in edges:
    if lower[a] and lower[b]:adjacency[a].append(int(b));adjacency[b].append(int(a))
visited=set();components=[]
for start in np.where(lower)[0]:
    if int(start) in visited:continue
    visited.add(int(start));queue=[int(start)];component=[]
    while queue:
        index=queue.pop();component.append(index)
        for other in adjacency[index]:
            if other not in visited:visited.add(other);queue.append(other)
    if len(component)>100:components.append(component)
assert len(components)==4,'Review changed source anatomy before rigging'
components.sort(key=lambda c:float(points[c,0].mean()))
region_components=dict(zip(['arm_R','leg_R','leg_L','arm_L'],components))
component_labels=np.full(len(points),-1,dtype=np.int32)
region_ids={}
for i,(name,component) in enumerate(region_components.items()):
    component_labels[component]=i;region_ids[name]=i
study.source_art['lowerAnatomicalRegions']=[{'region':name,'vertices':len(c),'centroid':points[c].mean(axis=0).tolist()} for name,c in region_components.items()]
bone=study.bone
bone('root',(0,0,0),(0,0,.16),deform=False)
bone('pelvis',(.20,-.24,.68),(.25,-.18,.87),'root')
bone('spine',(.25,-.18,.87),(.28,-.13,1.08),'pelvis')
bone('chest',(.28,-.13,1.08),(.29,-.08,1.32),'spine')
bone('neck',(.29,-.08,1.32),(.30,-.04,1.48),'chest')
bone('head',(.30,-.04,1.48),(.33,.01,1.73),'neck')
arms={'L':[(.58,-.08,1.20),(.86,-.18,.85),(1.00,-.31,.39),(.79,-.43,.18)],
      'R':[(-.12,-.14,1.18),(-.58,-.18,.86),(-.90,.01,.40),(-.84,.39,.14)]}
groups={'body':['pelvis','spine','chest','neck'],'head':['head']}
seeds={'body':[(.2,-.25,.78),(.3,-.25,1.0),(.3,.03,1.13),(.4,.02,1.33)],
       'head':[(.33,.0,1.57),(.34,.02,1.73)]}
arm_names={}
finger_names={}
for side,path in arms.items():
    names=[]
    # Six shorter segments spread bending through the thick curved limb instead
    # of folding all of its mass around one conventional elbow.
    subdivided=[]
    for a,b in zip(path,path[1:]):
        subdivided.extend([Vector(a),Vector(a).lerp(Vector(b),.5)])
    subdivided.append(Vector(path[-1]))
    prefixes=['upperarm','upperarm_roll','arm_mid','arm_mid_roll','forearm','forearm_roll']
    for prefix,(a,b) in zip(prefixes,zip(subdivided,subdivided[1:])):
        name=prefix+'_'+side;bone(name,a,b,'chest' if not names else names[-1]);names.append(name)
    arm_names[side]=names
    wrist=Vector(path[-1])
    hand_end=(.72,-.47,.23) if side=='L' else (-.83,.62,.13)
    bone('hand_'+side,wrist,hand_end,names[-1])
    fingers=[]
    if side=='L':
        tips=[(.60,-.47,.43),(.72,-.50,.45),(.84,-.49,.39)]
    else:
        tips=[(-1.15,.88,.06),(-.88,.98,.07),(-.56,.80,.07)]
    for i,tip in enumerate(tips):
        a=Vector(hand_end);b=a.lerp(Vector(tip),.55);c=Vector(tip)
        first='finger%d_a_%s'%(i,side);second='finger%d_b_%s'%(i,side)
        bone(first,a,b,'hand_'+side);bone(second,b,c,first);fingers.extend([first,second])
    finger_names[side]=fingers
    groups['arm_'+side]=names+['hand_'+side]+fingers
    seeds['arm_'+side]=path[1:]+[hand_end]+tips
    hip,knee,ankle=((.28,-.58,.68),(.47,-.85,.36),(.45,-.75,.11)) if side=='L' else ((-.08,-.51,.68),(-.15,-.61,.34),(-.12,-.54,.11))
    bone('thigh_'+side,hip,knee,'pelvis');bone('shin_'+side,knee,ankle,'thigh_'+side)
    bone('foot_'+side,ankle,Vector(ankle)+Vector((0,.07,-.035)),'shin_'+side)
    groups['leg_'+side]=[p+'_'+side for p in ['thigh','shin','foot']]
    seeds['leg_'+side]=[knee,ankle]
horns={}
for side,path in {'L':[(.50,-.30,1.40),(.60,-.35,1.70),(.70,-.30,1.97)],
                  'R':[(.10,-.30,1.40),(.00,-.30,1.70),(-.10,-.20,1.97)]}.items():
    names=[]
    for i,(a,b) in enumerate(zip(path,path[1:])):
        name='horn%d_'%i+side;bone(name,a,b,'chest' if not names else names[-1]);names.append(name)
    horns[side]=names;groups['horn_'+side]=names;seeds['horn_'+side]=path[1:]
    name='rear_loop_'+side
    x=.55 if side=='L' else .05
    bone(name,(x,-.31,1.4),(x,-.78,1.51),'chest')
    groups['loop_'+side]=[name];seeds['loop_'+side]=[(x,-.70,1.5)]
x,y,z=points.T
def ramp(v,a,b):
    t=np.clip((v-a)/(b-a),0,1);return t*t*(3-2*t)
masks={'head':ramp(z,1.37,1.52),'body':(component_labels<0).astype(float)}
for side,sign in [('L',1),('R',-1)]:
    masks['leg_'+side]=1-ramp(z,.70,.86)
    masks['arm_'+side]=1-ramp(z,1.40,1.58)
    masks['horn_'+side]=ramp(z,1.37,1.52)
    masks['loop_'+side]=ramp(z,1.22,1.42)*(1-ramp(y,-.25,-.10))
for name,identifier in region_ids.items():
    masks[name]*=np.where(component_labels>=0,(component_labels==identifier).astype(float),1.)
study.bind(groups,seeds,masks,region_proximity=.20)
assert len(study.mesh.data.uv_layers)>=1 and len(study.mesh.data.materials)==4,'Retain original UVs and four material slots'

offsets={'hand_L':0.,'hand_R':.5,'foot_L':.5,'foot_R':0.}
anchors={n:study.heads[n].copy() for n in offsets}
low=points[:,2]<.30
xy=np.array([tuple(anchors[n])[:2] for n in offsets])
assignment=np.linalg.norm(points[:,None,:2]-xy[None,:,:],axis=2).argmin(axis=1)
def planted(name,role,t):
    if role in ['walk','run']:return (t+offsets[name])%1<(.70 if role=='walk' else .54)
    if role=='jump':return t<.28
    if role=='dash':return t<.18 or t>.85
    if name=='hand_L' and role=='melee_1':return t<.10 or t>.85
    if name=='hand_R' and role=='melee_2':return t<.10 or t>.85
    if name.startswith('hand') and role=='taunt':return t<.12 or t>.88
    return True
contacts={}
for i,name in enumerate(offsets):
    region=('arm_' if name.startswith('hand') else 'leg_')+name[-1]
    indices=np.where(component_labels==region_ids[region])[0];assert len(indices)>10,(name,len(indices))
    contacts[name]={'indices':indices,'planted':lambda role,t,name=name:planted(name,role,t)}
study.rig['CultUlhu.ContactRegionVertexCounts']=str({n:len(c['indices']) for n,c in contacts.items()})

def pose(role,t,corrections=None):
    phase=math.tau*t
    q={};shift=Vector((0,0,-.05))
    targets={n:p.copy() for n,p in anchors.items()}
    if role=='idle':
        q['chest']=rot(x=math.sin(phase)*.8)
        q['head']=rot(x=-math.sin(phase)*.4)
    if role in ['walk','run']:
        fast=role=='run';duty=.54 if fast else .70;stride=.58 if fast else .36
        shift.x=math.sin(phase)*(.028 if fast else .018)
        shift.z-=.025 if fast else .005
        shift.z+=math.cos(phase*2)*(.018 if fast else .008)
        q['spine']=rot(x=-5 if fast else -2,z=math.sin(phase)*1.8)
        q['chest']=rot(z=-math.sin(phase)*2)
        q['head']=rot(x=3 if fast else 1)
        for name in targets:
            u=(t+offsets[name])%1
            if u<duty:targets[name].y+=stride*(.5-u/duty)
            else:
                f=(u-duty)/(1-duty)
                targets[name].y+=stride*(-.5+smooth(f))
                targets[name].z+=(.17 if fast else .09)*math.sin(math.pi*f)**2
    if role.startswith('melee_'):
        hit=envelope(t,[(0,0),(.26,-.2),(.49,1),(.70,.65),(1,0)])
        strike=max(0,hit)
        if role=='melee_1':
            targets['hand_L']+=Vector((-.36*hit,1.02*hit,.38*strike))
            q['chest']=rot(z=9*hit);q['head']=rot(z=-3*hit)
        elif role=='melee_2':
            lift=envelope(t,[(0,0),(.27,1),(.49,.12),(.70,0),(1,0)])
            targets['hand_R']+=Vector((.18*strike,.25*strike,1.03*lift))
            q['chest']=rot(z=-6*hit,x=3*lift)
        else:
            # A three-mouthed body shove, braced on both immense forepaws.
            shift.y+=.14*strike
            shift.z-=.025*strike
            q['spine']=rot(x=-7*hit);q['chest']=rot(x=-8*hit)
            q['head']=rot(x=5*hit)
    if role=='jump':
        load=envelope(t,[(0,0),(.22,1),(.43,0),(1,0)])
        air=envelope(t,[(0,0),(.28,0),(.52,1),(.80,.8),(1,.5)])
        shift.z-=.09*load;q['spine']=rot(x=-5*load);q['head']=rot(x=6*air)
        for name in targets:targets[name].z+=(.17 if name.startswith('hand') else .20)*air
    if role=='dash':
        pulse=math.sin(math.pi*t)**2
        shift.z-=.075*pulse;q['spine']=rot(x=-8*pulse);q['head']=rot(x=7*pulse)
        for name in targets:
            targets[name].y+=.16*pulse;targets[name].z+=.07*pulse
    if role=='land':
        settle=math.sin(math.pi*t)**2
        shift.z-=.08*settle;q['chest']=rot(x=-4*settle)
    if role=='taunt':
        flare=envelope(t,[(0,0),(.25,.8),(.45,1),(.76,.85),(1,0)])
        shift.y-=.04*flare
        q['spine']=rot(x=5*flare);q['chest']=rot(x=6*flare)
        q['head']=rot(x=10*flare,z=math.sin(phase)*3*flare)
        for side,sign in [('L',1),('R',-1)]:
            targets['hand_'+side]+=Vector((sign*.10*flare,(1.0 if side=='L' else .18)*flare,(.68 if side=='L' else .78)*flare))
    for side,sign in [('L',1),('R',-1)]:
        for i,name in enumerate(horns[side]):q[name]=rot(y=math.sin(phase-i*.7)*.55*sign)
        q['rear_loop_'+side]=rot(z=math.sin(phase-.7)*.9*sign)
        for i,name in enumerate(finger_names[side]):
            q[name]=rot(x=(3*math.sin(math.pi*t) if role.startswith('melee') else .6*math.sin(phase-i*.25)))
    world=study.fk(q,shift)
    for name in targets:
        if corrections:targets[name].z+=corrections[name]
    for side in ['L','R']:
        endpoint=study.chain(world,arm_names[side],targets['hand_'+side])
        rest_hand=study.rest['hand_'+side].to_quaternion()
        last=arm_names[side][-1]
        follow=world[last].to_quaternion()@study.rest[last].to_quaternion().inverted()@rest_hand
        release=0.
        if role in ['walk','run']:
            duty=.70 if role=='walk' else .54
            u=(t+offsets['hand_'+side])%1
            if u>duty:release=.7*math.sin(math.pi*(u-duty)/(1-duty))**2
        elif (role=='melee_1' and side=='L') or (role=='melee_2' and side=='R'):
            release=envelope(t,[(0,0),(.18,1),(.75,1),(1,0)])
        elif role=='taunt':release=envelope(t,[(0,0),(.25,.9),(.75,1),(1,0)])
        elif role=='jump':release=.8*envelope(t,[(0,0),(.28,0),(.55,1),(1,.5)])
        elif role=='dash':release=.6*math.sin(math.pi*t)**2
        world['hand_'+side]=Matrix.Translation(endpoint)@rest_hand.slerp(follow,release).to_matrix().to_4x4()
        for name in finger_names[side]:
            parent=study.parents[name];inherited=world[parent]@study.rest[parent].inverted()@study.rest[name]
            world[name]=Matrix.Translation(inherited.translation)@(q[name]@inherited.to_quaternion()).to_matrix().to_4x4()
        study.leg(world,side,targets['foot_'+side])
    return world

roles={'idle':3.8,'walk':1.6,'run':1.0,'melee_1':.78,'melee_2':.90,'melee_3':.85,'jump':1.1,'dash':.52,'taunt':2.8,'land':.4}
study.bake(roles,pose,contact_setup=contacts)
for role in ['walk','run']:
    study.actions[role]['CultUlhu.StanceFraction']=.70 if role=='walk' else .54
    study.actions[role]['CultUlhu.StanceTravelMetres']=.36 if role=='walk' else .58
study.save(center=(.12,0,1.0),scale=3.2)
