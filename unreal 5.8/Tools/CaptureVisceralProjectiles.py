"""Render the actual imported projectile meshes/materials; never save the studio level."""
from pathlib import Path
import unreal
PROJECT=Path(__file__).resolve().parents[1]
assert Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())).resolve()==PROJECT
world=unreal.EditorLevelLibrary.get_editor_world();sub=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
origin=unreal.Vector(0,0,1000000)
surface=unreal.load_asset('/Game/CultUlhu/Materials/M_VisceralGlob');assert surface
directory=PROJECT/'Saved/Screenshots/VisceralProjectiles';directory.mkdir(parents=True,exist_ok=True)
created=[]
def make_stage(name):
    mesh=unreal.load_asset('/Game/CultUlhu/Projectiles/Visceral20261127/'+name);assert mesh
    actor=sub.spawn_actor_from_class(unreal.StaticMeshActor,origin);created.append(actor)
    component=actor.static_mesh_component;component.set_static_mesh(mesh);component.set_material(0,surface)
    camera_position=origin+unreal.Vector(160,-190,120)
    camera=sub.spawn_actor_from_class(unreal.SceneCapture2D,camera_position,unreal.MathLibrary.find_look_at_rotation(camera_position,origin));created.append(camera)
    capture=camera.get_component_by_class(unreal.SceneCaptureComponent2D)
    target=unreal.RenderingLibrary.create_render_target2d(world,512,512,unreal.TextureRenderTargetFormat.RTF_RGBA8)
    capture.texture_target=target;capture.capture_source=unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR
    capture.primitive_render_mode=unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST
    capture.show_only_actor_components(actor)
    capture.set_editor_property('capture_every_frame',False)
    for location,intensity in [(unreal.Vector(120,-100,160),800),(unreal.Vector(-90,100,100),400)]:
        light=sub.spawn_actor_from_class(unreal.PointLight,origin+location);created.append(light)
        light.get_component_by_class(unreal.PointLightComponent).set_intensity(intensity);light.get_component_by_class(unreal.PointLightComponent).set_attenuation_radius(1500)
    pp=capture.post_process_settings;pp.set_editor_property('override_auto_exposure_method',True);pp.set_editor_property('auto_exposure_method',unreal.AutoExposureMethod.AEM_MANUAL)
    pp.set_editor_property('override_auto_exposure_bias',True);pp.set_editor_property('auto_exposure_bias',0)
    pp.set_editor_property('override_bloom_intensity',True);pp.set_editor_property('bloom_intensity',0)
    capture.post_process_settings=pp
    unreal.AutomationLibrary.finish_loading_before_screenshot()
    return capture,target,mesh

names=['SM_AcidGlob','SM_ChitinDart'];state={'index':0,'frame':0}
def tick(delta):
    try:
        if state['index']>=len(names):
            unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor();return
        name=names[state['index']]
        if state['frame']==0:state['capture'],state['target'],state['mesh']=make_stage(name)
        state['frame']+=1
        if state['frame']==20:state['capture'].capture_scene()
        if state['frame']>=23:
            unreal.RenderingLibrary.export_render_target(world,state['target'],str(directory),name+'.png')
            unreal.log('CULT_PROJECTILE_CAPTURE '+name+' '+str(state['mesh'].get_bounding_box()))
            for item in reversed(created):sub.destroy_actor(item)
            created.clear();state['index']+=1;state['frame']=0
    except Exception:
        unreal.unregister_slate_post_tick_callback(handle);unreal.SystemLibrary.quit_editor();raise
handle=unreal.register_slate_post_tick_callback(tick)
