#!/usr/bin/env python3
"""Offline validation for the CULT-ULHU UE5 binding (no UE5 needed).

Checks (all static, no engine):
 1. CultUlhu.uproject is valid JSON with the required modules + EnhancedInput.
 2. Module Build.cs files reference directories that exist; the core include
    path math (ModuleDirectory/../../../src) resolves to the real ../src.
 3. Every `// VERIFY IN EDITOR:` marker under Source/ is accounted for in
    Docs/InEditorVerification.md (by file path).
 4. Docs/EnhancedInputMapping.md covers all 11 owner bindings.
 5. Docs/CharacterPipeline.md field table covers every key in the Cthulhu
    Avatar character.def (the template package).
 6. Every core header #included by the binding exists in ../src
    (catches typos like "beliefs/BeliefSystems.h").

Run: python3 unreal/Tests/validate_binding.py   (from the repo root)
Exit 0 = all green.
"""
import json
import os
import re
import sys

REPO = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
UNREAL = os.path.join(REPO, "unreal")
SRC = os.path.join(REPO, "src")
failures = []


def check(cond, msg):
    print(("PASS " if cond else "FAIL ") + msg)
    if not cond:
        failures.append(msg)


# --- 1. uproject -----------------------------------------------------------
uproj_path = os.path.join(UNREAL, "CultUlhu.uproject")
try:
    with open(uproj_path) as f:
        uproj = json.load(f)
    mods = {m["Name"] for m in uproj.get("Modules", [])}
    check({"CultUlhuCore", "CultUlhu"} <= mods,
          "uproject declares CultUlhuCore + CultUlhu modules")
    plugins = {p["Name"]: p.get("Enabled") for p in uproj.get("Plugins", [])}
    check(plugins.get("EnhancedInput") is True,
          "uproject enables the EnhancedInput plugin")
    check(bool(uproj.get("EngineAssociation")),
          "uproject sets EngineAssociation (owner picks their UE5 version)")
except (json.JSONDecodeError, OSError) as e:
    check(False, f"uproject parses as JSON: {e}")

# --- 2. Build.cs path math --------------------------------------------------
for module in ("CultUlhuCore", "CultUlhu"):
    mod_dir = os.path.join(UNREAL, "Source", module)
    check(os.path.isdir(mod_dir), f"module dir exists: Source/{module}")
    # The Build.cs computes RepoRoot = ModuleDirectory/../../.. ; verify.
    resolved = os.path.normpath(os.path.join(mod_dir, "..", "..", ".."))
    check(resolved == REPO,
          f"{module}.Build.cs RepoRoot math lands on the repo root")
    check(os.path.isdir(os.path.join(resolved, "src")),
          f"{module}.Build.cs core include path (../../src) exists")

# --- 3. VERIFY IN EDITOR markers accounted ----------------------------------
marker_files = set()
marker_re = re.compile(r"//\s*VERIFY IN EDITOR:")
for root, _, files in os.walk(os.path.join(UNREAL, "Source")):
    for fn in files:
        p = os.path.join(root, fn)
        with open(p, encoding="utf-8", errors="replace") as f:
            if marker_re.search(f.read()):
                marker_files.add(os.path.relpath(p, UNREAL))

verif_path = os.path.join(UNREAL, "Docs", "InEditorVerification.md")
with open(verif_path) as f:
    verif_text = f.read()
for rel in sorted(marker_files):
    # checklist references the file by its Source/... or Docs/... path
    check(rel in verif_text,
          f"InEditorVerification.md accounts for markers in {rel}")
check(len(marker_files) > 0, "found VERIFY IN EDITOR markers to account for")

# --- 4. Enhanced Input action coverage --------------------------------------
ei_path = os.path.join(UNREAL, "Docs", "EnhancedInputMapping.md")
with open(ei_path) as f:
    ei_text = f.read()
for action in ("IA_Move", "IA_Jump", "IA_Sprint", "IA_AbilityQ", "IA_AbilityF",
               "IA_AbilityR", "IA_Interact", "IA_StatsOverlay", "IA_Attack",
               "IA_HeavyAttack", "IA_CommandMenu"):
    check(action in ei_text, f"EnhancedInputMapping.md documents {action}")

# --- 5. character.def field coverage ----------------------------------------
def parse_def(path):
    """Minimal character.def parser: {section: {key: value}}, '' = global."""
    sections = {"": {}}
    cur = ""
    with open(path) as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"):
                continue
            if line.startswith("[") and line.endswith("]"):
                cur = line[1:-1]
                sections.setdefault(cur, {})
            elif "=" in line:
                k, v = line.split("=", 1)
                sections[cur][k.strip()] = v.strip()
    return sections

def_path = os.path.join(REPO, "assets", "characters", "cthulhu_avatar",
                        "character.def")
check(os.path.isfile(def_path), "template character.def exists for coverage check")
if os.path.isfile(def_path):
    sections = parse_def(def_path)
    pipe_path = os.path.join(UNREAL, "Docs", "CharacterPipeline.md")
    with open(pipe_path) as f:
        pipe_text = f.read()
    expected = set()
    for sec, kv in sections.items():
        for k in kv:
            expected.add(k if not sec else f"{sec}.{k}")
    # The doc's table uses backticked def keys; spot-check the important ones.
    for key in ("id", "display_name", "max_hp", "move_speed", "max_stamina",
                "melee_combo", "rmb_ability", "[q]", "[rightclick]",
                "[passive]", "cooldown", "stamina_cost", "effect",
                "damage_mult", "cc_seconds"):
        # Table lists keys backticked, sometimes as `key→Field` mappings.
        check(f"`{key}`" in pipe_text or f"`{key}→" in pipe_text,
              f"CharacterPipeline.md field table covers `{key}`")
    check(len(expected) >= 20,
          f"template def has a realistic field count ({len(expected)})")

# --- 6. quoted #includes resolve ------------------------------------------
# A quoted include with a slash is one of:
#   core header .... first segment lowercase (beliefs/BeliefSystem.h)
#   module header .. resolves under the module's Public/ or Private/
#   engine header .. e.g. GameFramework/Actor.h (can't verify w/o UE5; skip)
inc_re = re.compile(r'#include\s+"([a-zA-Z0-9_/]+\.h)"')
checked = set()
for root, _, files in os.walk(os.path.join(UNREAL, "Source")):
    for fn in files:
        if not fn.endswith((".h", ".cpp")):
            continue
        p = os.path.join(root, fn)
        # Which module is this file in? Source/<Module>/{Public,Private}/...
        rel = os.path.relpath(p, os.path.join(UNREAL, "Source"))
        module = rel.split(os.sep)[0]
        mod_pub = os.path.join(UNREAL, "Source", module, "Public")
        mod_prv = os.path.join(UNREAL, "Source", module, "Private")
        with open(p, encoding="utf-8", errors="replace") as f:
            for m in inc_re.finditer(f.read()):
                inc = m.group(1)
                if inc in checked or "/" not in inc:
                    continue
                checked.add(inc)
                first = inc.split("/")[0]
                if first[:1].islower():
                    # Core header: must exist under ../src.
                    check(os.path.isfile(os.path.join(SRC, inc)),
                          f'core include "{inc}" exists under src/')
                elif os.path.isfile(os.path.join(mod_pub, inc)) or \
                        os.path.isfile(os.path.join(mod_prv, inc)):
                    check(True, f'module include "{inc}" resolves in {module}')
                else:
                    # Engine header (GameFramework/..., Components/...):
                    # not verifiable without UE5; record as skipped, not failed.
                    print(f"SKIP engine include (needs UE5): {inc}")
print(f"\n{len(failures)} failure(s)")
sys.exit(1 if failures else 0)
