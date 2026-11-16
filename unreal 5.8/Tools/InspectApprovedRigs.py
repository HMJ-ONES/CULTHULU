"""Read approved skeleton tracks and local source poses without modifying assets."""
from pathlib import Path
import unreal,json
project=Path(__file__).resolve().parents[1]
assert project.name=="unreal 5.8" and (project/"CultUlhu.uproject").is_file()
def collect():
 registry=json.loads((project/"Content/CharacterModels/registry.json").read_text(encoding="utf-8-sig"))
 rows=[]
 for key in ["mi_go","yig"]:
  entry=registry["characters"][key]
  for record in entry["clips"]:
   anim=unreal.load_asset(record["asset"])
   names=unreal.AnimationLibrary.get_animation_track_names(anim)
   row={"id":key,"clip":record["asset"],"length":anim.get_play_length(),"bones":[str(n) for n in names],"poses":{},"paths":{}}
   for name in names:
    row["paths"][str(name)]=[str(n) for n in unreal.AnimationLibrary.find_bone_path_to_root(anim,name)]
    try:
     pose=unreal.AnimationLibrary.get_bone_pose_for_time(anim,name,0,False)
     row["poses"][str(name)]={"location":[pose.translation.x,pose.translation.y,pose.translation.z],"rotation":[pose.rotation.x,pose.rotation.y,pose.rotation.z,pose.rotation.w],"scale":[pose.scale3d.x,pose.scale3d.y,pose.scale3d.z]}
    except Exception as ex: row["pose_error"]=str(ex)
   rows.append(row)
 (project/"Saved/CombatTactics20261030").mkdir(parents=True,exist_ok=True)
 (project/"Saved/CombatTactics20261030/PresentationAudit.json").write_text(json.dumps(rows,indent=2),encoding="utf-8")
 return rows
if __name__=="__main__":
 collect()
 unreal.SystemLibrary.quit_editor()
