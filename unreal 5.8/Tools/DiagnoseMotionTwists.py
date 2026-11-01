"""Read-only diagnostic of joint rotation discontinuities in saved motion studies."""
from pathlib import Path
import sys
import json
import math
import bpy

assert bpy.app.background
ROOT = Path(__file__).resolve().parents[2]
source = ROOT / sys.argv[sys.argv.index('--')+1]
bpy.ops.wm.open_mainfile(filepath=str(source), use_scripts=False)
rig = next(o for o in bpy.context.scene.objects if o.type == 'ARMATURE')
result = {}
for action in bpy.data.actions:
    role = action.get('CultUlhu.MotionRole')
    if not role:
        continue
    rig.animation_data.action = action
    if action.slots:
        rig.animation_data.action_slot = action.slots[0]
    previous = None
    steps = []
    for frame in range(int(action.frame_range[0]), int(action.frame_range[1])+1):
        bpy.context.scene.frame_set(frame)
        poses = {b.name: b.matrix.to_quaternion() for b in rig.pose.bones}
        if previous:
            for name, q in poses.items():
                angle = math.degrees(previous[name].rotation_difference(q).angle)
                angle = min(angle, 360-angle)
                steps.append({'bone': name, 'frame': frame, 'degrees': angle})
        previous = poses
    result[role] = sorted(steps, key=lambda r: -r['degrees'])[:5]
print('MOTION_TWISTS', json.dumps(result), flush=True)
