# Play together with Radmin

All three modes have the same four entry buttons. Radmin VPN is already installed on this development PC. Friends need their own copy of the same game build and must join the same Radmin network.

## First time, with a friend

1. Open Radmin VPN. One person creates a network with a name and password; friends choose **Join network** and enter those details. Radmin's network password is separate from the game's room code.
2. Launch Cult-Ulhu from the normal desktop shortcut. Choose **Free Roam**, **Domination**, or **Defense of the Great Old Ones**.
3. The host chooses **ENTER / HOST** for an open room, or **PRIVATE ROOM** for an invitation-only room. Choose an entity, then enter battle. Free Roam hosts also choose three initial beliefs.
4. While playing, the host presses **Esc**, then **COPY CODE / COPY ROOM CODE**. Send that code to the friend yourself.
5. The friend selects the same mode, pastes the complete code into the input box, and clicks **ENTER ROOM CODE**. They choose their own entity and enter. Free Roam guests share the host's cult and beliefs; competitive modes have no belief picker.
6. To host an existing Free Roam save, choose **CONTINUE SAVED WORLD**, then press **Esc** and **HOST CO-OP / GET ROOM CODE**. After it reloads as a room, open Esc again to copy the code.
7. For public rooms, friends can instead click **ENTER MATCH**. It searches the local LAN / Radmin network for about three seconds. If nothing is found, use the room code or ask someone to host.

**ENTER MATCH** searches currently open rooms of the selected mode. It is local network discovery, with no central online matchmaking service. **ENTER / HOST** starts an open room immediately. **PRIVATE ROOM** is hidden from discovery and requires its invitation. **ENTER ROOM CODE** works with either room type. **PRACTICE / OFFLINE** (or **NEW WORLD / OFFLINE**) still works without Radmin.

## Esc and leaving a match

- **Domination and DOTGOO never globally pause.** Esc opens a local menu with **RESUME** and **QUIT MATCH**. Other players, bots, creep waves and the match clock keep running. Your character remains vulnerable while you are in the menu.
- A guest who quits returns to the main menu; a bot takes their team slot. Disconnects also refill the slot.
- If the host quits the competitive match, a bot takes their slot and a **ROOM STILL RUNNING** window appears. Friends stay connected while the host keeps the game open. **REJOIN MATCH** returns the host to an available slot. **CLOSE ROOM & MAIN MENU** shuts down the room and disconnects guests. There is no host migration if the host closes the application or loses connection.
- After a competitive match ends, only the host starts a rematch. Guests remain in the same room.
- Free Roam keeps its save/load menu. Solo Free Roam can pause; multiplayer Free Roam continues. Only the host saves the shared world. Returning the Free Roam host to the main menu ends that room.

Competitive rooms support ten humans with bots filling unused slots. Free Roam supports eight players. Joining a competitive room replaces a bot on the team with fewer humans; the guest keeps that fighter's match progress.

## If a friend cannot connect

1. Confirm both Radmin peers are online in the same network and both are using the same Cult-Ulhu build.
2. Allow the game's Unreal executable through Windows Firewall if Windows asks. Do not disable the firewall. The default game port is UDP **7777**; public discovery uses UDP **27777**.
3. Try the full room code even if **ENTER MATCH** finds nothing. Some VPN configurations do not forward broadcast discovery.
4. Copy a fresh code after the host changes networks, restarts, or creates a different private room. Select the mode named by the code's validation message.

The game prefers the Radmin adapter's address when making codes. Codes contain the host address, port and mode; private codes also contain an invitation secret. Share private codes only with invited players. They are not account authentication, and there is no relay service.

Radmin's official network and firewall instructions: [Radmin VPN Help](https://www.radmin-vpn.com/help/index.html).

## Developer verification

Build only `unreal 5.8/CultUlhu.uproject`, following `AGENTS.md`. After building, `Tools/RunRoomNetworkProbe.py` launches independent hidden host and guest game processes for all three modes. `--mode 0`, `--mode 1`, and `--mode 2` isolate Free Roam, Domination, and DOTGOO. Evidence and temporary room invitations stay under `Saved/Automation`; never publish live codes or personal adapter addresses.

Native automation covers room-code validation, the four menu callbacks, normal mode selection, competitive Esc input/click coordinates, and Free Roam persistence. Same-PC tests do not certify a tunnel between different computers; one real peer is still needed for that final network check.
