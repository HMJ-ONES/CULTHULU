#!/usr/bin/env python3
"""CULT-ULHU pre-build directory validation (run before every compile).

Checks the permanent architectural invariant: there is exactly ONE Unreal
project, at <repo>/unreal 5.8/, and everything points at it.

Run:  python3 "unreal 5.8/Tools/ValidateCultUlhu.py"   (from the repo root)
Exit: 0 = all checks pass. 1 = STOP, do not build; read the failures.

Also wired into unreal 5.8/Tools/SetupCultUlhu.ps1 (step 2).
"""
import configparser
import os
import re
import sys

FAILURES = []


def check(name, ok, hint=""):
    print("[%s] %s" % ("OK" if ok else "FAIL", name), flush=True)
    if not ok:
        FAILURES.append(name + (" — " + hint if hint else ""))


def main():
    tools_dir = os.path.dirname(os.path.abspath(__file__))
    repo = os.path.normpath(os.path.join(tools_dir, "..", ".."))
    uproj = os.path.join(repo, "unreal 5.8")
    legacy = os.path.join(repo, "unreal")

    # 1. canonical directory exists
    check("canonical dir 'unreal 5.8' exists", os.path.isdir(uproj),
          "run Consolidate-CultUlhu.ps1 / re-clone game-core")

    # 2. project file exists there
    proj = os.path.join(uproj, "CultUlhu.uproject")
    check("CultUlhu.uproject in canonical dir", os.path.isfile(proj))
    projects = []
    for directory, folders, files in os.walk(repo):
        folders[:] = [name for name in folders if name not in
                      {'.git', '.vs', 'Binaries', 'Intermediate', 'Saved', 'DerivedDataCache', 'Content'}]
        projects.extend(os.path.normpath(os.path.join(directory, name))
                        for name in files if name.endswith('.uproject'))
    check("exactly one active project descriptor", projects == [proj],
          "preserve extra projects and archive their launch descriptors: " + ', '.join(projects))
    target_classes = []
    for directory, _, files in os.walk(os.path.join(uproj, 'Source')):
        for name in files:
            if name.endswith('.Target.cs'):
                with open(os.path.join(directory, name), encoding='utf-8-sig') as source:
                    target_classes.extend(re.findall(r'class\s+(\w+Target)\s*:', source.read()))
    check("no duplicate Unreal target classes", len(target_classes)==len(set(target_classes)),
          'retire duplicate Target.cs files; preserve the root Source targets')

    # 3+4. required gameplay sources + module definitions
    required = [
        "Source/CultUlhu/CultUlhu.Build.cs",
        "Source/CultUlhuCore/CultUlhuCore.Build.cs",
        "Source/CultUlhu/Private/CultUlhuPlayGameMode.cpp",
        "Source/CultUlhu/Public/CultUlhuPlayGameMode.h",
        "Source/CultUlhu/Private/CultUlhuGameInstance.cpp",
        "Source/CultUlhu/Public/CultUlhuGameInstance.h",
        "Source/CultUlhu/Private/CultUlhuPlayerController.cpp",
        "Source/CultUlhu/Public/CultUlhuPlayerController.h",
        "Source/CultUlhu/Private/CultUlhuPlayerPawn.cpp",
        "Source/CultUlhu/Public/CultUlhuPlayerPawn.h",
        "Source/CultUlhu/Private/UI/CultHomeScreenWidget.cpp",
        "Source/CultUlhu/Public/UI/CultHomeScreenWidget.h",
        "Source/CultUlhu/Private/UI/CultCharacterSelectWidget.cpp",
        "Source/CultUlhu/Public/UI/CultCharacterSelectWidget.h",
    ]
    missing = [r for r in required
               if not os.path.isfile(os.path.join(uproj, r))]
    check("required gameplay sources + module defs present",
          not missing, "missing: %s" % ", ".join(missing))

    # 5. no legacy project directory
    check("no legacy 'unreal/' project directory",
          not os.path.isdir(legacy),
          "old unreal/ reappeared — migrate it into 'unreal 5.8/' and delete it")

    # 6. build scripts target the canonical project
    ps1 = os.path.join(uproj, "Tools", "SetupCultUlhu.ps1")
    ps1_ok = (os.path.isfile(ps1) and
              "unreal 5.8\\CultUlhu.uproject" in
              open(ps1, encoding="utf-8", errors="replace").read())
    check("SetupCultUlhu.ps1 targets canonical project", ps1_ok)

    # 7. configuration points at the right maps and classes
    ini_path = os.path.join(uproj, "Config", "DefaultEngine.ini")
    want = {
        "EditorStartupMap": "/Game/Maps/L_MainMenu",
        "GameDefaultMap": "/Game/Maps/L_MainMenu",
        "GameInstanceClass": "/Script/CultUlhu.CultUlhuGameInstance",
        "GlobalDefaultGameMode": "/Script/CultUlhu.CultUlhuPlayGameMode",
    }
    ini_ok, ini_hint = False, ""
    if os.path.isfile(ini_path):
        cp = configparser.ConfigParser(strict=False)
        cp.optionxform = str  # keep key case
        try:
            cp.read(ini_path, encoding="utf-8")
            sect = "/Script/EngineSettings.GameMapsSettings"
            if cp.has_section(sect):
                bad = [k for k, v in want.items()
                       if cp.get(sect, k, fallback="") != v]
                ini_ok = not bad
                ini_hint = "wrong: %s" % ", ".join(bad) if bad else ""
            else:
                ini_hint = "missing [%s]" % sect
        except Exception as e:  # noqa: BLE001
            ini_hint = "unparseable: %s" % e
    else:
        ini_hint = "DefaultEngine.ini missing"
    check("DefaultEngine.ini points at menu map + game classes",
          ini_ok, ini_hint)

    print(flush=True)
    if FAILURES:
        print("VALIDATION FAILED — do not build. Fix:", flush=True)
        for f in FAILURES:
            print("  - " + f, flush=True)
        return 1
    print("ALL CHECKS PASSED — safe to build the canonical project.", flush=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
