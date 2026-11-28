# Production lobby integration

Canonical project: `unreal 5.8/CultUlhu.uproject`, Unreal 5.8.3.
API: https://cultulhu-lobby.pages.dev/api/v1 — protocol 1, game version 0.1.0.
Game packets use an Unreal listen server on port 7777 over the existing Radmin
VPN adapter. Cloudflare is the directory/reservation service, not a gameplay relay.

## Source and contract audit — 2026-11-06

Read Lovable project `7d4a475d-5fd3-4f21-886a-c17270da30c7` (Project Blueprint):
`docs/UNREAL_INTEGRATION.md`, `docs/openapi.yaml`, `docs/INTEGRATION_STATUS.md`,
`src/lib/cult/{types,api,election}.ts`, and `migrations/0001_init.sql`.
The website source is in this connected Lovable project; it is not in this
checkout's `apps/lobby`. The accessible GitHub `game-core` branch is substantially
older than the local canonical project. No working files were reset to it.

Production GET `/health` returned `ok:true`, `storage:d1`, `diag.hasDB:true`, with
the `DB` binding present. GET `/config` reports protocol 1, max players 10,
heartbeat 60 seconds, stale timeout 180 seconds, reservation TTL 120 seconds.
Public listing was empty. POST `/session` and empty-mode quickplay succeeded.
Disposable **private, pending** registration and deletion succeeded; no fake
public match was advertised.

**Current blocker:** the live `/reserve` endpoint returned HTTP 500 during the
two-process test, after increasing occupied seats. The host successfully loaded
the DOTGOO map with a real network driver listening on 7777. The guest never
received a usable reservation, so native joining/replicated movement through
this API are **not verified**. Health/D1 availability alone does not prove a
working reservation service. No Cloudflare deployment or website change was made.

Actual server corrections to the supplied example:

- Region is `sa`, not `sa-east` (the latter returns HTTP 400).
- Omit an absent password; an explicit empty network password is rejected.
  Hosting UI requires the actual existing network password, 6–60 characters.
- Production lobby IDs use `lby_...`; UUIDs alone are insufficient as a parser.
- Reservation fields are `reservation.id`, `.token`, `.lobbyId`, `.expiresAt`,
  with connection details nested inside `.connection`.
- Reserving the last seat can return a summary with `joinable:false` because
  that reservation filled it. Validate compatibility without rejecting that seat.

## Blueprint / UMG interface

Get Game Instance → Get Subsystem (CultLobbySubsystem). Bind `OnStatusChanged`.
Read `State`, `Status`, `LobbyId`, `InviteCode`, `RequiredNetwork` and the
transient `RequiredNetworkPassword` for the connection panel only.

- `QuickPlay(Mode, OpenBrowser=true)` opens the requested browsing URL and
  polls every 10 seconds. Ordinary Play never starts a host.
- `JoinByCode(Input, Mode)` accepts the official web join URL, custom join URI,
  raw production lobby ID, UUID, or private invitation code. It validates mode
  and version, resolves private invitations, then reserves an actual seat.
- `ContinueRadminConnection()` runs validated native ClientTravel after the
  player joins the network displayed in the connection panel.
- `ConfigureRadminNetwork(Name, Password)` stores existing VPN credentials only
  in memory. `HostGame(Mode, Private)` registers a hidden pending lobby, loads
  the real map with listen options, and publishes only after the actual network
  driver is listening on 7777. Private invitations also gate native PreLogin.
- `SetMatchInProgress(bool)` updates the server heartbeat flag. The current API
  rejects new reservations in `in_progress` rooms, so use this deliberately when
  closing recruitment; do not set it automatically merely because bots play.
- `Cancel()` / `LeaveLobby()` stop polling, cancel pending travel, release the
  reservation or delete the hosted lobby, and invalidate late async responses.

The existing room menu now routes through this subsystem after character
selection. Practice/offline and explicit `-CultOfflineNetworking` / LAN probes
remain available. Hosting uses an explicit **Host Game** action; it must not be
described as a completed automatic best-host election.

## Network prerequisites

1. Start Radmin VPN and join/create the same network on both PCs.
2. Host enters that network's actual name/password in the masked hosting fields.
3. Select mode and entity, then host publicly or privately.
4. Friend searches or pastes the private invitation. The game displays the host's
   network details, then Connect travels to the validated 26/8 address on 7777.

No Radmin SDK/CLI is available in the inspected integration. Adapter detection
cannot establish named-network membership. The game neither invents credentials
nor claims to create/join VPN networks. Tokens are memory-only and never logged
by the subsystem. Never upload raw networking logs: Unreal's own travel logs can
contain private invitation URL options. No Windows URI handler is installed by
this client; custom URIs can be pasted into its Join field.

## Server limitations requiring backend changes

The inspected `/hosts/elect` ranks candidates but grants no atomic offer/lease.
`/hosts/accept` does not verify ownership of a selected offer. Desktop benchmarks
are required by its ranking; no trustworthy upload/reachability benchmark exists
in this game client yet. Automatic best-host election is therefore unfinished;
fabricated scores and simultaneous fallback hosts would be misleading.

Heartbeat accepts only `sessionRunning`, `inProgress`, `closing`; authoritative
server-connected player counts cannot be reported. Reservations update counts
and orderly clients release them, but confirmed reservations have no renewable
client lease. A killed client can leave a stale occupied seat until lobby closure.
Server admission should eventually validate per-player reservations rather than
relying only on the shared private invitation. Revoking an invitation requires
closing its lobby. These are limits of the shared live registry, not a second
locally maintained registry.

`CloudLobbyServerSafety.patch` is a **prepared, undeployed** patch against Lovable
commit `3fa1f58641180f4022a0db420d0f23a0289fc6c7`, `src/lib/cult/api.ts`. It decrypts
connection details before changing occupancy, compensates ordinary insert/log
failures, and rejects missing durable encryption configuration instead of using
an isolate-local random key with D1. `Tools/TestCloudLobbyServerPatch.mjs` runs
seven safety checks against the exact patched functions and reproduces the
original seat leak with injected decryption failure. Store/crypto are mocked;
this does not diagnose the actual production exception or prove D1 transaction
atomicity. Production logs are needed to distinguish missing/rotated encryption
keys, schema errors, and other causes. Do not rotate a working key blindly.

## Verification and operation

`CultUlhu.Network.CloudLobby.InputBoundary` tests trusted origins, identifier
parsing, command injection rejection, strict 26/8 addresses and canonical modes.
`Tools/RunCloudLobbyProbe.py --mode 2` explicitly opts into a private production
API test with two local Unreal processes. It verifies reservation, actual native
connection, character selection, movement replication, confirmation and cleanup.
It does not prove two-PC VPN connectivity or create a Radmin network. Redacted
results are in `Saved/Automation`; decoded builds and logs stay out of Git.

Steady API request estimates: active host 60 heartbeats/hour; searching player
360 quickplay requests/hour; join typically session + detail/resolve + reserve +
confirm + leave. Session is reused for up to 24 hours. Search failures back off
and stop after three transient retries; 429 honors numeric Retry-After up to
five minutes. Mutating registration is not blindly retried after an ambiguous
timeout. Backend SQL work depends on live lobby/candidate counts and lazy cleanup;
request counts are not a measured D1 billing estimate. This implementation adds
no paid servers, subscriptions or services; account billing was not audited.

Hard-kill/offline shutdown cannot guarantee a DELETE reaches Cloudflare. The
server's 180-second heartbeat expiry is the fallback. Finish a real two-PC VPN
playtest before describing multiplayer as end-to-end verified.

## Recorded results

- Canonical `CultUlhuEditor Win64 Development` build: passed, UE 5.8.3.
- `CultUlhu.Network.CloudLobby.InputBoundary`: passed.
- `CultUlhu.Network.RoomCodes`: passed.
- `CultUlhu.Network.RoomMenus`: passed for all three modes.
- Native subsystem startup failure found by the first real-process run: fixed
  with explicit native construction and refreshed generated-class compilation;
  subsequent menu/PIE and game startup passed.
- Real private API host: registered, loaded DOTGOO, listened on 7777 and
  heartbeated successfully. Guest reservation: failed at the production API.
- Server safety patch application check against the exported source: passed.
  Exact-function mocked safety tests: passed. Patch is not deployed.
- Two-PC Radmin play, public quickplay joining, Domination/Free Roam API joining,
  abrupt-disconnect occupancy recovery and packaged shipping build: unverified.

## Next steps requiring deployment access

1. In the **game-only** Cloudflare Pages project `cultulhu-lobby`, inspect the
   failing `/reserve` function logs (test window 2026-11-06 around 21:52 UTC).
2. Confirm `DB` and the stable `CONNECTION_ENCRYPTION_KEY` secret are configured
   in production. Keep secrets out of chat, Git and the compiled game.
3. Apply the prepared safety patch to the existing Lovable backend, run its API
   test suite, diagnose the actual exception, then deploy to the **same** Pages
   project/D1 database. Do not create a replacement registry.
4. Repeat `RunCloudLobbyProbe.py --mode 2`; then modes 1 and 0. Verify reservation,
   confirmation, leave, invitation revocation and player counts before release.
5. Play between two Windows PCs in the same Radmin network. Then address atomic
   host election and renewable occupancy/admission before advertising those
   features as complete.

Local dashboard automation could not start (`failed to write kernel assets:
The system cannot find the path specified`), and no Cloudflare CLI credentials
or custom connector were available. Only production diagnosis/deployment access
requires owner assistance; the Unreal client source and patch are reviewable.
