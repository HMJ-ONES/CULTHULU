"""Original rig-aware motion recipes. Pure Python: deterministic, inspectable, no UE dependency.
Rotations use measured joint frames and anatomical world axes, never arbitrary local Euler axes.
Mi-Go locomotion remains its supplied Prowl; generated poses preserve planted idle feet.
Yig locomotion distributes a small horizontal wave along its coil, preserving ground height.
"""
import math,copy

def qmul(a,b):
 x,y,z,w=a;X,Y,Z,W=b
 return [w*X+x*W+y*Z-z*Y,w*Y-x*Z+y*W+z*X,w*Z+x*Y-y*X+z*W,w*W-x*X-y*Y-z*Z]
def qinv(q): return [-q[0],-q[1],-q[2],q[3]]
def rotate(q,v): return qmul(qmul(q,[*v,0]),qinv(q))[:3]
def axis_angle(axis,degrees):
 length=math.sqrt(sum(x*x for x in axis)); h=math.radians(degrees)*.5
 return [x/max(length,1e-12)*math.sin(h) for x in axis]+[math.cos(h)]
def ease(t):
 t=max(0.,min(1.,t)); return t*t*t*(t*(t*6-15)+10)
def strike(t):
 if t<.22: return -.3*ease(t/.22)
 if t<.5: return -.3+1.3*ease((t-.22)/.28)
 if t<.68: return 1.-.35*ease((t-.5)/.18)
 return .65*(1.-ease((t-.68)/.32))
def context(row):
 poses={n.lower():copy.deepcopy(p) for n,p in row['poses'].items()}
 parents={n.lower():(path[1].lower() if len(path)>1 else None) for n,path in row['paths'].items()}
 return {'id':row['id'],'poses':poses,'parents':parents,'world':world_poses(poses,parents)}
def world_poses(poses,parents):
 result={}
 def find(n):
  if n in result: return result[n]
  p=poses[n]; parent=parents.get(n)
  if parent and parent in poses:
   a=find(parent); scaled=[p['location'][i]*a['scale'][i] for i in range(3)]; offset=rotate(a['rotation'],scaled)
   value={'location':[a['location'][i]+offset[i] for i in range(3)],'rotation':qmul(a['rotation'],p['rotation']),'scale':[a['scale'][i]*p['scale'][i] for i in range(3)]}
  else: value=copy.deepcopy(p)
  result[n]=value; return value
 for n in poses: find(n)
 return result

def sample(ctx,role,t):
 t=max(0.,min(1.,t)); poses=copy.deepcopy(ctx['poses']); phase=2*math.pi*t
 attack=strike(t); pulse=math.sin(math.pi*t)**2
 air=ease(t/.35)*(.8+.2*math.sin(math.pi*t))
 for name,p in poses.items():
  lower=name; pitch=yaw=roll=0.
  secondary=('mini' in lower or '_001' in lower); side=-1 if ('left' in lower or lower.endswith('_l')) else 1
  shoulder='shoulder' in lower; upper='bicep' in lower or lower.startswith(('leftarm','rightarm')); fore='forearm' in lower
  arm=shoulder or upper or fore
  if role in ['walk','run']:
   strength=1.25 if role=='run' else 1.
   if ctx['id']=='yig':
    if lower.startswith('tail'):
     digits=''.join(c for c in lower if c.isdigit()); index=int(digits or 1)
     yaw=math.sin(phase-index*.48)*1.4*strength*(.65+.35*index/16)
    elif lower.startswith('spine_'): yaw=math.sin(phase-int(lower[-3:])*.4)*.35*strength
    elif arm: pitch=math.sin(phase+side*math.pi*.5)*(2.0 if shoulder else 4.)*strength
   elif arm: pitch=math.sin(phase)*side*3.
  elif role=='idle':
   if lower in ['spine','torso','neck']: pitch=math.sin(phase)*.8
   if 'wingbase' in lower: roll=math.sin(phase)*side*2.
   if lower in ['jaw_l','jaw_r']: yaw=math.sin(phase)*side*.8
  elif role=='attack':
   if arm:
    amplitude=5. if shoulder else 25. if upper else 20.
    if secondary: amplitude*=.55
    if ctx['id']=='yig' and side<0: amplitude*=.45
    pitch=attack*amplitude
   if lower in ['torso','spine','spine1']: yaw=attack*3.
   if lower.startswith('tail'): yaw=attack*.3
   if lower in ['jaw_l','jaw_r']: yaw=side*max(0.,attack)*5.
  elif role=='cast':
   if arm: pitch=-pulse*(4. if shoulder else 15. if upper else 18.)*(.65 if secondary else 1.)
   if 'wingbase' in lower: roll=side*pulse*12.
   if lower in ['torso','spine','spine1']: pitch=-pulse*2.
  elif role=='jump':
   if 'thigh' in lower: pitch=air*24.
   if lower.startswith('shin_'): pitch=-air*34.
   if arm: pitch=air*(4. if shoulder else 12.)
   if 'wingbase' in lower: roll=side*air*22.
   if lower.startswith('tail'): yaw=air*.3
  elif role=='dash':
   if lower in ['torso','spine','spine1']: pitch=pulse*3.
   if arm: pitch=pulse*(3. if shoulder else 9.)
   if 'wingbase' in lower: roll=-side*pulse*7.
  elif role=='land':
   if 'thigh' in lower: pitch=pulse*12.
   if lower.startswith('shin_'): pitch=-pulse*18.
   if lower in ['torso','spine','spine1']: pitch=pulse*2.
  # Convert the intended anatomical axes into this joint's measured local frame.
  inverse=qinv(ctx['world'][name]['rotation'])
  for axis,degrees in [([-1,0,0],pitch),([0,0,1],yaw),([0,1,0],roll)]:
   if degrees: p['rotation']=qmul(p['rotation'],axis_angle(rotate(inverse,axis),degrees))
 return poses

def drive_weapons(weapon,armor,role,t,mapping):
 poses=sample(weapon,role,t); body=sample(armor,role,t); current=world_poses(body,armor['parents']); parents=world_poses(poses,weapon['parents'])
 for bone,hand in mapping.items():
  ref=armor['world'][hand]; now=current[hand]; original=weapon['world'][bone]
  # Rigid source sword vertices are baked in model coordinates; the bone pivot is at zero.
  # Apply the hand's full delta around its actual reference position, not around that zero pivot.
  delta=qmul(now['rotation'],qinv(ref['rotation'])); offset=rotate(delta,[original['location'][i]-ref['location'][i] for i in range(3)])
  target=[now['location'][i]+offset[i] for i in range(3)]; target_rotation=qmul(delta,original['rotation'])
  parent=parents[weapon['parents'][bone]]; inv=qinv(parent['rotation']); local=rotate(inv,[target[i]-parent['location'][i] for i in range(3)])
  poses[bone]['location']=[local[i]/parent['scale'][i] for i in range(3)]
  poses[bone]['rotation']=qmul(inv,target_rotation)
 return poses
