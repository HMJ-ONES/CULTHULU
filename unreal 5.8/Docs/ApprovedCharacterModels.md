# Approved character model integration

Only exact approved sources are used. Current release: sixteen choices; the four previously deferred characters remain excluded. Unavailable models retain the working placeholder and original gameplay kit.

[Simple next steps and remeshing list](CharacterModels_NextSteps.md) · [Attribution](../Content/CharacterModels/CREDITS.md) · [Machine-readable results](ApprovedCharacterModelResults.json)

| Character | Source URL | Acquired | License verified | Type | Imported into Unreal | Visible in selection | Gameplay verified | Remaining issue |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| bokrug | [Selected source](https://sketchfab.com/3d-models/lizard-fe08fb17e7104a339433c689b1e91af5) | NO | NO | UNAVAILABLE | BLOCKED_ACQUISITION | Placeholder | Placeholder | Source page offers no download; obtain creator-authorized file and game redistribution license |
| cthulhu_avatar | [Selected source](https://sketchfab.com/3d-models/cthulhu-lovecraft-lore-d42fb260fa1546d09df367143520ccbd) | YES | YES | STATIC | YES | YES | NATIVE CHECK | Static source: rig and create idle, locomotion and attack animation; Retopologize/remesh and bake detail; source has 1,532,947 triangles; Review source forward axis, silhouette and foot placement in Unreal |
| dagon | [Selected source](https://sketchfab.com/3d-models/dagon-lovecraft-lore-2d96cd1308de4e2e8f0b1413e26a5448) | YES | YES | STATIC | YES | YES | NATIVE CHECK | Static source: rig and create idle, locomotion and attack animation; Retopologize/remesh and bake detail; source has 1,095,375 triangles; Review source forward axis, silhouette and foot placement in Unreal |
| ghatanothoa | Not approved | NO | NO | UNAVAILABLE | NO_APPROVED_MODEL | Placeholder | Placeholder | No final approved model for this task |
| hastur | [Selected source](https://sketchfab.com/3d-models/the-king-in-yellow-hastur-0d568df99cc64396877641b303e2c882) | NO | NO | UNAVAILABLE | BLOCKED_ACQUISITION | Placeholder | Placeholder | Source page offers no download; obtain creator-authorized file and game redistribution license |
| mi_go | [Selected source](https://sketchfab.com/3d-models/insectoid-monster-rig-01323e4b2563430f9da85cd255b6e176) | YES | YES | ANIMATED | YES | YES | NATIVE CHECK | Supplied Prowl transition retained and visually grounded; author seamless idle/run and original-kit attacks; Refine fungal growth and wings as desired |
| mother_hydra | Not approved | NO | NO | UNAVAILABLE | NO_APPROVED_MODEL | Placeholder | Placeholder | No final approved model for this task |
| nodens | [Selected source](https://sketchfab.com/3d-models/chariot-95baa95c8b9b4f4299de0e0609f7307c) | YES | YES | STATIC | YES | YES | NATIVE CHECK | Static source: rig and create idle, locomotion and attack animation; Review source forward axis, silhouette and foot placement in Unreal; Horse mesh Cylinder001__0 excluded; chariot body, wheels and shafts retained |
| nug_and_yeb | [Selected source](https://sketchfab.com/3d-models/floating-horror-54a063d8810540feb1906b27316a6d53) | NO | NO | UNAVAILABLE | BLOCKED_ACQUISITION | Placeholder | Placeholder | Source page offers no download; obtain creator-authorized file and game redistribution license, then create two differentiated floating instances |
| nyarlathotep | [Selected source](https://sketchfab.com/3d-models/lovecraftian-e640af2877d84d7d9ab14369dcd36d18) | YES | YES | STATIC | YES | YES | NATIVE CHECK | Static source: rig and create idle, locomotion and attack animation; Retopologize/remesh and bake detail; source has 648,390 triangles; Review source forward axis, silhouette and foot placement in Unreal |
| rhan_tegoth | [Selected source](https://sketchfab.com/3d-models/maw-of-the-void-free-creature-asset-402848d2da654330bc9c3f23ec06cf8d) | NO | YES; underlying rights BLOCKED | UNAVAILABLE | BLOCKED_RIGHTS | Placeholder | Placeholder | CC-BY uploader identifies this as fan art based on Darksiders Genesis concept art; underlying rights need clearance |
| sghllor | Original concept | ORIGINAL | YES | PROCEDURAL | YES | YES | NATIVE CHECK | Refine the original light and mist art after reviewing in play |
| shoggoths | [Selected source](https://sketchfab.com/3d-models/lovecraftian-horror-6093cbb205444e9aa7b36d7be4ed9b58) | YES | YES | STATIC | YES | YES | NATIVE CHECK | Static source: rig and create idle, locomotion and attack animation; Review source forward axis, silhouette and foot placement in Unreal |
| shub_niggurath | [Selected source](https://sketchfab.com/3d-models/dark-young-overlord-rigged-animated-3c4975d1bf604c0f9fe91623634a4f5c) | NO | YES; underlying rights BLOCKED | UNAVAILABLE | BLOCKED_RIGHTS | Placeholder | Placeholder | CC-BY uploader identifies the creature as the Overlord anime Dark Young from season 3; underlying rights need clearance |
| yig | [Selected source](https://sketchfab.com/3d-models/naga-dragon-serpent-stylized-fantasy-monster-2fb96bd7a3c543eeabfb3868107ed4a4) | YES | YES | ANIMATED | YES | YES | NATIVE CHECK | Original FBX idle timeline retained; author separate locomotion, attack, hit and death clips; Review sword placement and silhouette at canonical height |
| yog_sothoth | [Selected source](https://sketchfab.com/3d-models/yog-sothoth-lovecraft-lore-bdf73725a4494ff8b7981685d52f73ab) | YES | YES | STATIC | YES | YES | NATIVE CHECK | Static source: rig and create idle, locomotion and attack animation; Retopologize/remesh and bake detail; source has 1,516,875 triangles; Review source forward axis, silhouette and foot placement in Unreal |

## Validation

- CultUlhu.Art.ApprovedCharacterModels: **Success**
- CultUlhu.Menu.CharacterSelectionCapture: **Success**
- CultUlhu.Menu.CharacterSelectionLayout: **Success**
- CultUlhu.Menu.DominationMatch: **Success**
- CultUlhu.Menu.MobaMatch: **Success**

Automation errors: 0. Runtime character art: 138 assets, 205,208,681 bytes; largest asset 6,170,163 bytes.

The native checks inspect actual imported assets, render source models, enter a real match, and verify height, capsule collision, camera visibility and persistent corpse geometry. Screenshots require visual inspection too; a component existing is not proof of a good-looking model. These checks are not a human fun/balance review or low-spec performance certification.

Dense static sculptures use generated 10% render LODs while retaining LOD0 and original downloads. This is a practical interim reduction, not artist retopology or a promise of final deformation quality. Source clips are retained; missing idle/run/attack clips are not invented. Yig uses the original FBX Scene timeline for idle/preview, with original sword transforms preserved on rigid bones. Mi-Go uses the original Blender PoseLib action exported as Prowl, with visual grounding independent of gameplay collision.

Only the source-hash-owned imports are edited. Runtime model collision is disabled; canonical capsules, character heights, original kits, damage, movement and physical skill targeting remain in the existing pawn. The new model actor is hidden in human first person and reinstated in third person. Nodens uses the chariot body, wheels and shafts; the separate horse is omitted from the binding.

Raw source GLBs and local receipts stay in Downloads/Saved. Git LFS is available but this repository currently has no LFS tracking rules; the reviewed runtime asset sizes fit ordinary Git and the binary-safe push protocol. Avoid repeated sculpture revisions in normal Git: migrate large ongoing art work to LFS or a source-asset archive before routine iteration.
