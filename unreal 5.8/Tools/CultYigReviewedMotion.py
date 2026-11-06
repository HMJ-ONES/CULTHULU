"""Four-arm sword combinations and grounded serpent motion on Yig's measured rig.
No ability poses. Weapon transforms follow the actual corresponding hand delta.
"""
from pathlib import Path
import math,copy,runpy
BASE=runpy.run_path(str(Path(__file__).resolve().parent/'CultRigMotion.py'))
context=BASE['context'];world_poses=BASE['world_poses'];qmul=BASE['qmul'];qinv=BASE['qinv'];rotate=BASE['rotate'];axis_angle=BASE['axis_angle'];ease=BASE['ease']
def env(t,points):
    for (a,x),(b,y) in zip(points,points[1:]):
        if t<=b:return x+(y-x)*ease((t-a)/(b-a))
    return points[-1][1]
def sample(ctx,role,t):
    poses=copy.deepcopy(ctx['poses']);phase=math.tau*t
    upper_body=any(n.startswith('leftarm') for n in poses)
    hit=env(t,[(0,0),(.24,-.28),(.51,1),(.70,.7),(1,0)])
    load=env(t,[(0,0),(.24,1),(.51,0),(1,0)])
    pulse=math.sin(math.pi*t)**2
    for name,p in poses.items():
        side=-1 if 'left' in name else 1
        lower_arm='_001' in name
        shoulder='shoulder' in name
        upper=name.startswith(('leftarm','rightarm'))
        fore='forearm' in name
        arm=shoulder or upper or fore
        pitch=yaw=roll=0.
        if role=='idle':
            if name.startswith('spine') and upper_body:pitch=math.sin(phase)*.6
            elif arm:pitch=math.sin(phase-.4)*(.15 if shoulder else .45)
            elif name.startswith('tail'):
                index=int(''.join(c for c in name if c.isdigit()) or 1)
                yaw=math.sin(phase-index*.4)*.3
        elif role in ['walk','run']:
            fast=role=='run';strength=1.6 if fast else 1.
            if name.startswith('tail'):
                index=int(''.join(c for c in name if c.isdigit()) or 1)
                yaw=math.sin(phase-index*.43)*(2.8 if fast else 1.8)*(.25+.75*min(1,index/22))
            elif arm:
                pitch=math.sin(phase+side*math.pi*.5+(math.pi if lower_arm else 0))*(2 if shoulder else 5)*strength
                if fore:roll=side*math.cos(phase)*2*strength
            elif name.startswith('spine'):
                yaw=math.sin(phase)*.65*strength
        elif role.startswith('melee_'):
            kind=int(role[-1])
            if arm:
                active=(not lower_arm and ((kind==1 and side<0)or(kind==2 and side>0)))or(kind==3 and lower_arm)
                if active:
                    if kind==1:pitch=20*load+35*max(0,hit);yaw=side*(40*load-65*max(0,hit));roll=side*12*hit
                    elif kind==2:pitch=85*load+35*max(0,hit);yaw=side*(-12*load+32*hit);roll=side*8*hit
                    else:pitch=-20*load+65*max(0,hit);yaw=side*(-20*load+35*hit);roll=-side*15*hit
                    if shoulder:pitch*=.14;yaw*=.18;roll*=.2
                    if fore:pitch*=.55;yaw*=.3
                else:pitch=10*max(0,hit);yaw=side*6*max(0,hit)
            elif name.startswith('spine'):
                yaw=(7 if kind==1 else -7 if kind==2 else 0)*hit
                pitch=(-2 if kind==3 else 1)*hit
            elif name.startswith('tail'):
                index=int(''.join(c for c in name if c.isdigit()) or 1)
                yaw=math.sin(index*.25)*hit*.7
        elif role=='taunt':
            flare=env(t,[(0,0),(.25,.9),(.45,1),(.8,.9),(1,0)])
            if arm:
                pitch=(40 if not lower_arm else 18)*flare
                yaw=side*(35 if not lower_arm else 45)*flare
                roll=side*10*flare
                if shoulder:pitch*=.2;yaw*=.3;roll*=.3
            elif name.startswith('spine'):pitch=-2*flare
            elif name.startswith('tail'):yaw=math.sin(phase)*.7*flare
        elif role in ['jump','land','dash']:
            if arm:
                pitch=(20 if role=='jump' else -16 if role=='dash' else 8)*pulse
                if lower_arm:pitch*=.7
                if shoulder:pitch*=.2
            elif name.startswith('tail'):
                index=int(''.join(c for c in name if c.isdigit()) or 1)
                yaw=math.sin(index*.4+phase)*pulse*(2 if role=='dash' else 1)
            elif name.startswith('spine'):pitch=(-3 if role=='dash' else 2)*pulse
        inverse=qinv(ctx['world'][name]['rotation'])
        for axis,degrees in [([-1,0,0],pitch),([0,0,1],yaw),([0,1,0],roll)]:
            if degrees:p['rotation']=qmul(p['rotation'],axis_angle(rotate(inverse,axis),degrees))
    return poses

def drive_weapons(weapon,armor,role,t,mapping):
    poses=sample(weapon,role,t);body=sample(armor,role,t)
    current=world_poses(body,armor['parents']);parents=world_poses(poses,weapon['parents'])
    for bone,hand in mapping.items():
        if bone not in poses:continue
        ref=armor['world'][hand];now=current[hand];original=weapon['world'][bone]
        delta=qmul(now['rotation'],qinv(ref['rotation']))
        offset=rotate(delta,[original['location'][i]-ref['location'][i] for i in range(3)])
        target=[now['location'][i]+offset[i] for i in range(3)]
        rotation=qmul(delta,original['rotation']);parent=parents[weapon['parents'][bone]]
        inv=qinv(parent['rotation']);local=rotate(inv,[target[i]-parent['location'][i] for i in range(3)])
        poses[bone]['location']=[local[i]/parent['scale'][i] for i in range(3)]
        poses[bone]['rotation']=qmul(inv,rotation)
    return poses
