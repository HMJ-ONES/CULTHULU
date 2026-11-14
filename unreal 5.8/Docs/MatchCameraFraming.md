# Match camera framing

The initial Mi-Go crosshair hit an allied hero in both Domination and MOBA.
All five vessels spawned at the same forward distance, with the local player
at the left end of the row. The right-shoulder camera aimed into that row.
The four-case native baseline reproduced this for Mi-Go; Cthulhu's baseline
ray hit world geometry instead. This was not a universal camera failure.

An intermediate mirrored row cleared that allied ray hit, but its rendered
Domination frame showed a temple column directly in front of the camera.
That passing ray-only check was insufficient evidence for a clear spawn view.

SpawnFighter now places the lead vessel at the opposite end of the row,
shifts the row inward by 15 metres, and mirrors it for the opposing team.
The camera faces the open side, inside the temple's flank columns.
Forward distance, row spacing, entity scale, capsule dimensions and camera
collision are preserved. Respawns use the same SpawnFighter code path.

CultUlhu.Menu.CameraFraming tests Mi-Go and Cthulhu in both competitive modes.
Each enters through the real menu flow and checks the actual camera aim ray
after the world starts, also rejects solid hits behind the player's body plane,
switches to first person and back, checks unchanged
scale/capsule height, then spawns a solid wall between the camera and pivot.
The camera must actually retract against that wall. No collision is disabled.

This is a bounded spawn-view improvement. It does not establish clear views
for every roster entry, every camera angle, crowded live battles or Free Roam.
It also does not replace the placeholder vessel art. CameraFramingResults.json
records the baseline failures and final native regression results/log hashes.
These edits have not been cooked into a fresh independent executable.

The final canonical Editor build succeeded. All seven final native checks
passed with zero automation errors: the four camera cases, EditorInputSafety,
DominationMatch and MobaMatch. The latter checks retain actual objective play,
physical routes, assigned lane entry, damage and competitive mode rules.

The final Domination and MOBA introductory renders were viewed. The near
column is now to the right of the crosshair; allied placeholder bodies still
occupy much of the lower-left view and arena cover remains visible ahead.
These ordinary editor screenshots are 1288x680, not exact-size HUD captures.

Storage: this pass adds C++ and small text evidence only. No content downloads,
duplicate project, cooked package or standalone executable is created. Keep
the working editor output locally; source and durable evidence belong on the
existing GitHub review branch. Generated logs remain excluded from GitHub.
