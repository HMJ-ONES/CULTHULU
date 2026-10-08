# Art import & performance (UE5)

How the `assets/world/` art gets into the Unreal project, and how to
keep the host's PC happy. Written for the owner to follow in-editor;
nothing here is compile-verified in this repo (UE5 is not installed
here — see the `// VERIFY IN EDITOR:` markers in `unreal/Source/`).

## Importing the art

1. In the Content Browser, create the folder structure mirroring the
   repo (keeps paths predictable for the `ModelCatalog` wiring):
   - `/Game/CultUlhu/World/Terrain`
   - `/Game/CultUlhu/World/Buildings`
   - `/Game/CultUlhu/World/Props`
   - `/Game/CultUlhu/World/Cult`
2. Drag the FBX/GLB files from `assets/world/<category>/` into the
   matching folder. On the import dialog:
   - **Uncheck "Auto Generate Collision"** for small props (tombstones,
     rubble, braziers) — collision comes from simple box/sphere
     colliders you add, or none at all for pure decoration.
   - **Check "Generate LODs"** (or add LODs later via the Static Mesh
     editor): buildings get 3 LODs, small props get 1–2. Screen sizes
     roughly 1.0 / 0.5 / 0.15.
   - Materials: the packs are vertex-colored or carry tiny (≤512 px)
     textures, so the auto-created materials are fine. For the dark
     Lovecraftian grade, apply a single shared **darkening material
     instance** (desaturated, low albedo) across ruin pieces rather than
     per-asset materials — fewer shaders, consistent mood.
3. The ruined-city layout lives in `assets/maps/ruined_city.map`
   (x y z, rotY, scale, zone per prop). Recreate it in-editor with a
   simple placement pass, or write a one-off Editor Utility script that
   reads the `.map` file and spawns the meshes — the format is plain
   text precisely so this stays easy.

## Instancing: the single biggest win

The map reuses the same prop dozens of times (wall segments, tombstones,
braziers, dead trees). **Never place these as individual Static Mesh
Actors.** Instead:

- For each repeated prop, create an **Instanced Static Mesh (ISM)**
  component (or a `UInstancedStaticMeshComponent` in the map-builder
  actor) and add one instance per placement from the `.map` file.
- Result: one draw call per prop *type* instead of one per prop.
  ~100 props across ~25 types ≈ ~25 draw calls for the whole set
  dressing — trivial for any host PC.
- Unique hero pieces (the altar, collapsed tower) can stay individual
  actors — they need per-instance logic anyway (altar rituals reference
  the altar actor).

## LOD & culling guidance

- Buildings: 3 LODs (screen size 1.0 / 0.5 / 0.15). Props: 1–2 LODs.
- Small props (< 1 m): consider dropping to the lowest LOD aggressively
  — at distance they are silhouette only.
- Dungeon/cave interiors: put a **Precomputed Visibility Volume** over
  each dungeon and keep interiors in sublevels streamed by proximity to
  the entrance (see README "Optimization levers", item 8).
- Keep Nanite OFF for instanced small props (Nanite + ISM is a bad
  combination); reserve it for hero sculpts if any are added later.

## If the host still struggles (later)

Work through the README's "Optimization levers" list in order —
instancing and LODs (this doc) come first because they are pure content
pipeline wins with zero gameplay impact.
