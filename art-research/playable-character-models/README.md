# CULT-ULHU — playable character model review (October 2026)

**Research target reached: 20 playable entities × 3 individually identified candidate model listings = 60 review links (60 distinct URLs).** This is a **research shortlist**, not imported 3D assets. Models remain on their creator's sites, linked below. No model binaries, screenshots, rigging, animations, or licenses were copied into the game or this repository.

## How to review

1. Open the individual character folder below and follow each candidate's link to inspect its interactive 3D preview where supported. Two Elder Mind links point to indexed Thingiverse listings via STLFinder because direct original pages were not recovered.
2. Compare silhouette and lore fidelity before judging texturing, topology and animation support.
3. Treat **CC BY** as requiring attribution, **CC0** as public domain, **CC BY-NC** as **NOT approved for a commercial game**, and licensed models of third-party entertainment IP as requiring additional rights review regardless of what Sketchfab says.
4. A source offering download **does not imply a rig or animations**. Most lore-accurate sculpts are high-poly static meshes requiring retopology and rigging.
5. Model ownership, permitted commercial use, included textures, file types, provenance and download functionality must be rechecked on the download date.
6. **NoAI tags** should be respected: do not feed those models into generative-AI training or generation tools; ordinary licensed 3D use and Blender cleanup should be assessed separately.
7. All 60 review slots now link to individual model listings, **but this does not certify all 60 as suitable, directly downloadable, verified, or commercial-ready**. Some are far-from-canon placeholders, some are static STL/sculpts, and two Elder Mind options are index pages for free Thingiverse models whose original pages need locating.

## Playable roster / review folders
- [Bast, Queen of the Cats of Ulthar](./bast/CANDIDATES.md)
- [Bokrug, the Great Water Lizard](./bokrug/CANDIDATES.md)
- [Cthulhu, the Dreaming God](./cthulhu_avatar/CANDIDATES.md)
- [Dagon, the Deep Father](./dagon/CANDIDATES.md)
- [The Elder Mind, Protector of the Yith](./elder_mind/CANDIDATES.md)
- [Ghatanothoa, Lord of the Volcano](./ghatanothoa/CANDIDATES.md)
- [Hastur, the King in Yellow](./hastur/CANDIDATES.md)
- [The Howling Eye, Heart of the Polyp Scourge](./howling_eye/CANDIDATES.md)
- [Hypnos, Lord of Sleep](./hypnos/CANDIDATES.md)
- [The Mi-Go, Fungi from Yuggoth](./mi_go/CANDIDATES.md)
- [Mother Hydra, the Deep Mother](./mother_hydra/CANDIDATES.md)
- [Nodens, Lord of the Great Abyss](./nodens/CANDIDATES.md)
- [Nug and Yeb, the Twin Blasphemies](./nug_and_yeb/CANDIDATES.md)
- [Nyarlathotep, the Crawling Chaos](./nyarlathotep/CANDIDATES.md)
- [Rhan-Tegoth, Terror of the Museum](./rhan_tegoth/CANDIDATES.md)
- [Sghllor, the Colour Out of Space](./sghllor/CANDIDATES.md)
- [The Shoggoths, the Shapeless Ones](./shoggoths/CANDIDATES.md)
- [Shub-Niggurath, the Black Goat](./shub_niggurath/CANDIDATES.md)
- [Yig, Father of Serpents](./yig/CANDIDATES.md)
- [Yog-Sothoth, the All-in-One](./yog_sothoth/CANDIDATES.md)

### Review record
Mark a selection on the relevant entity page only *after* viewing the source. The active Unreal project is `unreal 5.8`; no character asset should be imported there until selected.

**Source of roster:** `unreal 5.8/Source/CultUlhu/Private/CultUlhuGameInstance.cpp` at branch `game-core`.

**STATUS (2026-10-22): 60/60 candidate listing slots filled.** This means **research leads only**, not 60 high-fidelity, rigged, commercially licensed, verified models. In particular:

- **Licensing risk:** Hastur's King in Yellow model is CC BY-NC and cannot be placed in a commercial release without separate permission. Quake Shub-Niggurath and Overlord Dark Young are derived from third-party franchise designs even though their upload pages display CC BY; treat as visual references only until rights are cleared.
- **Animations:** Only select entries explicitly mention rigs/animation, for example animated serpent attacks, Crabulb idle/walk, Shoggoth flesh pulsation, sea monster swimming, and the third-party-IP Dark Young. Everything else needs checking.
- **Lore fidelity:** Exact entity sculptures are marked in individual notes; adaptable bases are not implied to be canonical. Rare creatures, especially Elder Mind, Nug and Yeb, Howling Eye and Sghllor, will need serious redesign.
- **Link provenance:** The two Elder Mind Thingiverse suggestions use STLFinder's individual indexed-model pages rather than their original Thingiverse pages. Their free-listing metadata is a research lead, not proof that the downloads or commercial-use permissions are currently available.
- **No binary 3D models were downloaded or committed**. Selecting, licensing, remeshing, and importing assets into Unreal are separate steps.

**User's saved inspiration:** [The Human Deer and Mountain Dragon](./SAVED_FOR_LATER.md). These remain outside the 60.

Please mark the strongest **one** candidate per character before importing assets. The final source and license should be saved with any chosen model.
