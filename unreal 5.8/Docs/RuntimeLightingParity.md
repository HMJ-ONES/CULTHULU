# Runtime lighting parity

## Confirmed problem

The standalone guest view was substantially darker than the host. This was not merely a different camera angle. Matched 1280×720 north/south captures with the same 75-degree camera and gray reference shapes reproduced it in DOTGOO:

| Sample | Host near-black fraction | Guest near-black fraction |
| --- | ---: | ---: |
| North-facing play area | 0.033% | 18.725% |
| South-facing play area | 0.004% | 21.176% |

The host's skylight used `SLS_SpecifiedCubemap` and the existing `DaylightAmbientCubemap`; the guest used captured-scene lighting with no cubemap. The server-only `ACultUlhuPlayGameMode::InitGame` supplied the cubemap, while the guest's arena setup changed only its intensity. Capturing the dark/disabled sky supplied almost no ambient fill on the guest.

Free Roam had a related omission: its client-side generated-world handoff did not apply `CultWildsPresentation`. Adding that presentation exposed a second difference: the guest's moon remained at pitch −30/yaw 0, whereas the host used pitch −32/yaw −35. The shared runtime setup now explicitly makes directional lights movable before changing them. The rerun verified equal moon direction and reference shading.

## Changes

- `CultSceneLighting::Ambient` supplies one visible runtime skylight, sets movable mobility, disables scene recapture and uses the existing fixed cubemap with the intended tint and lower-hemisphere fill. Additional skylights are disabled at runtime to avoid competing sources.
- Both competitive arena builders call that helper locally. They retain the host's existing 0.6 intensity and blue tint.
- The server's initial world setup and Free Roam presentation use the same helper. Free Roam retains its existing 0.55 presentation intensity.
- A Free Roam guest applies its presentation after generating the shared terrain. Its sky, exposure, fog and cave fill now follow the same presentation path as the host.
- Directional lights receive explicit runtime mobility in both competitive and Free Roam profiles.

The authored maps, source art, materials, terrain collision and combat rules are preserved. No textures or model downloads are added. The engine cubemap was already used by this project; this introduces no additional asset dependency.

## Reproduce and inspect

After validating and building the canonical `CultUlhuEditor` target, run these from the repository root with the installed Python executable:

```text
python "unreal 5.8/Tools/RunRoomNetworkProbe.py" --mode 2 --lighting
python "unreal 5.8/Tools/RunRoomNetworkProbe.py" --mode 1 --lighting
python "unreal 5.8/Tools/RunRoomNetworkProbe.py" --mode 0 --lighting
```

The opt-in diagnostic temporarily supplies a local camera, hides the local HUD and adds two nonblocking gray reference shapes. It captures two opposite views, records actual light/cubemap/exposure settings, then restores the previous view/HUD and removes its fixtures. It does not pause the simulation or change normal player input. It has no effect without `-CultRoomLighting` and is excluded when development automation is disabled.

The runner requires one matching active ambient source, matching moon direction/intensity, four gray-region host/guest mean absolute differences no greater than 4 sRGB levels out of 255, and reference median brightness of at least 15. It also retains the real movement/combat/menu/bot-handoff checks appropriate to each mode and the DOTGOO wave/ward checks. `AnalyzeMatchLighting.py` reads actual RGB/RGBA PNG pixels using the standard library, validates PNG checksums and writes sample-region statistics without an extra imaging package. Domination's later overlook work raises the diagnostic camera/references above the balconies and moving fighters, records lens/camera geometry for projected sample regions, and requires unobstructed reference rays. These diagnostic changes preserve the original brightness/difference gates; see `DominationArchitecture.md`.

Do not confuse these image statistics with calibrated radiance, beauty, complete scene coverage or fun. The regions intentionally sample the calibration shapes; actors continue moving elsewhere. These are same-computer host/guest checks, not a different-PC tunnel, adverse-network, packaged-build or low-spec performance certification.

## Remaining work

Correcting missing fill does not finish the art direction. Repeated masonry, large primitive silhouettes, route/landmark identity, camera composition around large bodies, HUD density and artist-directed animation remain open. Continue reviewing both lit and shadowed gameplay positions, including caves and tunnel exits, rather than declaring the maps polished from a single bright view.

Detailed build, pixel and gameplay evidence is recorded in `RuntimeLightingVerification_2026-10-31.json`. Local screenshots remain under `Saved/Automation`; only source, compact measurements and documentation belong in the Git backup.
