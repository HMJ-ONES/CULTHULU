# Destruction pass (UE)

Free-roam buildings you can smash. Walk to the razed city, **left-click** —
columns, walls, towers, crypts, arches and obelisks break apart into flying
debris + dust, with a shockwave ring and a camera kick. A rubble mound stays
where each building stood.

## How it works

- `ACultDestructible` (`unreal/Source/CultUlhu/…/CultDestructible.*`) — one
  actor per breakable building. HP scales with mesh size (columns fall in one
  smash, towers take several). `ApplyDamage` → at 0 HP `Collapse()` hides the
  intact mesh, spawns a rubble mound (`SM_rubble-large`), 8 ballistic debris
  chunks (engine cube + the building's own material), and a dust burst.
- `ACultBlastFX` (`CultBlastFX.*`) — short-lived impact effects, spawned via
  `ACultBlastFX::SpawnBlast(World, Kind, Location, RadiusM)`:
  `ShockRing` (smash) and `DustBurst` (collapse). Built from
  `/Engine/BasicShapes` sphere + the FX materials below; fades by shrinking,
  destroys itself when done.
- `ACultUlhuPlayerPawn::Smash()` — bound to **left mouse** (0.9 s cooldown).
  Damages every destructible within ~1.25× your entity's height, with linear
  falloff. Camera kick is a manual decaying pitch/yaw nudge (no
  `UCameraShakeBase` subclass, so nothing version-specific to break).
- FX materials `M_FX_Dust` / `M_FX_Flash` (unlit) are created by
  `build_fx_materials()` in `BuildDarkWilds.py` under
  `/Game/CultUlhu/Materials`. The C++ degrades gracefully if they are missing.
- `BuildDarkWilds.py` spawns the destructibles: placements whose model is in
  `DESTRUCTIBLE_MODELS` become `ACultDestructible` actors (labelled
  `WILD_Destructible_*`) instead of instanced statics. If the C++ module is
  not compiled yet, the class won't resolve and the script leaves those
  buildings as instanced statics — **compile first, then run BuildAll.py**.

## Test it

1. `git pull origin game-core` + `python assets/decode_assets.py`
2. Open the project, accept the C++ rebuild prompt.
3. Tools → Execute Python Script → `unreal/Tools/BuildAll.py`
4. Press Play on `L_MainMenu` → pick Free Roam → any character → ENTER THE WILDS
5. Walk to the razed city (mid-map stone ruins), left-click near buildings.

## Tuning

- Smash damage/radius/cooldown: `ACultUlhuPlayerPawn::Smash()` (135 dmg,
  1.25× height radius, 0.9 s cooldown).
- Building HP: `ACultDestructible::BeginPlay()` (`50 + radiusM × 18`,
  × `HealthMultiplier_` per actor).
- Which buildings break: `DESTRUCTIBLE_MODELS` in `BuildDarkWilds.py`.
- Debris count/life: `ACultDestructible::SpawnDebris()` / `Tick()`.

## Not done (honest)

- No Chaos/geometry-collection fracture — collapse is mesh-hide + rubble +
  debris chunks (cheap, low-spec-host friendly, and it reads well at kaiju
  scale).
- No replication yet — smashing is local-only in this pass.
- The smash is a test tool. Kit abilities (Q/F/R/RMB) are not bridged into UE
  yet; wiring real abilities to damage buildings is the combat-bridge work.
