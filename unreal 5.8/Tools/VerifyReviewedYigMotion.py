"""Check Yig's real measured rig: four hand grips, loop closure, grounded tail."""
from pathlib import Path
import json,runpy,math
PROJECT=Path(__file__).resolve().parents[1]
recipe=runpy.run_path(str(PROJECT/'Tools/CultYigReviewedMotion.py'))
rows=[r for r in json.loads((PROJECT/'Saved/CombatTactics20261030/PresentationAudit.json').read_text()) if r['id']=='yig']
contexts={r['clip']:recipe['context'](r) for r in rows}
armor=next(contexts[r['clip']] for r in rows if 'Armor_' in r['clip'])
weapons=next(contexts[r['clip']] for r in rows if 'Sword_' in r['clip'])
tail=next(contexts[r['clip']] for r in rows if 'Tail_' in r['clip'])
mapping=json.loads((PROJECT/'Config/ApprovedRigMotion.json').read_text())['weapon_hands']
assert len(mapping)==4 and all(b in weapons['poses'] and h in armor['poses'] for b,h in mapping.items())
fk=recipe['world_poses'];sample=recipe['sample'];rotate=recipe['rotate'];inv=recipe['qinv']
distance=lambda a,b:math.sqrt(sum((x-y)**2 for x,y in zip(a,b)))
maximum_grip_error=0.;maximum_floor_error=0.;loop_error=0.;trajectories={}
for role in ['idle','walk','run','melee_1','melee_2','melee_3','jump','dash','taunt','land']:
    trajectory=[]
    for i in range(61):
        t=i/60
        body=fk(sample(armor,role,t),armor['parents'])
        armed=fk(recipe['drive_weapons'](weapons,armor,role,t,mapping),weapons['parents'])
        for bone,hand in mapping.items():
            ref_hand=armor['world'][hand];ref_weapon=weapons['world'][bone]
            old_relative=rotate(inv(ref_hand['rotation']),[ref_weapon['location'][j]-ref_hand['location'][j] for j in range(3)])
            new_relative=rotate(inv(body[hand]['rotation']),[armed[bone]['location'][j]-body[hand]['location'][j] for j in range(3)])
            error=distance(old_relative,new_relative);maximum_grip_error=max(maximum_grip_error,error)
            assert error<.001,(role,t,bone,error)
            trajectory.extend(body[hand]['location'])
        if role in ['idle','walk','run']:
            serpent=fk(sample(tail,role,t),tail['parents'])
            for name,p in serpent.items():
                error=abs(p['location'][2]-tail['world'][name]['location'][2]);maximum_floor_error=max(maximum_floor_error,error)
                assert error<.001,(role,t,name,error)
    if role.startswith('melee_'):trajectories[role]=trajectory
    if role in ['idle','walk','run']:
        for ctx in [armor,tail,weapons]:
            a=fk(sample(ctx,role,0),ctx['parents']);b=fk(sample(ctx,role,1),ctx['parents'])
            for name in a:loop_error=max(loop_error,distance(a[name]['location'],b[name]['location']))
            assert loop_error<.001
separation={}
for a,b in [('melee_1','melee_2'),('melee_1','melee_3'),('melee_2','melee_3')]:
    value=math.sqrt(sum((x-y)**2 for x,y in zip(trajectories[a],trajectories[b]))/len(trajectories[a]))
    assert value>.1,(a,b,value);separation[a+'/'+b]=value
report={'passed':True,'fourWeaponGripsMaxErrorSourceUnits':maximum_grip_error,
        'tailGroundHeightMaxErrorSourceUnits':maximum_floor_error,'loopClosureMaxErrorSourceUnits':loop_error,
        'comboTrajectoryRmsSeparationSourceUnits':separation,'scope':'measured skeletal transforms; native rendered and artistic review still required'}
revision=json.loads((PROJECT/'Config/ReviewedCharacterMotion.json').read_text())['characters']['yig']['revision']
output=PROJECT/f'Saved/MotionIntegration/yig_{revision}/recipe_verification.json';output.parent.mkdir(parents=True,exist_ok=True);output.write_text(json.dumps(report,indent=2)+'\n')
print(json.dumps(report,indent=2))
