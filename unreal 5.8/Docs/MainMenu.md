# Main Menu → Character Select → Play (in-UE flow)

One-time setup, then the loop. Everything UI is pure C++ (no Widget
Blueprints to author); the two maps are created by a script.

## One-time setup (in order)

1. **Git Bash** (repo root, `game-core` branch):
   ```
   git pull origin game-core
   python assets/decode_assets.py
   ```
2. **Compile** the new C++ in Visual Studio / the editor
   (`CultUlhuGameInstance`, `CultHomeScreenWidget`,
   `CultCharacterSelectWidget`, `CultCharacterCardWidget`,
   `CultUlhuPlayGameMode`, `CultUlhuPlayerPawn`). A normal editor
   launch auto-detects the new files; Live Coding or a full rebuild
   both work.
3. **UE editor**, Tools → Execute Python Script:
   1. `unreal 5.8/Tools/BuildMainMenu.py` — creates `/Game/Maps/L_MainMenu`
      and `/Game/Maps/L_DarkWilds` (skips any that already exist).
   2. `unreal 5.8/Tools/BuildDarkWilds.py` — builds the whole world into
      `L_DarkWilds` (terrain, props, beasts, lighting, 3 player starts).
4. Press **Play** (PIE). `Config/DefaultEngine.ini` already points the
   game at `L_MainMenu` with the new GameInstance + PlayGameMode.

## The flow

- **Home screen**: CULT-ULHU title, three mode buttons —
  Free Roam, Domination (5v5), Defense of the Great Old Ones (5v5).
- **Character select**: 20 cards (name + flavor, 5×4 grid), click to
  inspect, ENTER THE WILDS to play. Back returns home.
- **In world**: you spawn at the mode's player start
  (Free Roam → cavern overlook, Domination → city forum,
  DOTGOO → swamp edge) as the selected entity, scaled to its canonical
  kaiju height (20–30 m). WASD/arrows move (camera-relative),
  mouse looks, Space jumps, **V toggles first/third person**,
  Esc returns to the menu.

## Controls (no input assets required)

| Key | Action |
|---|---|
| W A S D / arrows | Move (camera-relative) |
| Mouse | Look |
| Space | Jump |
| V | First/third person camera |
| Esc | Back to home screen |

## Notes / honest limits

- The pawn is a scaled capsule until character FBX models land
  (Docs/CharacterPipeline.md) — movement and camera are fully testable now.
- Mode selection records intent and picks the spawn; Domination/DOTGOO
  *rules* (capture points, lanes, bots) light up as the UE sim bridge
  grows. The C++ core already simulates them headless (`match` driver).
- Chaos destruction of buildings is not in this pass — the world,
  spawns, and cameras are.
- After any `git pull`: rerun `BuildDarkWilds.py` (it rebuilds
  `L_DarkWilds` idempotently). `BuildMainMenu.py` only needs one run ever.
