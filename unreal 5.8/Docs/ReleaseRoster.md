# Current release roster

Hypnos (`hypnos`), Bast (`bast`), the Howling Eye (`howling_eye`), and the Elder Mind (`elder_mind`) are deferred to future updates. Their original `assets/characters/<id>/` folders, definitions, and roster metadata remain in the repository. None is selectable or loadable in the current Unreal game, including manually configured bots and stale selections. The selector falls back to Cthulhu when opened with an unavailable selection; direct attempts to enter play return to selection. Unavailable pawn configurations destroy themselves before kit initialization.

The shared allowlist is `Source/CultUlhu/Public/CultReleaseRoster.inl`. It contains sixteen current entities and is used by selection, kit loading, and UnrealBuildTool staging. Only those sixteen original `character.def` files are staged as UFS runtime dependencies under `Content/CharacterDefinitions/<id>/`. Packaged builds read these staged files, never the external development character folder. No original character file has been removed or changed.

To release a deferred character later, add its ID to the allowlist, rebuild, and rerun `CultUlhu.ReleaseRoster` after updating the expected release roster in that test. Its retained metadata and original definition will become available together. Any new art assets for deferred characters must also be kept outside the release cook until that update.

`CultUlhu.ReleaseRoster` checks the count, uniqueness, all retained kits, staged definitions against original source, rejection of all four deferred IDs, absence from staged release definitions, and preservation of the deferred originals. The rendered projectile/dash test now uses Yog-Sothoth's canonical Gate Lash / Between Spaces instead of spawning Elder Mind.

## Verification — 17 October 2026

- Native editor build succeeded. Selection uses a 4 by 4 grid.
- `CultUlhu.ReleaseRoster`, `CultUlhu.Menu.InitialBeliefs`, and `CultUlhu.Menu.SkillPreview` all passed in `Saved/Logs/ReleaseRosterEditor.log`.
- The editor build receipt contains exactly sixteen CharacterDefinitions runtime dependencies, all UFS. Every staged definition matches its original file byte for byte. All four deferred originals exist and none has a staged definition.
- Release staging configuration was verified; a full shipping package was not produced or published during this change.
- Pre-change source backup: `Saved/Backups/BeforeReleaseRoster.zip`. No original character files were deleted.
