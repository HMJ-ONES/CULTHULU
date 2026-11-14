#Latest update: [Initial beliefs, entity preaching, and ALT armies](ArmyCommands.md). Player combat orders bypass loyalty and preserve separate fronts; building still checks loyalty and doctrine.

 Living cult society

Latest command/menu update: [Initial beliefs, entity preaching, and ALT armies](ArmyCommands.md). Player combat orders now bypass loyalty and preserve separate fronts; building still checks loyalty and doctrine.

Latest combat, corpse, movement, PvE health and HUD changes: [SkillCombatReview.md](SkillCombatReview.md).

This pass populates the existing world with temple residents, civilians, prisoners, outreach groups, and rival garrisons. They are actual actors with health, allegiance, movement, and consequences, using the faith system described in `BeliefsReview.md`.

## What to watch

- Your temple is southwest of the initial central spawn, near world coordinates **−122m, −126m**. Its low courtyard altar is a visible work area beside the monumental temple. Each faction has one.
- Temple residents pray, read omens, share dreams, rest, and tend followers through the original ambient action system. Prayer and rites have distinct poses.
- A temple worker approaches a real prisoner, performs a 20-second interrogation, and earns that person's per-victim torture reward once. A separate **30-second source sacrifice** can follow. Its victim actually dies. Damage, a changed order, lost participants, or a destroyed altar can interrupt the work.
- Once prisoners are depleted, a free cultist can seek a real nearby civilian, capture them, and escort them home. Starting prisoners are actual civilian actors; spawning them grants no rewards.
- Outreach followers start near settlements, so recruitment and military activity occur locally. Cults with War/Onslaught or an existing war send expeditions into combat or raids; peaceful outreach converts civilians. Rival garrisons allow AI factions to fight each other. The autonomous society does not automatically declare war on the human.
- Construction has a rising, nonblocking preview and visible worker progress. Completion creates the real destructible structure, clears crew from its new collision, and releases them for subsequent work.
- Raiders target building surfaces. They no longer need to reach a building's blocked central point to damage it.

Temple staff are protected from distant wholesale military reassignment so worship, rituals, recruitment, construction, and fighting can happen at once. Explicit player orders remain available through ALT. A follower's loyalty, captivity, lunacy, interruption, and available targets constrain work.

## Identify actions and results

People show their owning cult, activity, health, work progress, and short-lived contribution receipts. Activity text uses a dedicated unlit font material; courtyard lighting illuminates the original models even in temple shadow. Violet indicates prayer/rites, amber indicates labor/interrogation, cyan indicates preaching/conversion, and red indicates military activity. Their movement and poses change with the action. Faction badges provide a further allegiance cue.

The **LIVING CULTS** overlay lists each faction's ongoing work, power, and actual combat multiplier. The last player-cult contribution is shown directly. **B** opens the detailed belief panel and history; hold **ALT** to click its controls.

Receipts use an observer on the original `ExertionSystem` power pipeline. They report the actual clamped resource change, including stacked-belief dilution, synergies, losses, and the 1000-power cap. They do not award an additional copy of the reward. An inactive belief may receive fervor without direct power income; a `+0.00 power` receipt with belief names is intentional. Adopt beliefs in B to favor the activities you want rewarded.

Strength continues to follow the original GitHub `DerivedStats` formula: War and Onslaught fervor each contribute `0.004 × fervor`, Magic contributes `0.002 × fervor`, and hot Blood Frenzy adds `0.15` to combat strength. Native entity kits and cultist attacks consume this faction-specific multiplier. Conversion and devotional work also change loyalty and recruitment success through the original formula. Power is not converted into an invented blanket damage bonus.

**Explicit data extension requested by this population pass:** a completed real construction now feeds **+2 Reconstruction fervor**, at the usual active/inactive/conflict rates, through a new `BuildCompleted` row in `ActionExertionTable.h`. Existing base power rewards, source character abilities, and other exertion feeds are unchanged.

## Original art and animation

The original repository packages `assets/characters/cultist_hooded`, `cultist_magus`, and `civilian_villager` are decoded/imported by `Tools/ImportCultPeople.py` into `/Game/CultUlhu/People`. They are the repository's CC0 KayKit NPC packages, including their original textures and modular body parts. Unused weapon attachments are excluded from the staged import; source packages remain untouched.

Native poseable components play the repository's `.canim` tracks for walking, work, channeling, casting, attacks, sacrifice performer/victim, stun, and fear. The original clip format specifies offsets from each rig's rest pose. Parts share the leader pose, preserving the authored skin. A basic articulated fallback is retained for missing art; it is not a new character kit.

## Binding policies and limits

The population pass attempts eight additional temple residents, eight pilgrims, three prisoners, and four local expedition followers per faction, plus a small rival garrison. Ground/capsule checks can reject unsafe positions. Initial prisoners and expeditions represent an already inhabited world; rewards still require completed actions. The broader settlement, creature, and civilian replenishment systems remain active.

Interrogation presentation lasts 20 seconds; its reward uses the existing Torture events. Society planning considers free, living, noncaptive, nonlunatic workers with loyalty at least 40 every five seconds. These are native scheduling policies, not new abilities. Original ritual durations remain 30/45 seconds.

This remains a local simulation with prototype navigation and session-only state. Existing advanced mimic, training, volunteer-selection, and full save-game gaps remain documented in `BeliefsReview.md`.

## Verification

The canonical build runs 18 suites, including construction feeds and actual clamped contribution receipts. `CultUlhu.Society` observes ordinary game-world behavior for 130 seconds without fabricating success events. The editor counterpart `CultUlhu.Menu.Society` also captures `Saved/Screenshots/CultSocietyWork.png` and `CultSocietyRitual.png`. The canonical 18 suites passed; the belief-binding suite reports 161 checks and zero failures. Native Beliefs, CanonicalGameplay, GroundSpawn, LivingWorld, and MinorCreatures also passed.

The first long game run exposed an expedition bug: unsafe city spawning and conversion/raid movement could prevent combat. City groups now probe safe neighboring positions and prioritize an actual combat target over their noncombat destination. A fresh editor run passed with initial war kills zero and these **130-second observed completions**: 8 torture events, 6 sacrifices, 21 prayers, 13 conversions, 4 constructions, 3 enemy cultist kills, and 19 city buildings destroyed. Final combat multipliers were x1.10 / x1.69 / x1.49. See `Saved/Logs/SocietyEditor.log`.

Original people were verified loaded with six articulated skeletal parts each. The first visual review exposed dark labels and courtyard lighting; dedicated unlit text, smaller captions, local warm fill lights, and regular rendering for the tiny imported cape/hat/staff meshes address that readability issue. A second fresh 130-second editor run also passed: 9 torture events, 6 sacrifices, 24 prayers, 12 conversions, 4 constructions, 3 war kills, and 24 destroyed city buildings; strength x1.10 / x1.75 / x1.50. See `Saved/Logs/SocietyFinalEditor.log`.

A final short visual smoke run verified the corrected emissive text connection (the vertex-color output is unnamed in UE), warm lighting, and textured people after disabling Nanite for the five tiny static accessories. This smoke run was intentionally stopped after the screenshot review; it is separate from the two completed long observation passes. PIE is stopped and the editor is at the main menu level, ready for Play. `CultSocietyWork.png` shows the final readable presentation.

Prototype-navigation warnings can still occur around dense settlement collision. This pass verifies successful work and combat, not a production navigation mesh or persistent save system.
