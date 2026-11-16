"""Opt-in production API + two-process UE private-lobby smoke test.

Uses one PC's existing 26/8 adapter. Does NOT create/join a Radmin network or
prove two-PC VPN connectivity. Private test lobby only; credentials never printed.
Requires an already compiled canonical CultUlhuEditor target.
"""
from pathlib import Path
import argparse
import json
import os
import subprocess
import time
import urllib.request
import urllib.error

PROJECT = Path(__file__).resolve().parents[1]
EDITOR = Path('C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe')
API = 'https://cultulhu-lobby.pages.dev/api/v1'


def run(mode):
    folder = PROJECT/'Saved/Automation'
    folder.mkdir(exist_ok=True)
    # Only exact test-owned files; preserve all artist files, saves and unrelated evidence.
    names = ('invite.txt', 'lobby-id.txt', 'guest-left', 'host.json', 'guest.json')
    for name in names:
        file = folder/('CloudLobby_'+name)
        if file.exists():
            file.unlink()
    children = []
    try:
        for role in ('Host', 'Guest'):
            args = [str(EDITOR), str(PROJECT/'CultUlhu.uproject'), '/Game/Maps/L_MainMenu',
                    '-game', '-unattended', '-nosound', '-nosplash', '-RenderOffscreen',
                    '-windowed', '-ResX=960', '-ResY=540', '-DisablePlugins=ModelContextProtocol',
                    '-ExecCmds=t.MaxFPS 30', f'-CultCloudLobbyProbe={role}', f'-CultCloudLobbyMode={mode}',
                    f'-abslog={PROJECT/"Saved"/("CloudLobby"+role+".log")}']
            child = subprocess.Popen(args, creationflags=subprocess.CREATE_NO_WINDOW if os.name == 'nt' else 0)
            children.append(child)
            if role == 'Host':
                until = time.monotonic()+160
                while not (folder/'CloudLobby_invite.txt').exists():
                    if child.poll() is not None or time.monotonic() > until:
                        raise RuntimeError('Private test host did not become ready. Inspect its local log.')
                    time.sleep(.5)
        until = time.monotonic()+150
        while any(c.poll() is None for c in children) and time.monotonic() < until:
            time.sleep(.5)
        results = {}
        for role in ('host', 'guest'):
            file = folder/f'CloudLobby_{role}.json'
            if not file.exists():
                raise RuntimeError(f'{role} did not write its result; inspect its local log.')
            results[role] = json.loads(file.read_text(encoding='utf-8-sig'))
            if not results[role]['ok']:
                raise RuntimeError(f'{role} smoke test failed; inspect its local log.')
        # Host teardown must revoke the invitation, not merely hide a public listing.
        invite = (folder/'CloudLobby_invite.txt').read_text(encoding='utf-8-sig')
        request = urllib.request.Request(API+'/invitations/resolve',
            data=json.dumps({'inviteCode': invite}).encode(), headers={'Content-Type': 'application/json'})
        try:
            urllib.request.urlopen(request, timeout=15).close()
            raise RuntimeError('Host exited but invitation still resolves.')
        except urllib.error.HTTPError as error:
            if error.code != 404:
                raise RuntimeError(f'Unexpected cleanup check status {error.code}') from None
        results['invitationRevokedAfterHostExit'] = True
        results['mode'] = mode
        output = folder/f'CloudLobby_mode{mode}_results.json'
        output.write_text(json.dumps(results, indent=2)+'\n', encoding='utf-8')
        print(json.dumps(results, indent=2))
    finally:
        for child in children:
            if child.poll() is None:
                child.terminate()
                child.wait(timeout=15)
        # Remove the disposable invitation; keep redacted result JSON and local logs.
        for name in ('invite.txt', 'lobby-id.txt'):
            file = folder/('CloudLobby_'+name)
            if file.exists():
                file.unlink()


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', type=int, choices=(0, 1, 2), default=2)
    run(parser.parse_args().mode)
