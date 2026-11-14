#Latest combat-feel update: [Responsive melee, enemy recovery, and readable combat feedback](CombatFeel.md).

 Initial beliefs, entity preaching, and ALT armies

The menu flow is Mode → Entity → Choose three initial beliefs → World. All twenty entities get a fresh picker each time. Entry requires three distinct choices from the twelve source beliefs. The picker includes each belief's original rules and explains **B** changes beliefs with an adoption delay (normally 120 game seconds; Chaos accelerates adoption). Player choices apply before the world starts its faith simulation. Bot cults keep their own doctrines.

## Selecting and commanding

Hold **ALT** to release the cursor. A cyan ground footprint previews the 10m nearby-group selection area. Click a friendly cultist to select visible owned followers within that area; for precision, drag a rectangle over followers instead. Selected followers get cyan markers. Clicking an enemy retains your friendly selection and any prepared army. Dragging a rectangle over enemies also picks an enemy target without replacing the prepared friendly group. Orders appear in a two-column panel. Keep ALT held while choosing a button; release ALT to return to movement and aiming.

**Preach / recruit nearby** is always present. The entity preaches at its own location, regardless of the clicked command point. Living non-captive civilian listeners must be within 45m and visible through unobstructed geometry. Recruits actually change faction, become registered cultists, and follow their entity. The entity emits a visible sermon ring and reports audience/recruit counts.

- Adopted **Conversion**, ready, with at least three listeners: exactly three local civilians join. Uses the original `MASS_CONVERT_COUNT=3` and 200s cooldown, and its one canonical three-soul reward receipt.
- Otherwise, an ordinary sermon can convince **at most one** civilian. It uses the original ambient sermon chance (`0.25 × conversionEfficiency`), with a modest ×1.5 terror bonus while the cult is in combat, capped at 75%. Most civilians remain unconverted. The entity sermon has a 20s recharge; an ordinary listener cannot be rerolled for 60s. No listeners means no recharge and no reward.
- The source Conversion belief still increases campaign efficiency and rival-conversion vulnerability. Sermons and real conversions feed existing belief events; no invented ability replaces any entity's Q/F/R kit. The sermon radius, action recharge, listener retry time, and raid modifier are native usability/balance choices for this request.

**Attack / Raze** with no prepared group sends every currently owned available cultist, regardless of loyalty. Existing player battle groups keep their assignments. Captives, dead people, and followers that have changed faction cannot be commanded. There is no six-unit cap, temple distance exclusion, obedience roll, or four-operation cap for player combat orders.

For a separate front:

1. ALT + drag over the followers you want.
2. Choose **Prepare for battle**.
3. ALT + click the enemy or city and choose **Attack** or **Raze city at target**.
4. Select another friendly group and repeat. Selecting a different friendly group clears the previous preparation snapshot while leaving its battle assignment intact.

Preparation is a snapshot of recipients, not an order to fight yet. Its button becomes **Disengage battle preparation**. Toggling preparation off releases that recipient filter for future orders; it does not recall existing fronts. If prepared followers die or leave the cult, an empty prepared group never silently falls back to the whole army.

Player battle assignments persist without the old 60s expiry. Ambient prayer, rest, rituals, and society jobs cannot overwrite them. Units remain near their assigned front when their original target is gone; they do not acquire enemies across the entire map. There is no group-count limit imposed by the command system.

**Disengage selected group** recalls only the selected followers. An explicit new order (including Build when accepted) also replaces those followers' combat assignment. A refused build order leaves their prior assignment intact. **GLOBAL RETREAT** releases every combat group and preparation and recalls followers to their entity; retreating units do not stop to acquire nearby enemies. **Clear selection** removes only the selection markers, preserving preparation and existing battles.

**Build** uses only the selected crew, or available unassigned followers when nothing is selected. It checks actual loyalty (source BuilderAI threshold 40), mental/cult state, and the source command obedience preview for Reconstruction-aligned construction. Each refusing follower is counted in the notice: “cultists are disobeying (loyalty / beliefs).” Construction needs real ground, actual travel, and workers at the site. Previous construction sites do not borrow workers reassigned to another building or battle. A site with no remaining assigned builders cancels, releasing the AI construction slot; it cannot permanently block future construction.

## Verification

`CultUlhu.ArmyCommands` / `CultUlhu.Menu.ArmyCommands` exercise initial-choice enforcement, low-loyalty battle obedience, separate fronts beyond the core four-operation limit, persistent assignments, preparation toggling, independent disengagement, build refusal feedback, global retreat, local recruitment, exact mass-conversion accounting, and sermon cooldowns. `CultUlhu.Menu.InitialBeliefs` opens a fresh picker for every roster entity and captures the rendered screen. Validation results are recorded after running the rebuilt project.

The army system issues real movement/combat orders through the existing population AI. Its existing terrain steering and navigation limitations still apply; this change does not replace navigation with a new pathfinder. Corpses and battle assignments are session state and are not saved across closing a match.

## Verified run — 17 October 2026

- Native editor build succeeded.
- All eight game suites passed together in `Saved/Logs/ArmyCommandsFinal.log`: ArmyCommands, Beliefs, CanonicalGameplay, GroundSpawn, LivingWorld, MinorCreatures, SkillCombat, Society.
- The army test assigned eleven separate fronts, checked zero-loyalty combat obedience, protected old assignments, verified independent/global recall, and recruited exactly three local listeners through one mass-conversion receipt.
- The 130-second society observation recorded seven torture completions, four sacrifices, twelve prayers, nine conversions, one completed construction, and twenty-three destroyed city buildings. Three real war kills had already occurred before the observation window. Final faction strength was x1.49 / x1.76 / x1.46.
- Regression work also corrected test isolation (restored pre-existing orders and wars) and moved the physical collision fixture above procedural terrain. The construction failure exposed abandoned sites consuming the AI's two-site limit; abandoned work now cancels correctly instead of counting toward that limit indefinitely.

- The connected Unreal MCP editor checks `CultUlhu.Menu.ArmyCommands` and `CultUlhu.Menu.InitialBeliefs` both passed with zero automation errors or warnings. The picker test checked fresh choices for all twenty roster entities.
- Rendered screenshots were inspected: `Saved/Screenshots/InitialBeliefs.png` and `Saved/Screenshots/ArmyCommands.png`. Final presentation uses readable runtime fonts, wider command rows, single-line belief labels, and a compact entry button. The full eight-suite game run preceded these text/layout-only refinements; the two editor checks were rerun on the final UI build.
