"""Build compact wards with the full Editor -ExecutePythonScript option.

Uses the existing CC0 kit, preserves user assets/maps, and exits the CLI editor.
A missing crown GLB is decoded individually from its small repository .b64 file.
"""
from pathlib import Path
import base64,hashlib,json,math,shutil
import unreal

def main():
    PROJECT=Path(__file__).resolve().parents[1]
    REPO=PROJECT.parent
    assert (PROJECT/'CultUlhu.uproject').is_file() and PROJECT.name=='unreal 5.8'
    OWNER='CultUlhu.BuildWardArchitecture'
    REVISION='20261021.4'
    REPORTS=PROJECT/'Saved/Reports'
    REPORTS.mkdir(parents=True,exist_ok=True)
    library=unreal.EditorAssetLibrary
    actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    meshes=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
    assert meshes is not None,'Use full Editor -ExecutePythonScript, not the Python commandlet'
    unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    MATERIALS=[unreal.load_asset(p) for p in ['/Game/CultUlhu/Materials/M_MatchStone','/Game/CultUlhu/Materials/M_ArenaSurface','/Game/CultUlhu/Materials/M_AttackSignal']]
    assert all(MATERIALS)
    BASE=unreal.load_asset('/Game/CultUlhu/World/tower-square-base/StaticMeshes/tower-square-base')
    MID=unreal.load_asset('/Game/CultUlhu/World/tower-square-mid/StaticMeshes/tower-square-mid')
    CUBE=unreal.load_asset('/Engine/BasicShapes/Cube')
    CONE=unreal.load_asset('/Engine/BasicShapes/Cone')
    SPHERE=unreal.load_asset('/Engine/BasicShapes/Sphere')
    # Import only the repository crown geometry in memory. Never save this package.
    pipeline=unreal.InterchangeGenericAssetsPipeline()
    pipeline.material_pipeline.import_materials=False
    pipeline.material_pipeline.texture_pipeline.import_textures=False
    override=unreal.InterchangePipelineStackOverride()
    override.override_pipelines=[unreal.SoftObjectPath(pipeline.get_path_name())]
    task=unreal.AssetImportTask()
    source=REPO/'assets/world/buildings/tower-square-top.glb'
    if not source.exists():
        encoded=source.with_suffix('.glb.b64')
        assert encoded.is_file(),'Missing original crown source'
        source.write_bytes(base64.b64decode(encoded.read_bytes()))
    task.filename=str(source)
    task.destination_path='/Game/CultUlhu/TransientWardBuildSource'
    task.automated=True
    task.replace_existing=False
    task.save=False
    task.options=override
    assert not library.does_directory_exist(task.destination_path),'Preserve existing source folder'
    unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
    crown_paths=[p for p in task.imported_object_paths if isinstance(unreal.load_asset(p),unreal.StaticMesh)]
    assert len(crown_paths)==1,task.imported_object_paths
    CROWN=unreal.load_asset(crown_paths[0])

    def part(items,mesh,centre,size,material=0,yaw=0):
        box=mesh.get_bounding_box()
        extent=[box.max.x-box.min.x,box.max.y-box.min.y,box.max.z-box.min.z]
        origin=[(box.min.x+box.max.x)/2,(box.min.y+box.max.y)/2,(box.min.z+box.max.z)/2]
        scale=[size[i]/extent[i] for i in range(3)]
        angle=math.radians(yaw)
        shift=[origin[i]*scale[i] for i in range(3)]
        location=unreal.Vector(centre[0]-shift[0]*math.cos(angle)+shift[1]*math.sin(angle),centre[1]-shift[0]*math.sin(angle)-shift[1]*math.cos(angle),centre[2]-shift[2])
        actor=actors.spawn_actor_from_class(unreal.StaticMeshActor,location,unreal.Rotator(pitch=0,yaw=yaw,roll=0))
        component=actor.static_mesh_component
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_static_mesh(mesh)
        component.set_world_scale3d(unreal.Vector(*scale))
        component.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
        for index in range(max(1,len(mesh.get_editor_property('static_materials')))): component.set_material(index,MATERIALS[material])
        items.append(actor)
        return actor

    def level(items,mesh,size,bottom,material=0):
        return part(items,mesh,(0,0,bottom+size[2]/2),size,material)

    def line(items,a,b,width=16):
        delta=unreal.Vector(b[0]-a[0],b[1]-a[1],b[2]-a[2])
        length=math.sqrt(sum(v*v for v in (delta.x,delta.y,delta.z)))
        actor=part(items,CUBE,tuple((a[i]+b[i])/2 for i in range(3)),(width,width,length),2)
        actor.set_actor_rotation(unreal.MathLibrary.make_rot_from_z(delta),False)

    def sigils(items,radius,height):
        for face in range(4):
            a=face*math.pi/2
            def point(x,z): return (radius*math.cos(a)-x*math.sin(a),radius*math.sin(a)+x*math.cos(a),height+z)
            diamond=[(-95,0),(0,165),(95,0),(0,-165)]
            for i in range(4): line(items,point(*diamond[i]),point(*diamond[(i+1)%4]))
            line(items,point(0,-75),point(0,75),20)
            for sign in (-1,1): line(items,point(0,sign*215),point(0,sign*285),14)

    def own_backup(path):
        if not library.does_asset_exist(path): return
        asset=unreal.load_asset(path)
        assert library.get_metadata_tag(asset,'CultWardOwner')==OWNER,'Refusing to replace user asset: '+path
        disk=PROJECT/'Content'/(path.removeprefix('/Game/')+'.uasset')
        digest=hashlib.sha256(disk.read_bytes()).hexdigest()[:16]
        backup=REPORTS/'WardArchitectureBackups'/digest
        backup.mkdir(parents=True,exist_ok=True)
        for suffix in ('.uasset','.uexp','.ubulk'):
            source=disk.with_suffix(suffix)
            if source.exists():
                target=backup/source.name
                assert not target.exists() or target.read_bytes()==source.read_bytes()
                shutil.copyfile(source,target)
        assert library.delete_asset(path)

    result=[]
    for nexus in (False,True):
        name='SM_NexusWard' if nexus else 'SM_LaneWard'
        path='/Game/CultUlhu/World/Wards/'+name
        if library.does_asset_exist(path) and library.get_metadata_tag(unreal.load_asset(path),'CultWardRevision')==REVISION:
            result.append({'asset':path,'reused_revision':REVISION})
            continue
        own_backup(path)
        items=[]
        if not nexus:
            level(items,BASE,(620,620,440),-1250)
            level(items,MID,(560,560,880),-810)
            level(items,CROWN,(680,680,280),70,1)
            for x in (-215,215):
                for y in (-215,215): part(items,CONE,(x,y,595),(110,110,650))
            sigils(items,288,-290)
            part(items,SPHERE,(0,0,500),(300,300,300),2)
        else:
            level(items,BASE,(740,740,350),-1250)
            level(items,MID,(620,620,600),-900)
            level(items,CROWN,(760,760,240),-300,1)
            for i in range(4):
                a=i*math.pi/2
                centre=(280*math.cos(a),280*math.sin(a),300)
                part(items,MID,centre,(140,140,1100),0,i*90)
                part(items,CONE,(centre[0],centre[1],880),(200,200,280))
            for z in (350,650):
                for side in (-1,1):
                    part(items,CUBE,(side*280,0,z),(40,560,35),1)
                    part(items,CUBE,(0,side*280,z),(560,40,35),1)
            part(items,SPHERE,(0,0,500),(430,430,430),2)
            sigils(items,318,-600)
        settings=unreal.MeshMergingSettings()
        settings.merge_materials=False
        settings.merge_physics_data=False
        settings.generate_light_map_uv=False
        settings.bake_vertex_data_to_mesh=True
        settings.pivot_type=unreal.MeshMergePivotType.WORLD_ORIGIN
        options=unreal.MergeStaticMeshActorsOptions()
        options.base_package_name=path.rsplit('/',1)[0]+'/'+name.removeprefix('SM_')
        options.new_actor_label=name
        options.destroy_source_actors=True
        options.spawn_merged_actor=True
        options.mesh_merging_settings=settings
        merged=meshes.merge_static_mesh_actors(items,options)
        if isinstance(merged,tuple): merged=next((v for v in merged if isinstance(v,unreal.StaticMeshActor)),None)
        assert merged,'Merge failed: '+path
        asset=merged.static_mesh_component.static_mesh
        actual=asset.get_path_name().split('.')[0]
        if actual!=path:
            assert not library.does_asset_exist(path)
            assert library.rename_asset(actual,path)
            asset=unreal.load_asset(path)
        assert asset,'Expected merged mesh: '+path
        socket=unreal.StaticMeshSocket(outer=asset)
        socket.set_editor_property('socket_name','WardMuzzle')
        socket.set_editor_property('relative_location',unreal.Vector(0,0,500))
        asset.add_socket(socket)
        bounds=asset.get_bounding_box()
        assert max(abs(bounds.min.x),abs(bounds.max.x),abs(bounds.min.y),abs(bounds.max.y))<600,'Unexpected horizontal rotation/bounds'
        assert bounds.min.z>=-1251 and bounds.max.z<=1251,'Unexpected vertical bounds'
        library.set_metadata_tag(asset,'CultWardOwner',OWNER)
        library.set_metadata_tag(asset,'CultWardRevision',REVISION)
        allowed={m.get_path_name() for m in MATERIALS}
        assert all(m.material_interface and m.material_interface.get_path_name() in allowed for m in asset.get_editor_property('static_materials'))
        assert library.save_loaded_asset(asset)
        box=asset.get_bounding_box()
        result.append({'asset':path,'source_parts':len(items),'triangles':asset.get_num_triangles(0),'min':[box.min.x,box.min.y,box.min.z],'max':[box.max.x,box.max.y,box.max.z],'materials':[str(s.material_interface.get_path_name()) for s in asset.get_editor_property('static_materials')]})
        actors.destroy_actor(merged)
    (REPORTS/'WardArchitectureBuild.json').write_text(json.dumps(result,indent=2)+'\n',encoding='utf-8')
    unreal.log('CULT_WARD_BUILD '+json.dumps(result))

if __name__=='__main__':
    try:
        main()
    finally:
        if '-ExecutePythonScript=' in unreal.SystemLibrary.get_command_line():
            unreal.SystemLibrary.quit_editor()
