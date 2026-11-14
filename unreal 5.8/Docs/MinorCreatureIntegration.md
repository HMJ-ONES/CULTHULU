> Updated by [CanonicalGameplayIntegration.md](CanonicalGameplayIntegration.md): Risen Dead is retired from live encounters, character HP comes from the original kit, and the forward-facing offset is corrected.

# Imported minor enemies

The creature update assembles the existing modular Content assets into moving
minor enemies. No source art is reimported, replaced, or edited.

| Enemy | Skeletal parts | Height | Health | Swipe damage |
| --- | ---: | ---: | ---: | ---: |
| Charnel Imp | 9 | 1.62 m | 35 | 5 |
| Skittering Ghoul | 8 | 1.98 m | 40 | 6 |
| Pale Wight | 7 | 2.25 m | 45 | 7 |
| Risen Dead | 6 | 1.80 m | 40 | 5 |
| Ossified Brute | 9 | 3.24 m | 50 | 10 |

Damage is against the existing 1000-health player. All minor enemies die from
even the weakest normal in-range Smash hit (54 damage). Attack cooldown is 1.8 s.
Attacks require proximity, line of sight, and a reachable player foot height;
jumping above their reach avoids a swipe.

## Presentation and movement

One poseable skeletal component drives the creature's pose. Modular body parts
follow its bone transforms, retaining the imported materials. Hood, cape, hat,
and helmet static accessories retain their imported model-space placement.
Model bounds normalize the assembled body to the species height, preserving the
player's much larger canonical height. Authored creature geometry replaces the
previous sphere stand-ins, which remain only as a missing-asset fallback.

The KayKit creature clips were stripped during original asset preparation.
Procedural local bone animation therefore supplies leg strides, arm swings, and
swipes. Death uses the existing tip-over/sink effect. This is a basic animation
pass, not authored animation blending, IK, ragdolls, or full navigation.

Capsule collision handles actor movement. Ground queries select static geometry,
not other creatures, and reject steep surfaces or large height discontinuities.
Far-away enemies update at 2 Hz with accumulated time. Species ids are now
serialized so editor-built enemy actors retain their species after saving.

Each of the three runtime encounter packs includes all five species. The world
builder also supports native skeletal assemblies without requiring a nonexistent
single static creature mesh at the species folder root.

## Validation and deployment

Offline validation confirmed all 39 skeletal body-part assets exist and the
world-builder Python parses. Read-only MCP inspection verified the ghoul and
wight rigs and the Risen Dead bone names.

`CultUlhu.MinorCreatures` checks imported body-part completeness, damage tuning,
one-hit deaths, actual movement, and changing skeletal leg transforms. It must be
run after compiling the update, alongside `CultUlhu.GroundSpawn`.

The Development Editor build succeeded. Both `CultUlhu.GroundSpawn` and
`CultUlhu.MinorCreatures` passed in an actual Dark Wilds game world on 2026-10-16.
All 39 parts loaded; the ghoul moved 384.4 cm with changing leg transforms.
Results: `Saved/Logs/MinorCreaturesVerified.log`.

An in-editor visual check confirmed the assembled imported bodies and caught
double-applied static accessory offsets, which were corrected before delivery.
Unity compilation is disabled for the game module because existing UI source
files reuse anonymous-namespace helper names.
