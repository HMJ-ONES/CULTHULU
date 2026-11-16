"""Original basic motion authoring on a supplied rig, preserving source data.

The authoring frame wraps the complete import. Mesh coordinates, rest bones,
weights, UVs and supplied action keys remain unchanged. Export in source space.
"""
from pathlib import Path
import hashlib
import json
import math
import struct
import bpy
import numpy as np
from mathutils import Matrix, Quaternion, Vector

PROJECT=Path(__file__).resolve().parents[1]
ROOT=PROJECT.parent
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()

def approved_source(key):
    config=json.loads((PROJECT/'Config/ApprovedCharacterModels.json').read_text())['characters'][key]
    assert config['approved'] and config['rights_cleared']
    path=PROJECT/config['source_file']
    if not path.exists():
        archive=ROOT/'assets/character_animation/source_archive'
        metadata=json.loads((archive/'manifest.json').read_text())[key]
        path=archive/metadata['decodedFile']
        assert path.is_file() and sha(path)==metadata['sha256'],'Decode the approved source archive first'
    return path

def action_curves(action):
    return [curve for layer in action.layers for strip in layer.strips for bag in strip.channelbags for curve in bag.fcurves]

def action_signature(action):
    rows=[]
    for curve in action_curves(action):
        rows.append([curve.data_path,curve.array_index,[[*p.co,*p.handle_left,*p.handle_right,
            p.interpolation,p.handle_left_type,p.handle_right_type,p.easing] for p in curve.keyframe_points]])
    return hashlib.sha256(json.dumps([action.name,sorted(rows,key=lambda r:(r[0],r[1]))],separators=(',',':')).encode()).hexdigest()

def rig_signature(rig):
    data=[(b.name,b.parent.name if b.parent else None,[list(row) for row in b.matrix_local],b.use_deform,
        rig.pose.bones[b.name].rotation_mode) for b in rig.data.bones]
    return hashlib.sha256(json.dumps(data,separators=(',',':')).encode()).hexdigest()

def mesh_signature(obj):
    mesh=obj.data;h=hashlib.sha256()
    def array(collection,key,components,dtype):
        a=np.empty(len(collection)*components,dtype=dtype);collection.foreach_get(key,a);h.update(a.tobytes())
    array(mesh.vertices,'co',3,np.float32);array(mesh.loops,'vertex_index',1,np.int32)
    array(mesh.polygons,'loop_total',1,np.int32);array(mesh.polygons,'material_index',1,np.int32)
    for layer in mesh.uv_layers:
        h.update(layer.name.encode());array(layer.data,'uv',2,np.float32)
    h.update(json.dumps([g.name for g in obj.vertex_groups]).encode())
    for vertex in mesh.vertices:
        h.update(struct.pack('<I',len(vertex.groups)))
        for group in vertex.groups:h.update(struct.pack('<If',group.group,group.weight))
    h.update(json.dumps([m.name if m else None for m in mesh.materials]).encode())
    return h.hexdigest()

def smooth(t):
    t=max(0,min(1,t));return t*t*t*(10+t*(-15+6*t))

def envelope(t,points):
    for (a,x),(b,y) in zip(points,points[1:]):
        if t<=b:return x+(y-x)*smooth((t-a)/(b-a))
    return points[-1][1]

def rot(x=0,y=0,z=0):
    return Quaternion((0,0,1),math.radians(z))@Quaternion((0,1,0),math.radians(y))@Quaternion((1,0,0),math.radians(x))

class SuppliedStudy:
    def __init__(self,key,title,revision,yaw):
        assert bpy.app.background
        self.key,self.title,self.revision=key,title,revision
        assert revision.startswith('v') and revision[1:].isdigit()
        self.dest=ROOT/'assets/character_animation'/key/revision;self.dest.mkdir(parents=True,exist_ok=True)
        self.output=self.dest/(title+'_BasicMotion.blend')
        assert not self.output.exists() and not (self.dest/'report.json').exists()
        config=json.loads((PROJECT/'Config/ApprovedCharacterModels.json').read_text())['characters'][key]
        assert config['approved'] and config['rights_cleared']
        self.source=approved_source(key);self.source_sha=sha(self.source)
        bpy.ops.wm.read_factory_settings(use_empty=True)
        bpy.ops.import_scene.fbx(filepath=str(self.source),use_anim=True)
        self.scene=bpy.context.scene
        self.source_fps=self.scene.render.fps;self.source_fps_base=self.scene.render.fps_base
        self.rate=self.source_fps/self.source_fps_base
        rigs=[o for o in self.scene.objects if o.type=='ARMATURE']
        assert len(rigs)==1,'This recipe expects one supplied armature'
        self.rig=rigs[0];self.meshes=[o for o in self.scene.objects if o.type=='MESH']
        self.original=self.rig.animation_data.action;assert self.original
        self.original_sha=action_signature(self.original)
        self.original_rig_sha=rig_signature(self.rig)
        self.original_mesh_sha={m.name:mesh_signature(m) for m in self.meshes}
        self.source_start,self.source_end=map(float,self.original.frame_range)
        self.scene.frame_set(int(self.source_start),subframe=self.source_start%1)
        bpy.context.view_layer.update()
        self.object_basis=self.rig.matrix_basis.copy()
        raw=self.points()
        rotation=Matrix.Rotation(math.radians(yaw),4,'Z')
        turned=np.einsum('ij,nj->ni',np.array(rotation.to_3x3()),raw,optimize=False)
        low,high=turned.min(axis=0),turned.max(axis=0)
        origin=np.array([(low[0]+high[0])/2,(low[1]+high[1])/2,low[2]])
        scale=2/(high[2]-low[2])
        roots=[o for o in list(self.scene.objects) if not o.parent]
        self.frame=bpy.data.objects.new(title+'_AuthorFrame',None);self.scene.collection.objects.link(self.frame)
        self.frame.matrix_world=Matrix.Scale(float(scale),4)@Matrix.Translation(-Vector(origin))@rotation
        for obj in roots:obj.parent=self.frame;obj.matrix_parent_inverse=Matrix.Identity(4)
        bpy.context.view_layer.update()
        self.m=self.rig.matrix_world.copy();self.mi=self.m.inverted()
        scales=self.m.to_scale();assert max(scales)-min(scales)<max(scales)*1e-4 and self.m.determinant()>0
        self.s=float(sum(scales)/3);self.scale=Matrix.Diagonal((self.s,self.s,self.s,1))
        inverse_scale=Matrix.Diagonal((1/self.s,1/self.s,1/self.s,1))
        self.base={b.name:self.m@b.matrix@inverse_scale for b in self.rig.pose.bones}
        self.rest={b.name:b.matrix_local.copy() for b in self.rig.data.bones}
        self.rest_world={n:self.m@r@inverse_scale for n,r in self.rest.items()}
        self.parents={b.name:b.parent.name if b.parent else None for b in self.rig.data.bones}
        self.heads={n:m.translation.copy() for n,m in self.base.items()}
        self.tails={b.name:self.m@b.tail for b in self.rig.pose.bones}
        self.actions={};self.qa={}
        self.frame_record={'sourceSha256':self.source_sha,'yawDegrees':yaw,'originAfterRotation':origin.tolist(),
            'scale':float(scale),'referenceFrame':self.source_start,'sourceFPS':self.source_fps,
            'sourceFPSBase':self.source_fps_base,'sourceAction':self.original.name,
            'frame':'supplied motion first pose; source face rotated to +Y authoring forward; height 2m; lowest surface Z=0'}
        for mesh in self.meshes:mesh.color=(.27,.38,.34,1)

    def points(self):
        graph=bpy.context.evaluated_depsgraph_get();surfaces=[]
        for obj in self.meshes:
            vertices=obj.evaluated_get(graph).data.vertices
            p=np.empty(len(vertices)*3,dtype=np.float32);vertices.foreach_get('co',p)
            surfaces.append(np.einsum('ij,nj->ni',np.array(obj.matrix_world.to_3x3()),p.reshape(-1,3),optimize=False)+np.array(obj.matrix_world.translation))
        return np.concatenate(surfaces)

    def fk(self,q,shift,overrides=None):
        world={}
        for name,base in self.base.items():
            if overrides and name in overrides:
                world[name]=overrides[name].copy();continue
            parent=self.parents[name]
            inherited=world[parent]@self.base[parent].inverted()@base if parent else base.copy()
            position=inherited.translation.copy()
            if name=='Pelvis':position+=shift
            world[name]=Matrix.Translation(position)@q.get(name,Quaternion()).to_matrix().to_4x4()@inherited.to_3x3().to_4x4()
        return world

    def aim(self,name,a,b,reference_direction=None):
        direction=reference_direction if reference_direction is not None else self.tails[name]-self.heads[name]
        delta=direction.normalized().rotation_difference((b-a).normalized())
        return Matrix.Translation(a)@delta.to_matrix().to_4x4()@self.base[name].to_3x3().to_4x4()

    def chain(self,world,names,end_name,target,initial_points=None):
        points=[p.copy() for p in initial_points] if initial_points is not None else [world[n].translation.copy() for n in names]+[world[end_name].translation.copy()]
        reference=[self.heads[n] for n in names]+[self.heads[end_name]]
        lengths=[(b-a).length for a,b in zip(reference,reference[1:])]
        anchor=points[0].copy()
        if (target-anchor).length>sum(lengths)*.997:
            direction=(target-anchor).normalized()
            for i,length in enumerate(lengths):points[i+1]=points[i]+direction*length
        else:
            for iteration in range(16):
                points[-1]=target.copy()
                for i in range(len(names)-1,-1,-1):points[i]=points[i+1]+(points[i]-points[i+1]).normalized()*lengths[i]
                points[0]=anchor.copy()
                for i,length in enumerate(lengths):points[i+1]=points[i]+(points[i+1]-points[i]).normalized()*length
        for i,name in enumerate(names):world[name]=self.aim(name,points[i],points[i+1],reference[i+1]-reference[i])
        return points[-1]

    def descend(self,world,parent,reference=None,skip=()):
        reference=reference or self.base
        for bone in self.rig.data.bones[parent].children:
            if bone.name in skip:continue
            world[bone.name]=world[parent]@reference[parent].inverted()@reference[bone.name]
            self.descend(world,bone.name,reference,skip)

    def apply(self,world):
        local={n:self.mi@m@self.scale for n,m in world.items()}
        for name,matrix in local.items():
            parent=self.parents[name]
            inherited=local[parent]@self.rest[parent].inverted()@self.rest[name] if parent else self.rest[name]
            self.rig.pose.bones[name].matrix_basis=inherited.inverted()@matrix
        bpy.context.view_layer.update()

    def bake(self,roles,recipe):
        # Source action retains its original time base and exact keys. New keys
        # are sampled at 60 Hz using fractional frames in that same time base.
        for role,seconds in roles.items():
            action=bpy.data.actions.new(self.title+'_'+role+'_'+self.revision);action.use_fake_user=True
            self.rig.animation_data.action=action
            action['CultUlhu.MotionRole']=role;action['CultUlhu.InPlace']=True
            frames=round(seconds*60);previous={};first=None;last=None;lows=[]
            for f in range(frames+1):
                frame=1+f*self.rate/60
                self.scene.frame_set(int(frame),subframe=frame%1)
                self.rig.matrix_basis=self.object_basis.copy()
                self.apply(recipe(role,f/frames))
                for bone in self.rig.pose.bones:
                    rotation='rotation_quaternion' if bone.rotation_mode=='QUATERNION' else 'rotation_axis_angle' if bone.rotation_mode=='AXIS_ANGLE' else 'rotation_euler'
                    if rotation=='rotation_quaternion':
                        if bone.name in previous and previous[bone.name].dot(bone.rotation_quaternion)<0:bone.rotation_quaternion.negate()
                        previous[bone.name]=bone.rotation_quaternion.copy()
                    for prop in ['location',rotation,'scale']:bone.keyframe_insert(data_path=prop,frame=frame,group=bone.name)
                rotation='rotation_quaternion' if self.rig.rotation_mode=='QUATERNION' else 'rotation_euler'
                for prop in ['location',rotation,'scale']:self.rig.keyframe_insert(data_path=prop,frame=frame)
                points=self.points();assert np.isfinite(points).all()
                first=points.copy() if first is None else first;last=points;lows.append(float(points[:,2].min()))
            closure=float(np.linalg.norm(first-last,axis=1).max())
            if role in ['idle','walk','run']:assert closure<.001,(role,closure)
            self.actions[role]=action
            self.qa[role]={'samples':frames+1,'sampleRateHz':60,'seconds':frames/60,
                'minimumSurfaceZ':min(lows),'endpointMaxVertexDistance':closure}
            print('SUPPLIED_RIG_MOTION',role,self.qa[role],flush=True)
        self.original['CultUlhu.MotionRole']='dash';self.original['CultUlhu.SourceMotionName']='Prowl'
        self.original.use_fake_user=True;self.actions['dash']=self.original
        self.rig.animation_data.action=self.original
        if self.original.slots:self.rig.animation_data.action_slot=self.original.slots[0]
        lows=[]
        for frame in np.linspace(self.source_start,self.source_end,69):
            self.scene.frame_set(int(frame),subframe=float(frame%1));lows.append(float(self.points()[:,2].min()))
        self.qa['dash']={'sourceMotion':'Prowl','frames':list(self.original.frame_range),
            'seconds':(self.source_end-self.source_start)/self.rate,'minimumSurfaceZ':min(lows),'sourceKeysPreserved':True}
        assert action_signature(self.original)==self.original_sha

    def save(self):
        scene=self.scene
        scene.world=bpy.data.worlds.new(self.title+' Review World');scene.world.color=(.035,.045,.055)
        scene.render.engine='BLENDER_WORKBENCH';scene.display.shading.color_type='OBJECT'
        scene.display.shading.background_type='WORLD';scene.display.shading.show_cavity=True
        scene.render.resolution_x,scene.render.resolution_y,scene.render.resolution_percentage=480,600,100
        data=bpy.data.cameras.new('Supplied Rig Review');data.type='ORTHO';data.ortho_scale=3.0
        camera=bpy.data.objects.new('Supplied Rig Review',data);scene.collection.objects.link(camera)
        camera.location=(3.2,5.2,2.3);camera.rotation_euler=(Vector((0,0,1))-camera.location).to_track_quat('-Z','Y').to_euler();scene.camera=camera
        self.rig.hide_render=True
        for role,action in self.actions.items():
            self.rig.animation_data.action=action
            if action.slots:self.rig.animation_data.action_slot=action.slots[0]
            start,end=map(float,action.frame_range)
            for i,t in enumerate([0,.25,.5,.75]):
                f=start+t*(end-start);scene.frame_set(int(f),subframe=f%1)
                scene.render.filepath=str(self.dest/f'{role}_{i}.png');bpy.ops.render.render(write_still=True)
        self.rig.hide_render=False;self.rig.show_in_front=True
        self.rig.animation_data.action=self.actions['idle']
        if self.actions['idle'].slots:self.rig.animation_data.action_slot=self.actions['idle'].slots[0]
        scene.frame_start,scene.frame_end=1,round(float(self.actions['idle'].frame_range[1]));scene.frame_set(1)
        bpy.ops.file.pack_all()
        assert action_signature(self.original)==self.original_sha and rig_signature(self.rig)==self.original_rig_sha
        assert all(mesh_signature(m)==self.original_mesh_sha[m.name] for m in self.meshes)
        assert sha(self.source)==self.source_sha
        bpy.ops.wm.save_as_mainfile(filepath=str(self.output),check_existing=False,compress=True)
        report={'character':self.key,'revision':self.revision,'source':self.source.relative_to(ROOT).as_posix(),
            'sourceSha256':self.source_sha,'blend':self.output.relative_to(ROOT).as_posix(),'blendSha256':sha(self.output),
            'boneCount':len(self.rig.data.bones),'sourceFPS':self.source_fps,'sourceFPSBase':self.source_fps_base,
            'sourceAction':self.original.name,'sourceActionSha256':self.original_sha,'sourceRigSha256':self.original_rig_sha,
            'sourceMeshesSha256':self.original_mesh_sha,'normalizationObject':self.frame.name,
            'normalizationMatrix':[list(row) for row in self.frame.matrix_world],'authoringFrame':self.frame_record,
            'actions':self.qa,'abilities':False,'status':'supplied-rig motion study; independent preservation and visual review required; not game integrated'}
        (self.dest/'report.json').write_text(json.dumps(report,indent=2)+'\n')
        print('SUPPLIED_RIG_STUDY_SAVED',str(self.output),flush=True)
