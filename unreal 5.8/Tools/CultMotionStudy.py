"""Shared anatomical motion-study authoring in background Blender.

Immutable geometry, bounded skin influences, true evaluated-surface checks and
fresh versioned files. Character recipes own anatomy and motion decisions.
"""
from pathlib import Path
import hashlib
import json
import math
import runpy
import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector

PROJECT = Path(__file__).resolve().parents[1]
ROOT = PROJECT.parent
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()

def smooth(t):
    t = max(0., min(1., t))
    return t*t*t*(10+t*(-15+6*t))

def envelope(t, points):
    for (a,x),(b,y) in zip(points,points[1:]):
        if t <= b:
            return x+(y-x)*smooth((t-a)/(b-a))
    return points[-1][1]

def rot(x=0, y=0, z=0):
    return Quaternion((0,0,1),math.radians(z)) @ Quaternion((0,1,0),math.radians(y)) @ Quaternion((1,0,0),math.radians(x))

class Study:
    def __init__(self, key, title, revision, color):
        assert bpy.app.background, 'Never reset a live artist session'
        assert revision.startswith('v') and revision[1:].isdigit()
        self.key, self.title, self.revision = key, title, revision
        self.dest = ROOT/'assets/character_animation'/key/revision
        self.dest.mkdir(parents=True,exist_ok=True)
        self.output = self.dest/(title+'_BasicMotion.blend')
        assert not self.output.exists(), 'Preserve existing artist revisions'
        self.source = ROOT/f'assets/retopology/{key}_retopology.obj'
        self.source_hash = sha(self.source)
        anatomy = json.loads((PROJECT/'Config/CharacterAnatomicalFrames.json').read_text())[key]
        assert anatomy['sourceSha256'] == self.source_hash
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.wm.obj_import(filepath=str(self.source),forward_axis='Y',up_axis='Z')
        self.mesh = next(o for o in bpy.context.scene.objects if o.type=='MESH')
        self.mesh.name = title+'_Retopology_Animated'
        rotation = Matrix.Rotation(math.radians(anatomy['yawDegrees']),4,'Z')
        for vertex in self.mesh.data.vertices:
            vertex.co = ((rotation@(self.mesh.matrix_world@vertex.co))-Vector(anatomy['originAfterRotation']))*anatomy['scale']
        self.mesh.matrix_world = Matrix.Identity(4)
        for face in self.mesh.data.polygons:
            face.use_smooth = True
        self.mesh.color = color
        self.data = bpy.data.armatures.new(title+'_AnatomicalSkeleton')
        self.rig = bpy.data.objects.new(title+'_AnimationRig',self.data)
        bpy.context.scene.collection.objects.link(self.rig)
        bpy.context.view_layer.objects.active = self.rig
        self.mesh.select_set(False)
        self.rig.select_set(True)
        bpy.ops.object.mode_set(mode='EDIT')

    def bone(self, name, a, b, parent=None, deform=True):
        item = self.data.edit_bones.new(name)
        item.head, item.tail, item.use_deform = a,b,deform
        if parent:
            item.parent = self.data.edit_bones[parent]

    def bind(self, groups, seeds, region_masks=None, region_proximity=0.):
        bpy.ops.object.mode_set(mode='OBJECT')
        self.mesh.parent = self.rig
        modifier = self.mesh.modifiers.new('Anatomical skinning','ARMATURE')
        modifier.object = self.rig
        modifier.use_deform_preserve_volume = False
        self.skin = runpy.run_path(str(PROJECT/'Tools/CultSkinWeights.py'))['bind'](self.mesh,self.rig,groups,seeds,region_masks=region_masks,region_proximity=region_proximity)
        self.rest = {b.name:b.matrix_local.copy() for b in self.data.bones}
        self.parents = {b.name:b.parent.name if b.parent else None for b in self.data.bones}
        self.heads = {b.name:b.head_local.copy() for b in self.data.bones}
        self.tails = {b.name:b.tail_local.copy() for b in self.data.bones}
        self.rig.animation_data_create()
        for bone in self.rig.pose.bones:
            bone.rotation_mode = 'QUATERNION'
        self.vertices = np.array([tuple(v.co) for v in self.mesh.data.vertices])
        self.edges = np.array([tuple(e.vertices) for e in self.mesh.data.edges])
        self.lengths = np.linalg.norm(self.vertices[self.edges[:,0]]-self.vertices[self.edges[:,1]],axis=1)
        self.valid = self.lengths>1e-5

    def fk(self, rotations, pelvis_shift):
        world = {}
        for name, rest in self.rest.items():
            parent = self.parents[name]
            inherited = world[parent]@self.rest[parent].inverted()@rest if parent else rest.copy()
            location = inherited.translation.copy()
            if name == 'pelvis':
                location += pelvis_shift
            world[name] = Matrix.Translation(location)@(rotations.get(name,Quaternion())@inherited.to_quaternion()).to_matrix().to_4x4()
        return world

    def aim(self, name, a, b):
        delta = (self.tails[name]-self.heads[name]).normalized().rotation_difference((b-a).normalized())
        return Matrix.Translation(a)@(delta@self.rest[name].to_quaternion()).to_matrix().to_4x4()

    def leg(self, world, side, target):
        first,second,foot = [s+'_'+side for s in ['thigh','shin','foot']]
        a,b,c = self.heads[first],self.heads[second],self.tails[second]
        start = world[first].translation
        l1,l2 = (b-a).length,(c-b).length
        offset = target-start
        distance = max(abs(l1-l2)+.0001,min(offset.length,(l1+l2)*.997))
        direction = offset.normalized()
        rest_direction = (c-a).normalized()
        bend = (b-a)-rest_direction*(b-a).dot(rest_direction)
        pole = b+bend.normalized()*.8
        bend = (pole-start)-direction*(pole-start).dot(direction)
        bend.normalize()
        along = (l1*l1-l2*l2+distance*distance)/(2*distance)
        joint = start+direction*along+bend*math.sqrt(max(0.,l1*l1-along*along))
        end = start+direction*distance
        world[first],world[second] = self.aim(first,start,joint),self.aim(second,joint,end)
        world[foot] = Matrix.Translation(end)@self.rest[foot].to_quaternion().to_matrix().to_4x4()

    def apply(self, world):
        for name,matrix in world.items():
            parent = self.parents[name]
            inherited = world[parent]@self.rest[parent].inverted()@self.rest[name] if parent else self.rest[name]
            loc,q,scale = (inherited.inverted()@matrix).decompose()
            bone = self.rig.pose.bones[name]
            bone.location,bone.rotation_quaternion,bone.scale = loc,q,scale
        bpy.context.view_layer.update()

    def points(self):
        evaluated = self.mesh.evaluated_get(bpy.context.evaluated_depsgraph_get())
        result = np.empty(len(evaluated.data.vertices)*3,dtype=np.float32)
        evaluated.data.vertices.foreach_get('co',result)
        return result.reshape(-1,3)

    def bake(self, roles, recipe, stance_fractions=None):
        self.actions,self.qa = {},{}
        scene = bpy.context.scene
        scene.render.fps = 60
        soles = None
        if stance_fractions is not None:
            ankles = np.array([tuple(self.heads['foot_'+s]) for s in ['L','R']])
            assignment = np.linalg.norm(self.vertices[:,None,:2]-ankles[None,:,:2],axis=2).argmin(axis=1)
            soles = {s:np.where((self.vertices[:,2]<.14)&(assignment==i))[0] for i,s in enumerate(['L','R'])}
            assert all(len(indices)>10 for indices in soles.values())
        for role,duration in roles.items():
            action = bpy.data.actions.new(self.title+'_'+role+'_'+self.revision)
            action.use_fake_user = True
            action['CultUlhu.MotionRole'],action['CultUlhu.InPlace'] = role,True
            action['CultUlhu.Authoring'] = 'Original anatomical basic motion study; no ability animation'
            self.rig.animation_data.action = action
            frames = round(duration*60)
            previous,first,last = {},None,None
            lows,stretch = [],[]
            for frame in range(frames+1):
                scene.frame_set(frame+1)
                t = frame/frames
                if soles is not None:
                    corrections = {'L':0.,'R':0.}
                    self.apply(recipe(role,t,corrections))
                    for iteration in range(4):
                        actual = self.points()
                        for side,offset in [('L',0),('R',.5)]:
                            minimum = float(actual[soles[side],2].min())
                            planted = ((t+offset)%1<stance_fractions[role]) if role in stance_fractions else (t<.26 if role=='jump' else True)
                            corrections[side] -= minimum-.002 if planted else min(0.,minimum-.002)
                        self.apply(recipe(role,t,corrections))
                else:
                    self.apply(recipe(role,t))
                points = self.points()
                correction = max(0.,.002-float(points[:,2].min()))
                if correction:
                    self.rig.pose.bones['root'].location += self.rest['root'].to_3x3().inverted()@Vector((0,0,correction))
                    bpy.context.view_layer.update()
                    points = self.points()
                assert np.isfinite(points).all()
                for bone in self.rig.pose.bones:
                    if bone.name in previous and previous[bone.name].dot(bone.rotation_quaternion)<0:
                        bone.rotation_quaternion.negate()
                    previous[bone.name] = bone.rotation_quaternion.copy()
                    for prop in ['location','rotation_quaternion','scale']:
                        bone.keyframe_insert(data_path=prop,frame=frame+1,group=bone.name)
                first = points.copy() if first is None else first
                last = points
                lows.append(float(points[:,2].min()))
                ratios = np.linalg.norm(points[self.edges[:,0]]-points[self.edges[:,1]],axis=1)[self.valid]/self.lengths[self.valid]
                stretch.append(float(np.quantile(ratios,.995)))
            closure = float(np.linalg.norm(first-last,axis=1).max())
            if role in ['idle','walk','run']:
                assert closure<.001,(role,closure)
            self.actions[role] = action
            self.qa[role] = {'frames':frames+1,'seconds':frames/60,'minimumSurfaceZ':min(lows),
                'worstP995EdgeStretch':max(stretch),'endpointMaxVertexDistance':closure}
            print('ANATOMICAL_MOTION',self.key,role,self.qa[role],flush=True)

    def save(self, center=(0,0,1), scale=2.8):
        scene = bpy.context.scene
        scene.world = bpy.data.worlds.new(self.title+' Review World')
        scene.world.color = (.035,.045,.055)
        scene.render.engine = 'BLENDER_WORKBENCH'
        scene.display.shading.color_type = 'OBJECT'
        scene.display.shading.background_type = 'WORLD'
        scene.display.shading.show_cavity = True
        scene.render.resolution_x,scene.render.resolution_y,scene.render.resolution_percentage = 480,600,100
        camera_data = bpy.data.cameras.new('Motion Review')
        camera_data.type,camera_data.ortho_scale = 'ORTHO',scale
        camera = bpy.data.objects.new('Motion Review',camera_data)
        scene.collection.objects.link(camera)
        camera.location = Vector(center)+Vector((3.2,5.2,1.3))
        camera.rotation_euler = (Vector(center)-camera.location).to_track_quat('-Z','Y').to_euler()
        scene.camera = camera
        self.rig.hide_render = True
        for role,action in self.actions.items():
            self.rig.animation_data.action = action
            if action.slots:
                self.rig.animation_data.action_slot = action.slots[0]
            for i,t in enumerate([0,.25,.45,.7]):
                scene.frame_set(1+round(t*(self.qa[role]['frames']-1)))
                scene.render.filepath = str(self.dest/f'{role}_{i}.png')
                bpy.ops.render.render(write_still=True)
        self.rig.hide_render = False
        self.rig.show_in_front = True
        self.rig.animation_data.action = self.actions['idle']
        if self.actions['idle'].slots:
            self.rig.animation_data.action_slot = self.actions['idle'].slots[0]
        scene.frame_start,scene.frame_end = 1,self.qa['idle']['frames']
        scene.frame_set(1)
        for screen in bpy.data.screens:
            for area in screen.areas:
                if area.type=='VIEW_3D':
                    area.spaces.active.region_3d.view_location = center
                    area.spaces.active.region_3d.view_distance = 3.6
        bpy.ops.wm.save_as_mainfile(filepath=str(self.output),check_existing=False,compress=True)
        assert sha(self.source)==self.source_hash
        report = {'character':self.key,'revision':self.revision,'source':self.source.relative_to(ROOT).as_posix(),
            'sourceSha256':self.source_hash,'blend':self.output.relative_to(ROOT).as_posix(),
            'blendSha256':sha(self.output),'skinning':self.skin,'boneCount':len(self.data.bones),
            'sampleRate':60,'actions':self.qa,'abilities':False,
            'status':'anatomical motion study; visual/deformation review required; not game integrated'}
        (self.dest/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        print('ANATOMICAL_STUDY_SAVED',str(self.output),flush=True)
