"""Numerical anatomy/contact/rigid-weapon checks, runnable without Unreal."""
from pathlib import Path
import json,runpy,math
P=Path(__file__).resolve().parents[1]
M=runpy.run_path(str(P/'Tools/CultRigMotion.py')); data=json.loads((P/'Config/ApprovedRigMotion.json').read_text(encoding='utf-8'))
contexts=[M['context']({'id':r['id'],'poses':r['reference_pose'],'paths':r['paths']}) for r in data['rigs']]
roles=['idle','walk','run','jump','attack','cast','dash','land']; checks=0; max_angle=0.; max_contact=0.; max_weapon=0.
for ctx in contexts:
 for role in roles:
  for i in range(31):
   pose=M['sample'](ctx,role,i/30)
   for name,value in pose.items():
    assert abs(sum(x*x for x in value['rotation'])-1)<1e-5,(name,role)
    a=ctx['poses'][name]['rotation']; b=value['rotation']; dot=min(1,abs(sum(x*y for x,y in zip(a,b))))
    angle=math.degrees(2*math.acos(dot)); max_angle=max(max_angle,angle); assert angle<36,(name,role,angle)
   if ctx['id']=='mi_go' and role=='idle':
    world=M['world_poses'](pose,ctx['parents'])
    for n in ['foot_l','foot_r']:
     error=math.dist(world[n]['location'],ctx['world'][n]['location']); max_contact=max(max_contact,error); assert error<1e-6
   if ctx['id']=='yig' and 'tail01' in pose and role in ['walk','run']:
    world=M['world_poses'](pose,ctx['parents'])
    for n in world:
     error=abs(world[n]['location'][2]-ctx['world'][n]['location'][2]); max_contact=max(max_contact,error); assert error<1e-5,(n,error)
   checks+=1
  if role in ['idle','walk','run','attack','cast','dash','land']:
   first=M['sample'](ctx,role,0); last=M['sample'](ctx,role,1)
   for n in first:
    dot=abs(sum(x*y for x,y in zip(first[n]['rotation'],last[n]['rotation']))); assert abs(1-dot)<1e-5,(n,role)
armor=contexts[1]; weapon=contexts[2]
def point(frame,local):
 return [frame['location'][i]+M['rotate'](frame['rotation'],[local[k]*frame['scale'][k] for k in range(3)])[i] for i in range(3)]
def inverse(frame,world):
 delta=M['rotate'](M['qinv'](frame['rotation']),[world[i]-frame['location'][i] for i in range(3)])
 return [delta[i]/frame['scale'][i] for i in range(3)]
for role in roles:
 for t in [.1,.3,.5,.8]:
  driven=M['drive_weapons'](weapon,armor,role,t,data['weapon_hands']); world=M['world_poses'](driven,weapon['parents']); hands=M['world_poses'](M['sample'](armor,role,t),armor['parents'])
  for bone,hand in data['weapon_hands'].items():
   vertex=data['weapon_geometry'][bone]; actual=point(world[bone],inverse(weapon['world'][bone],vertex)); expected=point(hands[hand],inverse(armor['world'][hand],vertex)); error=math.dist(actual,expected); max_weapon=max(max_weapon,error); assert error<1e-4,(bone,role,error)
  checks+=1
result={'samples_checked':checks,'max_authored_joint_delta_degrees':max_angle,'max_planted_contact_drift_source_units':max_contact,'max_rigid_weapon_follow_error_source_units':max_weapon,'passed':True}
output=P/'Saved/CombatTactics20261030/RigMotionChecks.json'; output.parent.mkdir(parents=True,exist_ok=True); output.write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8'); print(json.dumps(result))
