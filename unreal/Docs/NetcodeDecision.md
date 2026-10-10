# Netcode Decision — UE replication supersedes the socket layer (for the UE5 build)

## Decision

**Use Unreal's built-in replication + listen servers for all UE5
multiplayer.** Our custom socket layer (`src/net`: `Socket`, `RadminNet`,
`Discovery`, `Lobby`, `Netcode`) is **not** ported into the UE build.

Rationale:

1. **Listen server == our player-hosted model.** The wave-6 design is
   "no dedicated server; the host runs the match on their machine."
   That is exactly a UE listen server (`?listen`). No new concept to learn.
2. **Replication replaces `Netcode` snapshots.** Host-authoritative actor
   replication (`bReplicates`, `Replicated`/`ReplicatedUsing` properties,
   Server/Client RPCs) does what our 20 Hz snapshot + 30 Hz input poll did,
   with dormancy, relevancy, and prediction-ready movement for free.
3. **Radmin still does the hard part.** UE has no NAT traversal on the
   NULL subsystem; Radmin's virtual LAN (26.x.x.x) gives us a flat LAN
   where `open 26.12.34.56` just works. Our Radmin adapter/IP detection
   and setup guide remain the connection UX.
4. **Less custom code to maintain.** The socket layer was the right call
   for the headless core (testable without an engine); inside UE5 it would
   fight the engine.

## What survives from `src/net`

| src/net piece | UE5 fate |
|---|---|
| Host-authoritative sim, 30 Hz inputs / 20 Hz snapshots | Replaced by replication. Sim ticks on host only (`HasAuthority()` gates in actors/GameMode). |
| `Discovery` UDP beacons | **Port the design, not the code.** Options: (a) keep it simple — players share their Radmin IP out-of-band (Discord) and `open` it; (b) reimplement the beacon as a UE UDP socket subsystem for an in-game server browser. (a) first, (b) later. |
| `Lobby` (ready-up, 5v5 teams, chat) | Reimplement as UMG + `AGameModeBase` lobby state (or UE sessions with NULL subsystem). Team assignment/KDA rows map 1:1. |
| `PlayerStatsTracker` / `KdaRow` / `PlayerKda` message | Core tracker stays; replicate rows via a `UCultMatchState`-style actor or `AGameStateBase` subclass (`Replicated` TArray). KDA broadcast at 1 Hz (same as `kKdaHz`). |
| Radmin setup guide | Still the onboarding doc; link it from the in-game host screen. |
| Socket layer as reference | `src/net` remains the **headless-tested reference implementation** — CI still builds and tests it; the UE netcode is validated against its behavior (host authority, KDA rules, lobby flow). |

## Replication map (initial)

| Gameplay data | Mechanism |
|---|---|
| Entity transform/hp | `ACultUlhuEntityActor`: `SetReplicatingMovement` or manual transform sync; `ReplicatedHp` (`ReplicatedUsing`) |
| Character vitals/stamina | `ACultUlhuCharacter`: `ReplicatedHp`, `ReplicatedStamina` |
| Belief gauges / power / insurrection | `AGameStateBase` subclass, `Replicated` floats, 4–5 Hz |
| KDA table | Same game state, `Replicated` array of structs, 1 Hz |
| Directive/command issues | Server RPCs from the owning client (`Server_IssueDirective`) |
| Wave of Domination victims | Host-owned: victim positions replicated via the victim actors; levitation is visual on clients |

## Listen-server session over Radmin (player flow)

1. Host: in-game **Host** button → `ACultUlhuGameMode::HostListenServer(Map)`
   (opens `<Map>?listen` on port 7777).
2. Host reads their Radmin IP (`myip` equivalent — shown on the host screen;
   the core `RadminNet` adapter detection can feed it).
3. Clients: connect Radmin to the host's network, then in-game **Join** →
   console `open 26.x.x.x` (or a Join button with an IP field).
4. Lobby map handles ready-up + 5v5 teams; `ServerTravel` to the match map.

// VERIFY IN EDITOR: full loop — two editor instances (one `-server`?
// no: PIE listen server + second client `open 127.0.0.1`) then over real
// Radmin addresses. Confirm replication of vitals/KDA and Server RPCs.

## Non-goals

- No dedicated server build this wave (the design explicitly avoids one).
- No cross-network NAT traversal (Radmin is the transport story).
