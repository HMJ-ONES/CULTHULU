"""Original Dagon anatomy rig and character-specific basic motion studies.

Background Blender only. Immutable source, versioned output, no ability clips.
Actions are in-place; character movement owns travel and jump displacement.
"""
from pathlib import Path
import sys
import json
import math
import hashlib
import runpy
import bpy
import numpy as np
from mathutils import Vector, Matrix, Quaternion

assert bpy.app.background
PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT.parent
REV = sys.argv[sys.argv.index('--') + 1] if '--' in sys.argv else 'v001'
assert REV.startswith('v') and REV[1:].isdigit()
DEST = ROOT / 'assets/character_animation/dagon' / REV
DEST.mkdir(parents=True, exist_ok=True)
BLEND = DEST / 'Dagon_BasicMotion.blend'
assert not BLEND.exists(), 'Preserve existing artist/animation revisions'
SOURCE = ROOT / 'assets/retopology/dagon_retopology.obj'
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
before = sha(SOURCE)
frame = json.loads((PROJECT / 'Config/CharacterAnatomicalFrames.json').read_text())['dagon']
assert frame['sourceSha256'] == before
bpy.ops.wm.read_factory_settings(use_empty=True)
bpy.ops.wm.obj_import(filepath=str(SOURCE), forward_axis='Y', up_axis='Z')
mesh = next(o for o in bpy.context.scene.objects if o.type == 'MESH')
mesh.name = 'Dagon_Retopology_Animated'
rotation = Matrix.Rotation(math.radians(frame['yawDegrees']), 4, 'Z')
origin = Vector(frame['originAfterRotation'])
for v in mesh.data.vertices:
    v.co = ((rotation @ (mesh.matrix_world @ v.co)) - origin) * frame['scale']
mesh.matrix_world = Matrix.Identity(4)
for f in mesh.data.polygons:
    f.use_smooth = True
mesh.color = (.24, .43, .36, 1)

data = bpy.data.armatures.new('Dagon_AnatomicalSkeleton')
rig = bpy.data.objects.new('Dagon_AnimationRig', data)
bpy.context.scene.collection.objects.link(rig)
bpy.context.view_layer.objects.active = rig
rig.select_set(True)
mesh.select_set(False)
bpy.ops.object.mode_set(mode='EDIT')


def bone(name, head, tail, parent=None, deform=True):
    b = data.edit_bones.new(name)
    b.head, b.tail, b.use_deform = head, tail, deform
    if parent:
        b.parent = data.edit_bones[parent]
    return b


bone('root', (0, 0, 0), (0, 0, .16), deform=False)
bone('pelvis', (0, -.08, .72), (0, -.10, .94), 'root')
bone('spine_01', (0, -.10, .94), (0, -.15, 1.19), 'pelvis')
bone('chest', (0, -.15, 1.19), (0, -.10, 1.43), 'spine_01')
bone('neck', (0, -.10, 1.43), (0, .03, 1.60), 'chest')
bone('head', (0, .03, 1.60), (0, .17, 1.82), 'neck')
bone('jaw', (0, .20, 1.49), (0, .39, 1.47), 'head')
bone('crest', (0, -.18, 1.69), (0, -.16, 1.95), 'head')
bone('tail_01', (0, -.19, .73), (0, -.42, .55), 'pelvis')
bone('tail_02', (0, -.42, .55), (0, -.63, .32), 'tail_01')
bone('tail_03', (0, -.63, .32), (0, -.70, .12), 'tail_02')
for side, sign in [('L', 1), ('R', -1)]:
    p = lambda x, y, z: (sign * x, y, z)
    bone('clavicle_' + side, p(.04, -.10, 1.40), p(.30, -.10, 1.43), 'chest')
    bone('upperarm_' + side, p(.30, -.10, 1.43), p(.43, -.035, 1.00), 'clavicle_' + side)
    bone('forearm_' + side, p(.43, -.035, 1.00), p(.43, .17, .61), 'upperarm_' + side)
    bone('hand_' + side, p(.43, .17, .61), p(.44, .22, .50), 'forearm_' + side)
    for i, x in enumerate([.35, .44, .52]):
        bone(f'claw{i+1}_01_{side}', p(x, .21, .53), p(x, .25, .46), 'hand_' + side)
        bone(f'claw{i+1}_02_{side}', p(x, .25, .46), p(x - .015, .29, .41), f'claw{i+1}_01_{side}')
    bone('thigh_' + side, p(.18, -.02, .70), p(.23, .06, .34), 'pelvis')
    bone('shin_' + side, p(.23, .06, .34), p(.24, -.02, .12), 'thigh_' + side)
    bone('foot_' + side, p(.24, -.02, .12), p(.26, .23, .045), 'shin_' + side)
    bone('toes_' + side, p(.26, .23, .045), p(.28, .34, .035), 'foot_' + side)
bpy.ops.object.mode_set(mode='OBJECT')
rig.show_in_front = True
data.display_type = 'OCTAHEDRAL'
for obj in bpy.context.selected_objects:
    obj.select_set(False)
mesh.select_set(True)
rig.select_set(True)
bpy.context.view_layer.objects.active = rig
bound = bpy.ops.object.parent_set(type='ARMATURE_AUTO')
assert 'FINISHED' in bound
GROUPS = {
 'body': ['pelvis', 'spine_01', 'chest', 'neck'],
 'head': ['head', 'jaw', 'crest'],
 'tail': ['tail_01', 'tail_02', 'tail_03'],
}
SEEDS = {'body': [(0, .23, .85), (0, .18, 1.2), (0, -.22, 1.12)],
         'head': [(0, .20, 1.72), (0, -.1, 1.83)],
         'tail': [(0, -.55, .35), (0, -.68, .14)]}
for side, sign in [('L', 1), ('R', -1)]:
    GROUPS['arm_'+side] = [b.name for b in data.bones if b.name.endswith('_'+side) and b.name.startswith(('clavicle', 'upperarm', 'forearm', 'hand', 'claw'))]
    GROUPS['leg_'+side] = [b.name for b in data.bones if b.name.endswith('_'+side) and b.name.startswith(('thigh', 'shin', 'foot', 'toes'))]
    SEEDS['arm_'+side] = [(sign*.365, -.07, 1.21), (sign*.43, .04, .85), (sign*.43, .17, .59)]
    SEEDS['leg_'+side] = [(sign*.20, .02, .56), (sign*.24, .02, .24), (sign*.25, .14, .08)]
skin_report = runpy.run_path(str(PROJECT/'Tools/CultSkinWeights.py'))['bind'](mesh, rig, GROUPS, SEEDS)
modifier = next(m for m in mesh.modifiers if m.type == 'ARMATURE')
modifier.use_deform_preserve_volume = True
deform = {b.name for b in data.bones if b.use_deform}
unweighted = [v.index for v in mesh.data.vertices if not any(g.weight > .00001 and mesh.vertex_groups[g.group].name in deform for g in v.groups)]
assert not unweighted, f'Unweighted vertices: {len(unweighted)}'
rest = {b.name: b.matrix_local.copy() for b in data.bones}
parents = {b.name: b.parent.name if b.parent else None for b in data.bones}
heads = {b.name: b.head_local.copy() for b in data.bones}
tails = {b.name: b.tail_local.copy() for b in data.bones}
roles = {'idle': 3.2, 'walk': 1.15, 'run': .72, 'melee_1': .56, 'melee_2': .58, 'melee_3': .72, 'jump': .9, 'land': .30, 'dash': .38, 'taunt': 2.4}
scene = bpy.context.scene
scene.render.fps = 30
rig.animation_data_create()
actions = {}


def smooth(x):
    x = max(0, min(1, x))
    return x*x*x*(10+x*(-15+6*x))


def envelope(t, points):
    for (a, x), (b, y) in zip(points, points[1:]):
        if t <= b:
            return x+(y-x)*smooth((t-a)/(b-a))
    return points[-1][1]


def rot(x=0, y=0, z=0):
    return Quaternion((0, 0, 1), math.radians(z)) @ Quaternion((0, 1, 0), math.radians(y)) @ Quaternion((1, 0, 0), math.radians(x))


def aimed(name, a, b):
    direction = (b-a).normalized()
    delta = (tails[name]-heads[name]).normalized().rotation_difference(direction)
    return Matrix.Translation(a) @ (delta @ rest[name].to_quaternion()).to_matrix().to_4x4()


def limb_pole(first, second, displacement):
    a, b, c = heads[first], heads[second], tails[second]
    direction = (c-a).normalized()
    bend = (b-a)-direction*(b-a).dot(direction)
    return b+bend.normalized()*.8+displacement


def chain_matrices(first, second, a, b, c):
    rest_a, rest_b = (tails[first]-heads[first]).normalized(), (tails[second]-heads[second]).normalized()
    normal_rest = rest_a.cross(rest_b).normalized()
    normal_pose = (b-a).cross(c-b).normalized()
    def frame(direction, normal):
        return Matrix((normal, direction, normal.cross(direction))).transposed()
    result = []
    for name, point, direction, rest_dir in [(first, a, (b-a).normalized(), rest_a), (second, b, (c-b).normalized(), rest_b)]:
        rotation = frame(direction, normal_pose) @ frame(rest_dir, normal_rest).transposed()
        result.append(Matrix.Translation(point) @ (rotation.to_quaternion() @ rest[name].to_quaternion()).to_matrix().to_4x4())
    return result


def solve(a, target, pole, l1, l2):
    offset = target-a
    distance = min(offset.length, (l1+l2)*.997)
    distance = max(distance, abs(l1-l2)+.0001)
    direction = offset.normalized()
    bend = pole-a-direction*(pole-a).dot(direction)
    bend.normalize()
    along = (l1*l1-l2*l2+distance*distance)/(2*distance)
    height = math.sqrt(max(0, l1*l1-along*along))
    return a+direction*along+bend*height, a+direction*distance


def pose(role, t, foot_correction=None):
    phase = 2*math.pi*t
    q = {n: Quaternion() for n in rest}
    delta = Vector((0, 0, 0))
    hands = {s: heads['hand_'+s].copy() for s in ['L', 'R']}
    feet = {s: heads['foot_'+s].copy() for s in ['L', 'R']}
    pitch = {'L': 0., 'R': 0.}
    toe = {'L': 0., 'R': 0.}
    if role == 'idle':
        q['chest'] = rot(x=math.sin(phase)*.85)
        q['neck'] = rot(x=-math.sin(phase)*.35)
        q['head'] = rot(z=math.sin(phase)*1.4)
        delta.x = math.sin(phase)*.004
    if role in ['walk', 'run']:
        running = role == 'run'
        stance = .48 if running else .62
        stride = .64 if running else .36
        delta.x = math.sin(phase)*(.018 if running else .012)
        delta.z = (.025 if running else .012)*math.cos(phase*2)-(.035 if running else .018)
        q['pelvis'] = rot(y=math.sin(phase)*2.0, z=math.sin(phase)*2.4)
        q['spine_01'] = rot(x=-7 if running else -2.5, z=-math.sin(phase)*1.5)
        q['chest'] = rot(z=-math.sin(phase)*3.2)
        q['head'] = rot(x=4 if running else 1.4)
        for s, sign, offset in [('L', 1, 0), ('R', -1, .5)]:
            u = (t+offset) % 1
            if u < stance:
                f = u/stance
                feet[s].y += stride*(.5-f)
                pitch[s] = envelope(f, [(0, 10), (.18, 0), (.72, 0), (1, -16)])
                toe[s] = max(0, (f-.72)/.28)*18
            else:
                f = (u-stance)/(1-stance)
                feet[s].y += stride*(-.5+smooth(f))
                feet[s].z += (.19 if running else .095)*math.sin(math.pi*f)**1.3
                pitch[s] = -16*(1-smooth(f))+10*smooth(f)
            hands[s].y -= math.cos(phase+offset*2*math.pi)*(.20 if running else .11)
            hands[s].z += .22 if running else .015
            hands[s].x += sign*(.035 if running else .012)
    if role.startswith('melee_'):
        hit = envelope(t, [(0, 0), (.25, -.3), (.44, 1), (.65, .78), (1, 0)])
        strike = max(0, hit)
        kind = int(role[-1])
        q['pelvis'] = rot(z=hit*(7 if kind == 1 else -7 if kind == 2 else 0))
        q['chest'] = rot(x=hit*(8 if kind == 3 else -3), z=hit*(12 if kind == 1 else -14 if kind == 2 else 0))
        delta.y = strike*.035
        if kind == 1:
            hands['L'] += Vector((-.44*hit, .48*hit, .42*hit))
            hands['R'] += Vector((-.03*hit, -.06*hit, .07*hit))
        elif kind == 2:
            hands['R'] += Vector((.46*hit, .46*hit, .37*hit))
            hands['L'] += Vector((.03*hit, -.08*hit, .09*hit))
        else:
            lift = envelope(t, [(0, 0), (.25, 1), (.45, .3), (.65, .12), (1, 0)])
            for s, sign in [('L', 1), ('R', -1)]:
                hands[s] += Vector((sign*.1*lift-sign*.15*strike, .45*strike+.26*lift, .72*lift+.18*strike))
            delta.z = -.055*strike
        q['head'] = rot(x=-hit*4)
    if role in ['jump', 'dash']:
        load = envelope(t, [(0, 0), (.18, 1), (.38, -.2), (.72, -.12), (1, 0)])
        delta.z = -.105*max(0, load)
        q['spine_01'] = rot(x=-9*max(0, load))
        if role == 'jump':
            air = envelope(t, [(0, 0), (.25, 0), (.48, 1), (.80, .7), (1, .45)])
            for s in feet:
                feet[s].z += air*.24
                feet[s].y -= air*.10
                hands[s].z += air*.23
                hands[s].y -= air*.06
            q['head'] = rot(x=3*air)
        else:
            surge = math.sin(math.pi*t)**2
            q['chest'] = rot(x=-15*surge)
            for s in hands:
                hands[s].y -= surge*.22
                hands[s].z += surge*.12
    if role == 'land':
        settle = math.sin(math.pi*t)**2
        delta.z = -.065*settle
        q['spine_01'] = rot(x=-5*settle)
        q['head'] = rot(x=2*settle)
        for s in hands:
            hands[s].y += .05*settle
            hands[s].z += .07*(1-smooth(t))
    if role == 'taunt':
        flare = envelope(t, [(0, 0), (.20, .65), (.42, 1), (.73, .92), (1, 0)])
        roar = flare*(.8+.2*math.sin(t*math.pi*8)**2)
        q['spine_01'] = rot(x=4*flare)
        q['chest'] = rot(x=5*flare)
        q['head'] = rot(x=9*flare, z=math.sin(phase)*4*flare)
        q['jaw'] = rot(x=-14*roar)
        delta.z = -.025*flare
        for s, sign in [('L', 1), ('R', -1)]:
            hands[s] += Vector((sign*.23*flare, .07*flare, .43*flare))
    for i in range(1, 4):
        q[f'tail_0{i}'] = rot(z=math.sin(phase-i*.65)*(1.8 if role == 'idle' else 4.0))
    q['crest'] = rot(y=math.sin(phase-.4)*.7)
    for s in ['L', 'R']:
        for i in range(1, 4):
            for j in [1, 2]:
                q[f'claw{i}_0{j}_{s}'] = rot(x=(max(0, math.sin(math.pi*t))*5 if role.startswith('melee') else -3*math.sin(phase) if role == 'taunt' else 0))
    world = {}
    for name in rest:
        parent = parents[name]
        inherited = world[parent] @ rest[parent].inverted() @ rest[name] if parent else rest[name].copy()
        location = inherited.translation.copy()
        if name == 'pelvis':
            location += delta
        world[name] = Matrix.Translation(location) @ (q[name] @ inherited.to_quaternion()).to_matrix().to_4x4()
    for s, sign in [('L', 1), ('R', -1)]:
        a = world['thigh_'+s].translation
        target = feet[s]
        if foot_correction:
            target.z += foot_correction[s]
        knee, ankle = solve(a, target, limb_pole('thigh_'+s, 'shin_'+s, delta), (tails['thigh_'+s]-heads['thigh_'+s]).length, (tails['shin_'+s]-heads['shin_'+s]).length)
        world['thigh_'+s], world['shin_'+s] = chain_matrices('thigh_'+s, 'shin_'+s, a, knee, ankle)
        world['foot_'+s] = Matrix.Translation(ankle) @ (rot(x=pitch[s]) @ rest['foot_'+s].to_quaternion()).to_matrix().to_4x4()
        inherited = world['foot_'+s] @ rest['foot_'+s].inverted() @ rest['toes_'+s]
        world['toes_'+s] = Matrix.Translation(inherited.translation) @ (rot(x=toe[s]) @ inherited.to_quaternion()).to_matrix().to_4x4()
        a = world['upperarm_'+s].translation
        target = hands[s] + delta
        pole = limb_pole('upperarm_'+s, 'forearm_'+s, delta)
        raised = smooth(max(0, (hands[s].z-heads['hand_'+s].z)/.45))
        pole = pole.lerp(Vector((sign*.87, .02, 1.06))+delta, raised*.9)
        elbow, wrist = solve(a, target, pole, (tails['upperarm_'+s]-heads['upperarm_'+s]).length, (tails['forearm_'+s]-heads['forearm_'+s]).length)
        world['upperarm_'+s], world['forearm_'+s] = chain_matrices('upperarm_'+s, 'forearm_'+s, a, elbow, wrist)
        world['hand_'+s] = Matrix.Translation(wrist) @ (world['forearm_'+s].to_quaternion() @ rest['forearm_'+s].to_quaternion().inverted() @ rest['hand_'+s].to_quaternion()).to_matrix().to_4x4()
        for i in range(1, 4):
            for j in [1, 2]:
                name = f'claw{i}_0{j}_{s}'
                inherited = world[parents[name]] @ rest[parents[name]].inverted() @ rest[name]
                world[name] = Matrix.Translation(inherited.translation) @ (q[name] @ inherited.to_quaternion()).to_matrix().to_4x4()
    # Keep the tail's ground-contacting tip from following a crouching pelvis
    # through the floor. FABRIK preserves all three actual segment lengths.
    chain = [world[f'tail_0{i}'].translation.copy() for i in range(1, 4)]
    chain.append(world['tail_03'] @ (rest['tail_03'].inverted() @ tails['tail_03']))
    lengths = [(tails[f'tail_0{i}']-heads[f'tail_0{i}']).length for i in range(1, 4)]
    anchor = chain[0].copy()
    target = chain[-1].copy()
    target.z = max(tails['tail_03'].z, target.z)+(foot_correction.get('tail', 0) if foot_correction else 0)
    for iteration in range(8):
        chain[-1] = target.copy()
        for i in range(2, -1, -1):
            chain[i] = chain[i+1]+(chain[i]-chain[i+1]).normalized()*lengths[i]
        chain[0] = anchor.copy()
        for i in range(3):
            chain[i+1] = chain[i]+(chain[i+1]-chain[i]).normalized()*lengths[i]
    for i in range(3):
        world[f'tail_0{i+1}'] = aimed(f'tail_0{i+1}', chain[i], chain[i+1])
    for name, matrix in world.items():
        parent = parents[name]
        inherited = world[parent] @ rest[parent].inverted() @ rest[name] if parent else rest[name]
        basis = inherited.inverted() @ matrix
        loc, quat, scale = basis.decompose()
        pb = rig.pose.bones[name]
        pb.rotation_mode = 'QUATERNION'
        pb.location, pb.rotation_quaternion, pb.scale = loc, quat, scale
    bpy.context.view_layer.update()


rest_vertices = np.array([tuple(v.co) for v in mesh.data.vertices])
foot_indices = {s: np.where((rest_vertices[:, 2] < .18) & (rest_vertices[:, 1] > -.27) & (rest_vertices[:, 0]*sign > .06))[0] for s, sign in [('L', 1), ('R', -1)]}
tail_indices = np.where((rest_vertices[:, 1] < -.27) & (rest_vertices[:, 2] < .28))[0]
edge_indices = np.array([tuple(e.vertices) for e in mesh.data.edges])
edge_lengths = np.linalg.norm(rest_vertices[edge_indices[:, 0]]-rest_vertices[edge_indices[:, 1]], axis=1)
valid_edges = edge_lengths > 1e-5
qa = {}
for role, duration in roles.items():
    action = bpy.data.actions.new('Dagon_' + role)
    action.use_fake_user = True
    rig.animation_data.action = action
    frames = round(duration*30)
    lows = []; stretch = []; previous_quaternions = {}
    for f in range(frames+1):
        t = f/frames
        scene.frame_set(f+1)
        pose(role, t)
        # Actual skinned sole samples drive the ground correction, not only an
        # ankle marker. Swing feet retain clearance; planted feet meet the floor.
        corrections = {'L': 0., 'R': 0., 'tail': 0.}
        if True:
            for _ in range(3):
                evaluated = mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
                actual = np.array([tuple(v.co) for v in evaluated.data.vertices])
                for s, offset in [('L', 0), ('R', .5)]:
                    low = float(actual[foot_indices[s], 2].min())
                    stance = .48 if role == 'run' else .62
                    planted = (role not in ['walk', 'run', 'jump']) or (role in ['walk', 'run'] and (t+offset) % 1 < stance) or (role=='jump' and t<.3)
                    corrections[s] -= low if planted else min(0, low)
                corrections['tail'] -= min(0, float(actual[tail_indices, 2].min())-.008)
                pose(role, t, corrections)
        for pb in rig.pose.bones:
            if pb.name in previous_quaternions and pb.rotation_quaternion.dot(previous_quaternions[pb.name]) < 0:
                pb.rotation_quaternion.negate()
            previous_quaternions[pb.name] = pb.rotation_quaternion.copy()
            pb.keyframe_insert(data_path='location', frame=f+1, group=pb.name)
            pb.keyframe_insert(data_path='rotation_quaternion', frame=f+1, group=pb.name)
            pb.keyframe_insert(data_path='scale', frame=f+1, group=pb.name)
        evaluated = mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
        actual = np.array([tuple(v.co) for v in evaluated.data.vertices])
        assert np.isfinite(actual).all()
        lows.append(float(actual[:, 2].min()))
        ratios = np.linalg.norm(actual[edge_indices[:, 0]]-actual[edge_indices[:, 1]], axis=1)[valid_edges]/edge_lengths[valid_edges]
        stretch.append(float(np.quantile(ratios, .995)))
    action['CultUlhu.MotionRole'] = role
    action['CultUlhu.InPlace'] = True
    if role in ['walk', 'run']:
        action['CultUlhu.StanceFraction'] = .48 if role=='run' else .62
        action['CultUlhu.StanceTravelMetres'] = .64 if role=='run' else .36
    action['CultUlhu.Authoring'] = 'Original anatomical Dagon motion study; no ability animation'
    actions[role] = action
    qa[role] = {'frames': frames+1, 'seconds': frames/30, 'minimumSkinnedZ': min(lows), 'worstP995EdgeStretch': max(stretch)}
    print('DAGON_ACTION', role, qa[role], flush=True)

scene.world = bpy.data.worlds.new('Motion Review World')
scene.world.color = (.035, .045, .055)
scene.render.engine = 'BLENDER_WORKBENCH'
scene.display.shading.color_type = 'OBJECT'
scene.display.shading.show_cavity = True
scene.display.shading.cavity_type = 'BOTH'
scene.display.shading.background_type = 'WORLD'
scene.render.resolution_x = 480
scene.render.resolution_y = 600
scene.render.resolution_percentage = 100
camera_data = bpy.data.cameras.new('Dagon Motion Review')
camera_data.type = 'ORTHO'
camera_data.ortho_scale = 2.8
camera = bpy.data.objects.new('Dagon Motion Review', camera_data)
scene.collection.objects.link(camera)
camera.location = (3.2, 5.2, 2.3)
camera.rotation_euler = (Vector((0, .05, 1.0))-camera.location).to_track_quat('-Z', 'Y').to_euler()
scene.camera = camera
rig.hide_render = True
for role, action in actions.items():
    rig.animation_data.action = action
    if action.slots:
        rig.animation_data.action_slot = action.slots[0]
    for i, t in enumerate([0., .25, .45, .70]):
        scene.frame_set(1+round(t*(qa[role]['frames']-1)))
        scene.render.filepath = str(DEST / f'{role}_{i}.png')
        bpy.ops.render.render(write_still=True)
rig.hide_render = False
rig.animation_data.action = actions['idle']
if actions['idle'].slots:
    rig.animation_data.action_slot = actions['idle'].slots[0]
scene.frame_start, scene.frame_end = 1, qa['idle']['frames']
scene.frame_set(1)
for obj in bpy.context.selected_objects:
    obj.select_set(False)
rig.select_set(True)
bpy.context.view_layer.objects.active = rig
for screen in bpy.data.screens:
    for area in screen.areas:
        if area.type == 'VIEW_3D':
            area.spaces.active.region_3d.view_location = (0, 0, 1)
            area.spaces.active.region_3d.view_distance = 3.6
bpy.ops.wm.save_as_mainfile(filepath=str(BLEND), check_existing=False, compress=True)
assert sha(SOURCE) == before
record = {'character': 'dagon', 'revision': REV, 'source': SOURCE.relative_to(ROOT).as_posix(), 'sourceSha256': before,
          'skinning': skin_report, 'boneCount': len(data.bones), 'unweightedVertices': len(unweighted), 'actions': qa,
          'blend': BLEND.relative_to(ROOT).as_posix(), 'blendSha256': sha(BLEND),
          'status': 'motion study; visual/deformation review required; not imported to game', 'abilities': False}
(DEST/'report.json').write_text(json.dumps(record, indent=2)+'\n')
print('DAGON_MOTION_STUDY_SAVED', str(BLEND), flush=True)
