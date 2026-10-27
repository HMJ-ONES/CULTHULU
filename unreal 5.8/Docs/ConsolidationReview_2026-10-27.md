# Consolidation and reliability review — 2026-10-27

The candidate inherits the tested PR #2 and #3 chain at `8bf27e9` and is intended as one PR into `game-core`. Existing local work was preserved; the checkout was not reset. Muse’s prompt was reviewed as advice, not a new authority over the owner’s decisions.

## Changes and decisions

- Wire all eleven optional Enhanced Input actions through the existing pawn actions. Shared press/release routing prevents duplicate activation alongside zero-asset keys; movement and Shift use the current movement and tap/hold decisions.
- Repair the stale binding validator: explicit canonical path, Windows separators, UTF-8 documentation and inline field mappings. The project validator now detects duplicate launch descriptors and target classes.
- Retire fourteen source backups from the proposed Git tree, preserving local copies under Saved. Retire the duplicate nested editor target; the canonical root target remains. Three old local launch descriptors were archived without deleting their source or art.
- Correct current setup and control documentation. BuildAll preserves existing authored maps; all five map files are byte-identical after its real editor run.
- Fix Windows build/test portability: executable source separation, compiler options, POSIX console/fuzz helpers, anti-cheat directory creation, test asset working directories and path separators. Anti-cheat thresholds, combat logic, kits, beliefs and scoring are unchanged.
- Update the stale death-position fixture to the existing typed death-event contract: real civilians still emit a positioned CivilianSlain event; monsters and avatars do not incorrectly award civilian kills. Positive monster and PvP position coverage remains.
- Preserve the sixteen-character roster, deferred character data, existing credits and all Content. Eight previously approved models are the existing CC-BY exception; Sghllor is original procedural art. No new rights decision or LFS migration is needed for this text/source pass.

## Verification

- Full clean core source export plus reviewed patches: decode assets first, Visual Studio 17 2022 x64 Release build, then CTest: **109/109 pass**.
- Exact canonical MSVC core helper: **18/18 pass**, library staged at `unreal 5.8/lib/Win64/cultulhu.lib`.
- Canonical CultUlhuEditor Win64 Development build: **Success**. Directory/target validator and offline binding validator pass.
- **11 distinct native checks have passing final results**: Enhanced Input and fallback, S+LMB/ALT+S safety, source models, four camera cases, actual menu callback flow, DominationMatch, MobaMatch and the LOD profile.
- The first movement fixture ran during protected match preparation and failed. It was corrected to wait for CanAct, retaining its movement assertion; real Enhanced input then advanced the pawn 665.1 cm. A profiling future-type compile error was repaired. First-person captures were held through rendering to avoid capturing a camera transition.
- All **566 inherited Content files** compare byte-for-byte with the prior checkpoint, including credits and maps. No new gameplay assets or triangle increases.
- Reviewed native captures: [third-person source model](ConsolidationEvidence/CthulhuThirdPerson.png) and [settled first person](ConsolidationEvidence/CthulhuFirstPerson.png).
- Removed 181,027,606 bytes of temporary clean verification source/build output after retaining its hashes and results. Canonical working builds and preserved source backups remain.

Commands are reproducible with the installed UE Python, `Tools/BuildCanonicalCore.ps1 -Test`, `Build.bat CultUlhuEditor Win64 Development -Project=<canonical uproject> -WaitMutex -NoHotReloadFromIDE -NoUBA -MaxParallelActions=1`, `Tools/VerifyConsolidationStartup.py`, and the native test names recorded in [the machine-readable report](ConsolidationResults_2026-10-27.json). Native runs use `-DisablePlugins=ModelContextProtocol` only on their command line; project configuration remains unchanged.

## Actual render LODs

| Entity | Active LOD | Triangles |
| --- | ---: | ---: |
| mi_go | 0 | 13156 |
| cthulhu_avatar | 1 | 153297 |
| dagon | 1 | 109536 |
| nyarlathotep | 1 | 64837 |
| shoggoths | 0 | 5212 |
| yog_sothoth | 1 | 151688 |
| yig | 0 | 76141 |
| nodens | 0 | 2062 |
| sghllor | procedural | engine sphere and mist planes |

The existing Cthulhu LOD1 measured GPU means of **4.348 ms**, versus **6.233 ms** at LOD0 in this isolated A-B-B-A editor comparison. Whole-frame means were 18.619 / 19.578 ms. This pass did not change the LODs; these are bounded measurements, not a game-wide improvement or a low-spec guarantee.

## Preserved limits

NewMap remains in the candidate: the asset registry reports no referencers, but emptiness was not established, so it was not removed. Two-machine networking, human fun/balance, full-match sustained performance, remaining model licensing/acquisition gaps and artist rigging are outside this reliability pass. No branch history was rewritten and PR #1 was not changed.
