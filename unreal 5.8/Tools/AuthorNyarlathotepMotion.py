"""Tripod stalking gait and original claw/head-whip studies on reviewed retopology.

Background Blender only; immutable source and fresh versioned working copies.
No ability animations. +Y forward, two front legs and one rear support leg.
"""
from pathlib import Path
import hashlib
import json
import math
import runpy
import sys
import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector

assert bpy.app.background
PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT.parent
revision = sys.argv[sys.argv.index('--')+1] if '--' in sys.argv else 'v001'
assert revision.startswith('v') and revision[1:].isdigit()
dest = ROOT / 'assets/character_animation/nyarlathotep' / revision
dest.mkdir(parents=True, exist_ok=True)
output = dest / 'Nyarlathotep_BasicMotion.blend'
assert not output.exists(), 'Preserve artist revisions'
source = ROOT / 'assets/retopology/nyarlathotep_retopology.obj'
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
source_hash = sha(source)
anatomy = json.loads((PROJECT/'Config/CharacterAnatomicalFrames.json').read_text())['nyarlathotep']
assert anatomy['sourceSha256'] == source_hash
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.wm.obj_import(filepath=str(source), forward_axis='Y', up_axis='Z')
mesh = next(o for o in bpy.context.scene.objects if o.type == 'MESH')
mesh.name = 'Nyarlathotep_Tripod_Retopology'
rotation = Matrix.Rotation(math.radians(anatomy['yawDegrees']), 4, 'Z')
for v in mesh.data.vertices:
    v.co = ((rotation @ (mesh.matrix_world @ v.co))-Vector(anatomy['originAfterRotation']))*anatomy['scale']
mesh.matrix_world = Matrix.Identity(4)
for f in mesh.data.polygons:
    f.use_smooth = True
mesh.color = (.36, .30, .45, 1)
data = bpy.data.armatures.new('Nyarlathotep_Tripod_Skeleton')
rig = bpy.data.objects.new('Nyarlathotep_AnimationRig', data)
bpy.context.scene.collection.objects.link(rig)
bpy.context.view_layer.objects.active = rig
mesh.select_set(False)
rig.select_set(True)
bpy.ops.object.mode_set(mode='EDIT')
def bone(name, a, b, parent=None, deform=True):
    item = data.edit_bones.new(name)
    item.head, item.tail, item.use_deform = a, b, deform
    if parent:
        item.parent = data.edit_bones[parent]
bone('root', (0,.8,0), (0,.8,.15), deform=False)
bone('pelvis', (0,.8,.87), (0,.71,1.05), 'root')
bone('spine', (0,.71,1.05), (0,.59,1.25), 'pelvis')
bone('chest', (0,.59,1.25), (0,.43,1.42), 'spine')
head = [(0,.43,1.42),(0,.28,1.64),(0,.13,1.86),(0,-.25,1.93),(0,-.65,1.85),(0,-1.10,1.97)]
for i, (a,b) in enumerate(zip(head,head[1:])):
    bone('head_%02d'%i, a,b,'chest' if i==0 else 'head_%02d'%(i-1))
for side, sign in [('L',1),('R',-1)]:
    bone('upperarm_'+side,(sign*.18,.53,1.26),(sign*.32,.30,.98),'chest')
    bone('forearm_'+side,(sign*.32,.30,.98),(sign*.44,.12,.74),'upperarm_'+side)
    bone('hand_'+side,(sign*.44,.12,.74),(sign*.46,.10,.66),'forearm_'+side)
    for i,x in enumerate([.40,.46,.51]):
        bone('claw%d_'%i+side,(sign*x,.10,.69),(sign*(x+.01),.12,.59),'hand_'+side)
legs = {'L':[(.22,.85,.93),(.29,1.00,.46),(.30,1.03,.065)],
        'R':[(-.22,.85,.93),(-.29,1.00,.46),(-.30,1.03,.065)],
        'rear':[(0,.71,.91),(0,.56,.43),(0,.42,.065)]}
for name, (a,b,c) in legs.items():
    bone('thigh_'+name,a,b,'pelvis')
    bone('shin_'+name,b,c,'thigh_'+name)
    bone('foot_'+name,c,Vector(c)+Vector((0,.09,-.025)),'shin_'+name)
bpy.ops.object.mode_set(mode='OBJECT')
mesh.parent = rig
modifier = mesh.modifiers.new('Anatomical skinning','ARMATURE')
modifier.object = rig
# Linear skinning matches the runtime default and makes pinching visible here.
modifier.use_deform_preserve_volume = False
groups = {'body':['pelvis','spine','chest'], 'head':['head_%02d'%i for i in range(5)]}
seeds = {'body':[(0,.79,1.0),(0,.65,1.21)], 'head':head[1:]}
for side,sign in [('L',1),('R',-1)]:
    groups['arm_'+side] = [b.name for b in data.bones if b.name.endswith('_'+side) and b.name.startswith(('upperarm','forearm','hand','claw'))]
    seeds['arm_'+side] = [(sign*.27,.40,1.09),(sign*.40,.18,.81),(sign*.46,.1,.65)]
for name, (a,b,c) in legs.items():
    groups['leg_'+name] = [n+'_'+name for n in ['thigh','shin','foot']]
    seeds['leg_'+name] = [(Vector(a)+Vector(b))*.5,b,c]
skin = runpy.run_path(str(PROJECT/'Tools/CultSkinWeights.py'))['bind'](mesh,rig,groups,seeds)
rest = {b.name:b.matrix_local.copy() for b in data.bones}
parents = {b.name:b.parent.name if b.parent else None for b in data.bones}
heads = {b.name:b.head_local.copy() for b in data.bones}
tails = {b.name:b.tail_local.copy() for b in data.bones}
def smooth(t):
    t=max(0,min(1,t)); return t*t*t*(10+t*(-15+6*t))
def envelope(t, points):
    for (a,x),(b,y) in zip(points,points[1:]):
        if t<=b:
            return x+(y-x)*smooth((t-a)/(b-a))
    return points[-1][1]
def rot(x=0,y=0,z=0):
    return Quaternion((0,0,1),math.radians(z)) @ Quaternion((0,1,0),math.radians(y)) @ Quaternion((1,0,0),math.radians(x))
def aim(name,a,b):
    q=(tails[name]-heads[name]).normalized().rotation_difference((b-a).normalized())
    return Matrix.Translation(a) @ (q@rest[name].to_quaternion()).to_matrix().to_4x4()
def pose(role,t):
    phase=math.tau*t
    q={n:Quaternion() for n in rest}
    shift=Vector((0,0,0))
    feet={n:Vector(v[2]) for n,v in legs.items()}
    if role in ['walk','run']:
        fast=role=='run'; duty=.64 if not fast else .56
        shift.z=-.018+.012*math.cos(3*phase)
        q['chest']=rot(z=-math.sin(phase)*2,x=-3 if fast else -1)
        for i,n in enumerate(legs):
            u=(t+i/3)%1
            if u<duty:
                feet[n].y+=(.28 if fast else .18)*(.5-u/duty)
            else:
                f=(u-duty)/(1-duty)
                feet[n].y+=(.28 if fast else .18)*(-.5+smooth(f))
                feet[n].z+=(.13 if fast else .065)*math.sin(math.pi*f)**2
    if role.startswith('melee_'):
        number=int(role[-1])
        hit=envelope(t,[(0,0),(.24,-.22),(.48,1),(.7,.65),(1,0)])
        if number<3:
            side='L' if number==1 else 'R'; sign=1 if number==1 else -1
            q['chest']=rot(z=sign*12*hit)
            q['upperarm_'+side]=rot(x=-48*hit,z=-sign*30*hit)
            q['forearm_'+side]=rot(x=25*hit,y=sign*15*hit)
            q['hand_'+side]=rot(y=-sign*18*hit)
        else:
            # A bodily head-whip gives the third strike its own silhouette.
            q['spine']=rot(x=-8*hit)
            for i in range(5):
                q['head_%02d'%i]=rot(x=hit*(14 if i<2 else -18),z=hit*(20 if i<2 else 12))
            for side,sign in [('L',1),('R',-1)]:
                q['upperarm_'+side]=rot(x=-12*hit,z=sign*12*hit)
    if role in ['jump','dash','land']:
        load=math.sin(math.pi*t)**2
        shift.z=-.08*load
        q['spine']=rot(x=-8*load)
        if role=='jump':
            air=envelope(t,[(0,0),(.27,0),(.55,1),(1,.5)])
            for n in feet:
                feet[n].z+=.14*air
        if role=='dash':
            q['chest']=rot(x=-12*load)
            for i in range(5):
                q['head_%02d'%i]=rot(x=-3*load)
    if role=='taunt':
        flare=envelope(t,[(0,0),(.23,1),(.73,.9),(1,0)])
        q['chest']=rot(x=5*flare)
        for side,sign in [('L',1),('R',-1)]:
            q['upperarm_'+side]=rot(z=sign*30*flare,x=-20*flare)
            q['forearm_'+side]=rot(x=-20*flare)
        for i in range(5):
            q['head_%02d'%i]=rot(z=math.sin(phase-i*.5)*6*flare,x=-3*flare)
    for i in range(5):
        q['head_%02d'%i] = q['head_%02d'%i] @ rot(z=math.sin(phase-i*.65)*(.45 if role=='idle' else 1.2))
    world={}
    for n in rest:
        parent=parents[n]
        inherited=world[parent]@rest[parent].inverted()@rest[n] if parent else rest[n].copy()
        location=inherited.translation.copy()+(shift if n=='pelvis' else Vector())
        world[n]=Matrix.Translation(location)@(q[n]@inherited.to_quaternion()).to_matrix().to_4x4()
    # Analytic tripod leg IK. Poles are fixed well away from the limb axis.
    for n, (a,b,c) in legs.items():
        thigh,shin,foot=['%s_%s'%(s,n) for s in ['thigh','shin','foot']]
        start=world[thigh].translation; target=feet[n]
        l1=(Vector(b)-Vector(a)).length; l2=(Vector(c)-Vector(b)).length
        offset=target-start; distance=min(offset.length,(l1+l2)*.997)
        direction=offset.normalized()
        pole=Vector(b)+Vector((0,-.7,0))
        bend=(pole-start)-direction*(pole-start).dot(direction); bend.normalize()
        along=(l1*l1-l2*l2+distance*distance)/(2*distance)
        joint=start+direction*along+bend*math.sqrt(max(0,l1*l1-along*along))
        end=start+direction*distance
        world[thigh]=aim(thigh,start,joint); world[shin]=aim(shin,joint,end)
        world[foot]=Matrix.Translation(end)@rest[foot].to_quaternion().to_matrix().to_4x4()
    for n in rest:
        parent=parents[n]
        inherited=world[parent]@rest[parent].inverted()@rest[n] if parent else rest[n]
        rig.pose.bones[n].matrix_basis=inherited.inverted()@world[n]
    bpy.context.view_layer.update()
scene=bpy.context.scene
scene.render.fps=30
rig.animation_data_create()
for b in rig.pose.bones:
    b.rotation_mode='QUATERNION'
roles={'idle':3.4,'walk':1.6,'run':.9,'melee_1':.65,'melee_2':.7,'melee_3':.85,'jump':1.,'dash':.45,'taunt':2.6,'land':.35}
actions={}
for role,duration in roles.items():
    action=bpy.data.actions.new('Nyarlathotep_'+role+'_'+revision)
    action.use_fake_user=True; action['CultUlhu.MotionRole']=role; action['CultUlhu.InPlace']=True
    rig.animation_data.action=action
    frames=round(duration*30); previous={}; low=0.
    for f in range(frames+1):
        scene.frame_set(f+1); pose(role,f/frames)
        for b in rig.pose.bones:
            b.rotation_mode='QUATERNION'
            if b.name in previous and previous[b.name].dot(b.rotation_quaternion)<0:
                b.rotation_quaternion.negate()
            previous[b.name]=b.rotation_quaternion.copy()
            for prop in ['location','rotation_quaternion','scale']:
                b.keyframe_insert(data_path=prop,frame=f+1,group=b.name)
        actual=mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
        p=np.array([tuple(v.co) for v in actual.data.vertices]); assert np.isfinite(p).all()
        # Sub-centimetre sole correction uses evaluated skin, not an ankle guess.
        # Keep authored jump clearance; only remove penetration into the floor.
        correction=max(0., .001-float(p[:,2].min()))
        if correction:
            rig.pose.bones['root'].location += rest['root'].to_3x3().inverted() @ Vector((0,0,correction))
            rig.pose.bones['root'].keyframe_insert(data_path='location',frame=f+1,group='root')
            bpy.context.view_layer.update()
            actual=mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
            p=np.array([tuple(v.co) for v in actual.data.vertices])
        low=min(low,float(p[:,2].min()))
    actions[role]={'frames':frames+1,'minimumSurfaceZ':low}
    print('TRIPOD_ACTION',role,actions[role],flush=True)
rig.animation_data.action=next(a for a in bpy.data.actions if a.get('CultUlhu.MotionRole')=='idle')
scene.frame_set(1);scene.frame_start=1;scene.frame_end=103
scene.world=bpy.data.worlds.new('Nyarlathotep Review World');scene.world.color=(.035,.045,.055)
scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.color_type='OBJECT'
scene.display.shading.background_type='WORLD';scene.display.shading.show_cavity=True
scene.render.resolution_x=600;scene.render.resolution_y=600;scene.render.resolution_percentage=100
camera_data=bpy.data.cameras.new('Tripod Review');camera_data.type='ORTHO';camera_data.ortho_scale=3.2
camera=bpy.data.objects.new('Tripod Review',camera_data);scene.collection.objects.link(camera)
camera.location=(3.2,5.2,2.3);camera.rotation_euler=(Vector((0,.2,1))-camera.location).to_track_quat('-Z','Y').to_euler();scene.camera=camera
rig.hide_render=True
for role in roles:
    action=next(a for a in bpy.data.actions if a.get('CultUlhu.MotionRole')==role)
    rig.animation_data.action=action
    if action.slots:rig.animation_data.action_slot=action.slots[0]
    scene.frame_set(1+round((actions[role]['frames']-1)*.45))
    scene.render.filepath=str(dest/(role+'.png'));bpy.ops.render.render(write_still=True)
rig.hide_render=False;rig.show_in_front=True
rig.animation_data.action=next(a for a in bpy.data.actions if a.get('CultUlhu.MotionRole')=='idle');scene.frame_set(1)
bpy.ops.wm.save_as_mainfile(filepath=str(output),check_existing=False,compress=True)
assert sha(source)==source_hash
(dest/'report.json').write_text(json.dumps({'character':'nyarlathotep','sourceSha256':source_hash,'blendSha256':sha(output),'bones':len(data.bones),'skinning':skin,'actions':actions,'abilities':False,'status':'first anatomical motion study; requires visual and deformation review; not game integrated'},indent=2)+'\n')
print('TRIPOD_MOTION_STUDY_SAVED',str(output),flush=True)
