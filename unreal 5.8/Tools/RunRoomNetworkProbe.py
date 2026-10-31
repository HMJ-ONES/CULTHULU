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


def run(mode, lighting=False):
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
            if lighting:
                args.append("-CultRoomLighting")
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
        if lighting:
            for role in ("host", "guest"):
                for suffix in (".json", "_0.png", "_1.png"):
                    if not (evidence / f"Room{mode}_lighting_{role}{suffix}").is_file():
                        raise RuntimeError(f"Missing matched lighting capture: {role}{suffix}")
            skies, moons = [], []
            for role in ("host", "guest"):
                configuration = json.loads((evidence / f"Room{mode}_lighting_{role}.json").read_text(encoding="utf-8-sig"))
                active = [sky for sky in configuration["sky"] if sky["intensity"] > 0 and sky["visible"] and not sky["hidden"]]
                if len(active) != 1 or active[0]["sourceType"] != 1 or "DaylightAmbientCubemap" not in active[0]["cubemap"]:
                    raise RuntimeError(f"{role} has no deterministic ambient source: {active}")
                skies.append(active[0])
                moons.append([light for light in configuration["directional"] if light["intensity"] > 0 and light["visible"] and not light["hidden"]])
            if skies[0]["cubemap"] != skies[1]["cubemap"] or abs(skies[0]["intensity"] - skies[1]["intensity"]) > .001:
                raise RuntimeError("Host/guest ambient lighting configuration differs")
            if any(len(active) != 1 for active in moons) or any(abs(moons[0][0][key] - moons[1][0][key]) > .01 for key in ("pitch", "yaw", "intensity")):
                raise RuntimeError(f"Host/guest moonlight configuration differs: {moons}")
            from AnalyzeMatchLighting import analyze
            measurements = analyze(evidence, mode)
            (evidence / f"Room{mode}_lighting_pixels.json").write_text(json.dumps(measurements, indent=2) + "\n", encoding="utf-8")
            differences = []
            for view in measurements["views"]:
                for name in ("grayLeft", "grayRight"):
                    region = view["regions"][name]
                    difference = region["hostGuestMeanAbsoluteDifferenceSrgb255"]
                    if difference > 4 or min(region[role]["medianSrgb255"] for role in ("host", "guest")) < 15:
                        raise RuntimeError(f"Unmatched or unreadable gray reference: view {view['view']} {name}: {region}")
                    differences.append(difference)
            print(json.dumps({"mode": mode, "matchedLighting": True, "referenceDifferencesSrgb255": differences}), flush=True)
        if mode == 2 and not (evidence / "Room2_ward").is_file():
            raise RuntimeError("DOTGOO guest never received the server's targeted ward warning")
        if mode == 2:
            warning = json.loads((evidence / "Room2_ward.json").read_text(encoding="utf-8-sig"))
            # A normal 0.25s tower attack cannot satisfy this eight-second
            # fixture check. Initial join clock skew must not extend it either.
            if not (4 < warning["serverRemaining"] <= 8
                    and 0 < warning["guestRemaining"] <= warning["serverRemaining"] + .01):
                raise RuntimeError(f"Guest ward countdown diverged: {warning}")
            print(json.dumps({"wardWarning": warning}), flush=True)
        return results
    finally:
        for p in processes:
            if p.poll() is None:
                p.terminate()
                p.wait(timeout=15)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mode", type=int, choices=(0, 1, 2))
    parser.add_argument("--lighting", action="store_true", help="Capture matched host/guest lighting views before combat checks")
    args = parser.parse_args()
    for mode in ([args.mode] if args.mode is not None else [0, 1, 2]):
        run(mode, args.lighting)
