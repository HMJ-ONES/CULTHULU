> Legacy implementation reference. Use [the current UE 5.8 runbook](IntegrationGuide.md) for setup; existing authored maps and native input need no regeneration.

# Dark Wilds — in-editor runbook

The stretched-cube ground is gone. The dark wilds are real terrain now:
a 1.2 km × 1.2 km heightfield mesh with hills, a massive open cavern,
cave mouths, three clumped razed-city complexes, a dead forest and a
murky swamp — built from repo data by a script, in about a minute.

## The world (all visible after the script runs)

| Zone | What you see |
|---|---|
| `highlands` | Rolling wind-scoured hills, tall rocks, dead trunks, bare dead trees, scattered bones |
| `cavern_approach` | Five natural rock arches marching to the bowl rim, stalactites under two arches, four cave mouths (black lightless openings) facing outward |
| `deep_cavern` | A massive sunken bowl (~400 m across, 26 m deep): stalagmite fields, dark crystal clusters, rim rocks. Stars die here — near-black fog |
| `razed_city` | Three clumped ruined complexes on a flattened plateau: the fallen forum (colonnade ring + broken obelisk), the shattered ward (wall ring + two ruined towers + rubble), the drowned gate (gate arches + ruined crypts) |
| `drywood` | Dead forest: bare dead trees, crooked dead pines, ash thickets, bone piles, ribcages |
| `dead_swamp` | Low basin with a dark water sheet, reed clusters, fallen trunks, mist banks, root tangles — and 24 feral beasts (ghouls, imps, wights, risen dead); 6 more in the cavern |

Lighting: cold moonlight + faint blue skylight + violet-black height fog.
Four reflection captures make the ground and the swamp water reflect —
that was the missing piece in your level (a lit material with no
captures in the scene reflects nothing).

## Build it (first time)

1. **Git Bash:** `cd` to your clone, then
   `git pull origin game-core`
   `python assets/decode_assets.py`  (restores the terrain/prop binaries)
2. **Editor:** open your persistent level (or make a new empty one).
3. **Tools → Execute Python Script…**, pick
   `<repo>/unreal 5.8/Tools/BuildDarkWilds.py`. Watch the Output Log —
   every section prints `[OK]`, `[SKIP]` or `[ERROR]`.
4. **PIE** and walk. The spawn point: put your PlayerStart near
   `(15000, 0, 800)` (razed-city plateau, centimeters) for the money view,
   or `(-30000, -40000, -2000)` at the cavern rim.

First run imports ~40 assets, so give it a minute. If a prop prints
`[SKIP] no static mesh`, its GLB didn't import — the world still builds;
paste the log lines back and we'll fix that one asset.

## See later changes (the sync model, honestly)

**Nothing syncs automatically.** The editor never talks to GitHub —
there is no auto-pull, no background sync. The loop is:

1. **Git Bash:** `git pull origin game-core`
2. **If C++ changed** (`unreal 5.8/Source/**` or `src/**`): recompile.
   In the editor: **Build → Build Solution**, or **Ctrl+Alt+F11**
   (Live Coding) for small changes without closing the editor.
   If `src/**` changed, rebuild `cultulhu.lib` first (IntegrationGuide §1)
   — the UE build won't do it for you.
3. **If world data changed** (`.map`, terrain, textures, props):
   **no recompile** — just re-run `BuildDarkWilds.py`. It is idempotent:
   it re-imports changed assets and rebuilds the level from the current
   files. ~30 seconds.
4. If new binaries arrived: `python assets/decode_assets.py` before step 3.

That script re-run is the closest thing to "automatically changes" —
it exists precisely so world iteration never means hand-placing props.

## Notes

- True enclosed caves/underground interiors are sublevels in this
  architecture (the Vale of Pnath pattern, IntegrationGuide §9) — the
  cave mouths mark where those stream in. The deep cavern is the
  massive open-air version you can walk today.
- Feral beasts spawn as static display meshes for now; wiring them as
  live actors is the CharacterPipeline step (same as any creature).
- Per-zone fog volumes aren't built yet — one global violet-black fog
  covers the level; the per-zone atmosphere numbers in the `.map`
  are the brief for that pass.
- To regenerate the world data itself (new seed/layout):
  `python3 assets/world/generate_dark_wilds.py` — deterministic,
  stdlib only, runs on your Windows Python too.
