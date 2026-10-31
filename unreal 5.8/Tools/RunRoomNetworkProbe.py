"""Run isolated, opt-in standalone host/guest probes against the canonical project.

Uses the already built editor. Writes only small local evidence under Saved/Automation.
No Radmin credentials are read or changed. Never publishes room codes.
"""
import argparse
import json
import os
from pathlib import Path
import subprocess
import time

PROJECT = Path(__file__).resolve().parents[1]
EDITOR = Path(os.environ.get("CULT_UNREAL_EDITOR", "C:/Program Files/Epic Games/UE_5.8/Engine/Binaries/Win64/UnrealEditor.exe"))


def run(mode):
    saved = PROJECT / "Saved"
    evidence = saved / "Automation"
    evidence.mkdir(exist_ok=True)
    # Exact test-owned prefix only; never remove saves, user screenshots or logs.
    for file in evidence.glob(f"Room{mode}_*"):
        if file.is_file():
            file.unlink()
    processes = []
    try:
        def launch(role):
            args = [str(EDITOR), str(PROJECT / "CultUlhu.uproject"), "/Game/Maps/L_MainMenu", "-game", "-unattended", "-nosound", "-nosplash", "-RenderOffscreen", "-windowed", "-ResX=1280", "-ResY=720", "-DisablePlugins=ModelContextProtocol", f"-CultRoomProbe={role}", f"-CultRoomMode={mode}", f"-port={7791+mode}", f"-CultDiscoveryPort={27991+mode}", "-ExecCmds=t.MaxFPS 30", f"-abslog={saved / ('Room'+str(mode)+role+'.log')}"]
            p = subprocess.Popen(args, creationflags=subprocess.CREATE_NO_WINDOW if os.name == "nt" else 0)
            processes.append(p)
            return p
        host = launch("Host")
        until = time.monotonic() + 180
        while not (evidence / f"Room{mode}_code.txt").exists():
            if host.poll() is not None or time.monotonic() > until:
                raise RuntimeError(f"Mode {mode}: host did not become ready; inspect Room{mode}Host.log")
            time.sleep(.5)
        guest = launch("Guest")
        until = time.monotonic() + 240
        while any(p.poll() is None for p in processes):
            if time.monotonic() > until:
                raise RuntimeError(f"Mode {mode}: probe process timeout")
            time.sleep(1)
        results = [json.loads((evidence / f"Room{mode}_{role}.json").read_text(encoding="utf-8-sig")) for role in ("host", "guest")]
        print(json.dumps(results), flush=True)
        if not all(r["ok"] for r in results):
            raise RuntimeError(f"Mode {mode} failed")
        return results
    finally:
        for p in processes:
            if p.poll() is None:
                p.terminate()
                p.wait(timeout=15)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", type=int, choices=(0, 1, 2))
    args = parser.parse_args()
    for mode in ([args.mode] if args.mode is not None else [0, 1, 2]):
        run(mode)
