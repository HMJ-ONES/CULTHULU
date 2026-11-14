"""Record verified consolidation evidence and prepare an explicit push manifest."""
from pathlib import Path
import csv,hashlib,json,re,statistics,subprocess
project=Path(__file__).resolve().parents[1]; root=project.parent
saved=project/'Saved/Consolidation20261027'; docs=project/'Docs'
baseline='8bf27e971313758ac2f3f7c239d6d90906670897'
read=lambda path:Path(path).read_text(encoding='utf-8-sig',errors='replace')
attempts=[]; latest={}; combined=''
for name in ['NativeTests.log','NativeFocused.log']:
 text=read(saved/name); combined+='\n'+text
 results=[dict(result=a,test=b) for a,b in re.findall(r'Test Completed\. Result=\{(\w+)\}.*?Path=\{([^}]+)\}',text)]
 attempts.append(dict(log=name,results=results,automation_errors=text.count('LogAutomationController: Error:')))
 latest.update({r['test']:r for r in results})
assert len(latest)==11 and all(r['result']=='Success' for r in latest.values()),'Native checks incomplete'
assert attempts[-1]['automation_errors']==0
assert '100% tests passed out of 109' in read(saved/'CoreTests.log')
assert '100% tests passed out of 18' in read(saved/'CanonicalCoreBuild.log')
assert 'Result: Succeeded' in read(saved/'EditorBuildVerified.log')
assert '0 failure(s)' in read(saved/'BindingValidation.log')
audit=json.loads(read(saved/'AssetAudit.json')); startup=json.loads(read(saved/'Startup.json'))
for entry in audit['characters']:
 if entry['kind']=='skeletal': entry['used_triangles']=sum(int(n) for entity,n in re.findall(r'MODEL_SKELETAL_TRIANGLES (\w+) (\d+) LOD0',combined) if entity==entry['id'])
 elif entry['kind']=='static': entry['used_triangles']=entry['used_static_triangles']
 else: entry['used_triangles']=None
profile=[]; keys=['FrameTime','GPUTime','GameThreadTime','RenderThreadTime']
for index in range(4):
 path=saved/f'LODProfile/Cthulhu_{index}.csv'; samples=[]
 with path.open(encoding='utf-8-sig',newline='') as stream:
  for row in csv.DictReader(stream):
   try:
    values={key:float(row[key]) for key in keys}
    if values['FrameTime']>0: samples.append(values)
   except (ValueError,TypeError,KeyError): pass
 assert len(samples)>=100,'Insufficient actual render frames'
 profile.append(dict(segment=index,lod=1 if index in [1,2] else 0,frames=len(samples),means={k:statistics.mean(s[k] for s in samples) for k in keys}))
means={str(lod):{key:statistics.mean(w['means'][key] for w in profile if w['lod']==lod) for key in keys} for lod in [0,1]}
records=[]; immutable=[]
for record in subprocess.check_output(['git','ls-tree','-r','-z',baseline],cwd=root).split(b'\0'):
 if not record: continue
 metadata,name=record.split(b'\t',1); relative=name.decode(); blob=metadata.decode().split()[2]
 if relative.startswith('unreal 5.8/Content/'):
  p=root/relative; assert p.is_file(),relative
  data=p.read_bytes(); actual=hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
  assert actual==blob,'Existing Content changed: '+relative
  immutable.append(relative)
 # Choose only reviewed changed text; do not stage unrelated working files.
 allowed=relative.startswith('unreal 5.8/Docs/') or relative in [
  '.gitignore','AGENTS.md','CMakeLists.txt','README.md','tools/gh_push.py',
  'src/driver/AntiCheat.cpp','src/driver/main.cpp','src/fuzz/fuzz_main.cpp','src/fuzz/fuzz_util.cpp',
  'tests/tests_anticheat.cpp','tests/tests_wave19.cpp','tests/tests_micro_slaypos.cpp',
  'unreal 5.8/README.md','unreal 5.8/Tests/validate_binding.py','unreal 5.8/Tools/ValidateCultUlhu.py',
  'unreal 5.8/Tools/BuildAll.py','unreal 5.8/Tools/BuildMainMenu.py',
  'unreal 5.8/Source/CultUlhu/Private/CultUlhuPlayerController.cpp',
  'unreal 5.8/Source/CultUlhu/Public/CultUlhuPlayerController.h',
  'unreal 5.8/Source/CultUlhu/Private/CultUlhuPlayerPawn.cpp',
  'unreal 5.8/Source/CultUlhu/Public/CultUlhuPlayerPawn.h',
  'unreal 5.8/Source/CultUlhu/Private/CultCharacterModelTest.cpp',
  'unreal 5.8/Source/CultUlhu/Private/CultCameraFramingTest.cpp',
  'unreal 5.8/Source/CultUlhuCore/CultUlhuCore.Build.cs']
 if allowed and (root/relative).is_file():
  data=(root/relative).read_bytes()
  if hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()!=blob: records.append(relative)
report=dict(date='2026-10-27',baseline=baseline,core_tests_passed=109,canonical_core_suites_passed=18,editor_build='Success',binding_validation='0 failures',native_latest=list(latest.values()),native_attempts=attempts,content_files_byte_identical=len(immutable),startup=startup,characters=audit['characters'],lod_profile=dict(scope='One unchanged Cthulhu model and camera in the offscreen Editor: LOD0/LOD1/LOD1/LOD0, 120 actual frames per window. Existing LODs are unchanged. Not full-match or low-spec FPS certification.',windows=profile,paired_means=means),new_map=audit['default_map'],retired_backup_count=14,duplicate_editor_target_retired=True,duplicate_local_launchers_archived=3,remaining_limits=['No two-machine multiplayer test','No human fun/balance or low-spec certification','No new models, materials, textures, or balancing','NewMap retained; unreferenced but emptiness not established'])
(docs/'ConsolidationResults_2026-10-27.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
report['clean_source_proof']=json.loads(read(saved/'CleanCoreSourceProof.json'))
report['cleanup']=json.loads(read(saved/'Cleanup.json'))
(docs/'ConsolidationResults_2026-10-27.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
lines=['# Consolidation and reliability review — 2026-10-27','',
 'The candidate inherits the tested PR #2 and #3 chain at `8bf27e9` and is intended as one PR into `game-core`. Existing local work was preserved; the checkout was not reset. Muse’s prompt was reviewed as advice, not a new authority over the owner’s decisions.','',
 '## Changes and decisions','',
 '- Wire all eleven optional Enhanced Input actions through the existing pawn actions. Shared press/release routing prevents duplicate activation alongside zero-asset keys; movement and Shift use the current movement and tap/hold decisions.',
 '- Repair the stale binding validator: explicit canonical path, Windows separators, UTF-8 documentation and inline field mappings. The project validator now detects duplicate launch descriptors and target classes.',
 '- Retire fourteen source backups from the proposed Git tree, preserving local copies under Saved. Retire the duplicate nested editor target; the canonical root target remains. Three old local launch descriptors were archived without deleting their source or art.',
 '- Correct current setup and control documentation. BuildAll preserves existing authored maps; all five map files are byte-identical after its real editor run.',
 '- Fix Windows build/test portability: executable source separation, compiler options, POSIX console/fuzz helpers, anti-cheat directory creation, test asset working directories and path separators. Anti-cheat thresholds, combat logic, kits, beliefs and scoring are unchanged.',
 '- Update the stale death-position fixture to the existing typed death-event contract: real civilians still emit a positioned CivilianSlain event; monsters and avatars do not incorrectly award civilian kills. Positive monster and PvP position coverage remains.',
 '- Preserve the sixteen-character roster, deferred character data, existing credits and all Content. Eight previously approved models are the existing CC-BY exception; Sghllor is original procedural art. No new rights decision or LFS migration is needed for this text/source pass.','',
 '## Verification','',
 '- Full clean core source export plus reviewed patches: decode assets first, Visual Studio 17 2022 x64 Release build, then CTest: **109/109 pass**.',
 '- Exact canonical MSVC core helper: **18/18 pass**, library staged at `unreal 5.8/lib/Win64/cultulhu.lib`.',
 '- Canonical CultUlhuEditor Win64 Development build: **Success**. Directory/target validator and offline binding validator pass.',
 '- **11 distinct native checks have passing final results**: Enhanced Input and fallback, S+LMB/ALT+S safety, source models, four camera cases, actual menu callback flow, DominationMatch, MobaMatch and the LOD profile.',
 '- The first movement fixture ran during protected match preparation and failed. It was corrected to wait for CanAct, retaining its movement assertion; real Enhanced input then advanced the pawn 665.1 cm. A profiling future-type compile error was repaired. First-person captures were held through rendering to avoid capturing a camera transition.',
 f'- All **{len(immutable)} inherited Content files** compare byte-for-byte with the prior checkpoint, including credits and maps. No new gameplay assets or triangle increases.',
 '- Reviewed native captures: [third-person source model](ConsolidationEvidence/CthulhuThirdPerson.png) and [settled first person](ConsolidationEvidence/CthulhuFirstPerson.png).',
 '- Removed 181,027,606 bytes of temporary clean verification source/build output after retaining its hashes and results. Canonical working builds and preserved source backups remain.','',
 'Commands are reproducible with the installed UE Python, `Tools/BuildCanonicalCore.ps1 -Test`, `Build.bat CultUlhuEditor Win64 Development -Project=<canonical uproject> -WaitMutex -NoHotReloadFromIDE -NoUBA -MaxParallelActions=1`, `Tools/VerifyConsolidationStartup.py`, and the native test names recorded in [the machine-readable report](ConsolidationResults_2026-10-27.json). Native runs use `-DisablePlugins=ModelContextProtocol` only on their command line; project configuration remains unchanged.','',
 '## Actual render LODs','',
 '| Entity | Active LOD | Triangles |','| --- | ---: | ---: |']
for row in audit['characters']: lines.append(f"| {row['id']} | {row['render_lod'] if row['kind']!='procedural' else 'procedural'} | {row['used_triangles'] if row['used_triangles'] is not None else 'engine sphere and mist planes'} |")
lines += ['',f"The existing Cthulhu LOD1 measured GPU means of **{means['1']['GPUTime']:.3f} ms**, versus **{means['0']['GPUTime']:.3f} ms** at LOD0 in this isolated A-B-B-A editor comparison. Whole-frame means were {means['1']['FrameTime']:.3f} / {means['0']['FrameTime']:.3f} ms. This pass did not change the LODs; these are bounded measurements, not a game-wide improvement or a low-spec guarantee.",'',
 '## Preserved limits','',
 'NewMap remains in the candidate: the asset registry reports no referencers, but emptiness was not established, so it was not removed. Two-machine networking, human fun/balance, full-match sustained performance, remaining model licensing/acquisition gaps and artist rigging are outside this reliability pass. No branch history was rewritten and PR #1 was not changed.']
(docs/'ConsolidationReview_2026-10-27.md').write_text('\n'.join(lines)+'\n',encoding='utf-8')
new=['Source/CultUlhu/Private/CultEnhancedInputTest.cpp','Source/CultUlhu/Private/CultConsolidationFlowTest.cpp','Source/CultUlhu/Private/CultModelLODProfileTest.cpp','Tools/AuditConsolidationAssets.py','Tools/VerifyConsolidationStartup.py','Tools/WriteConsolidationReport.py','Docs/ConsolidationReview_2026-10-27.md','Docs/ConsolidationResults_2026-10-27.json']
records += ['unreal 5.8/'+p for p in new]
records += [str(p.relative_to(root)).replace('\\','/') for p in (docs/'ConsolidationEvidence').glob('*.png')]
records=sorted(set(records)); assert all((root/p).is_file() for p in records)
(saved/'PushManifest.json').write_text(json.dumps(records,indent=2)+'\n',encoding='utf-8')
print(json.dumps(dict(native=len(latest),core=109,content_unchanged=len(immutable),files=len(records),bytes=sum((root/p).stat().st_size for p in records))))
