# Shared humanoid rendering

Civilian, hooded cultist and magus bodies originally rendered six separate
poseable skeletal components each. Their original source pieces are now merged
once per skin in memory and shared between actors. This retains the existing
activity sampler, body scale, orientation, foot offset and accessory attachments.
MOBA minion bodies use the same shared meshes. No duplicate baked model or
download is required. The original six assets remain the recovery/fallback path.

`cult.Population.MergeHumanMeshes 0` selects the modular population comparison;
`1` is the default shared path. Existing population actors refresh when this
changes. Minions select their rendering path when created. The cache resets at
the start of an Editor Play session, so reimported source art is picked up.

Before accepting a merge, the helper checks the source and output vertex and
triangle totals, rest vertex positions, primary UVs, material references,
reference-bone names/transforms and aggregate bounds. Morph-target meshes keep
the original modular path. Engine mesh merging handles skin-weight remapping;
the geometry checks alone do not establish pixel-identical animation.

The original animation tests additionally compare real native actor poses with
the original clips across six activities and all three skins, requiring exactly
one certified skinned component. Corpse tests compare actual rendered components
and check persistent, nonblocking poses. Logical human-part counts remain six.

`Tools/PrepareHumanMeshCpuData.py` enables CPU access on the 18 original source
mesh LODs for packaged merging. Their unmodified recovery bytes were verified
against GitHub checkpoint `df222ec893aee3e716eb8d046f9667aa35db3621` before saving.
The importer runs this preparation after future human-art imports. The CPU copy
has a memory cost; it is confined to these small original humanoid pieces.

## Editor measurement

`HumanMeshEditorResults.json` records four 120-frame windows from the same
evolving Free Roam world: modular, merged, merged, modular. Population stays 191;
war, beliefs, construction and AI remain active. The skinned-component snapshot
falls from 1,146 to 191. Paired average frame time falls from 74.239 to 53.193 ms
(28.3%); end-of-frame updates fall from about 17.43 to 8.98 ms (48.5%). This is an
offscreen Editor fixture, not a foreground FPS or human combat-quality claim.
Per-window p95s, count metrics and raw CSV hashes are included in that JSON.

The remaining frame time is still inadequate for the requested responsive
combat. Movement/collision cost, clustered decisions, environmental art,
navigation and human playtesting remain separate unresolved work. Editor tests
cannot prove cooked CPU buffers survive packaging; independent executable
verification is required before claiming packaged support.

## Final Editor regression record

ArmyCommands, EditorInputSafety and MobaMatch pass in the first combined batch.
Fresh-session reruns pass MergedHumanPoses, PopulationMotion and
PopulationPresentation, including persistent nonblocking corpse components.
MobaMechanics passes after waiting for an actual first wave and asserting complete
one-component bodies on the observed lane minions. Real tower shots and minion
throws still pass. The inspection render was viewed; it remains visibly prototype
art. Log scopes and SHA-256s, including the failed intermediate runs, are retained
in HumanMeshEditorResults.json. No standalone success is implied by these checks.
