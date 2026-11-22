# Bokrug head camera, forward targeting and RMB recharge — 2026-11-22

- Bokrug first person follows his imported `head_0285` / `head_end_0632` bones. The camera keeps its control rotation and probes nearby world geometry. His body is excluded only from his own first-person rendered view. Third-person framing is wider and hides self-meshes when they obstruct the camera corridor. Other players retain the normal visible model and collision.
- Combat uses the screen-centre forward ray starting at the caster's launch plane, so cover between the shoulder camera and the character cannot redirect aiming backwards. Front-facing world geometry still blocks targeting and projectiles. ALT cursor selection retains its existing ray.
- All native RMB recharge times are multiplied by 0.65: 35% shorter. Cthulhu's rock is 6.5s; his DOTGOO minion throw is 13s; his legacy wave recharge is 4.55s. Dropping a held rock still spends no cooldown. Active wave, hold, windup and damage timings retain their existing behavior.
- Server commit notifications and selection-screen metadata use the new timers. V switches first and third person.

Validation: canonical validator and actual CultUlhuEditor Win64 Development build passed; actual head position, per-view hiding, rear-wall rejection and front-wall blocking passed in all three modes. Existing camera-safety and Mi-Go/Cthulhu framing checks passed. All 16 released characters passed the live DOTGOO primary/RMB/Q/F/R checks, including exact shorter RMB timer assertions. Selection layout and minion-throw integration passed. Separate local host/client processes passed with Bokrug hosting and Mi-Go joining, including combat, creep movement, local leaving and bot replacement.

Core validation: 17 of 18 suites passed, including the new wave7d scaled-recharge case. The legacy wave25 kit-content/balance suite still reports seven failures involving accepted effect kinds, cooldown bounds and raw kit DPS. That suite and the implicated kit data were not changed to force a pass. This update therefore does not claim a completely green core suite.

Evidence is local under `Saved/Logs/HeadCamera*.log`, `Saved/Screenshots/BokrugHeadCamera_*.png`, `Saved/Automation/Room2_*.json`, and `Intermediate/EngineCore/Testing/Temporary/LastTest.log`.

The core header/library must remain synchronized: run `Tools/BuildCanonicalCore.ps1` before the canonical editor build when rebuilding from source. The engine-compatible core library and editor DLLs have already been rebuilt locally. Generated libraries, binaries and test output are excluded from this GitHub backup.
