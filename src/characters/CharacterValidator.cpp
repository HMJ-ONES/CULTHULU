#include "characters/CharacterValidator.h"

#include "characters/abilities/RmbAbility.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>

namespace cultulhu {

namespace {

// Effect kinds the engine binding understands today. Unknown kinds are a
// warning (forward compatibility), not an error.
// Wave 25 added: "dash" (power = meters moved), "pull" (power = drag
// meters toward the caster), "stun" (power = seconds). See
// assets/characters/KITS.md for the full vocabulary contract.
bool knownEffectKind(const std::string& k) {
    static const char* kinds[] = {"aoe_damage", "fear_aura", "summon",
                                  "buff",       "projectile", "heal",
                                  "shield",     "debuff",     "dash",
                                  "pull",       "stun"};
    for (const char* c : kinds)
        if (k == c) return true;
    return false;
}

bool knownCcType(const std::string& c) {
    return c == "Stun" || c == "Slow" || c == "Root" || c == "Fear";
}

std::string lower(std::string s) {
    for (auto& ch : s)
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    return s;
}

// Basename stem of a clip path, lowercased.
std::string clipStem(const std::string& path) {
    size_t slash = path.find_last_of("/\\");
    std::string base =
        (slash == std::string::npos) ? path : path.substr(slash + 1);
    const size_t dot = base.find_last_of('.');
    if (dot != std::string::npos) base = base.substr(0, dot);
    return lower(base);
}

// Which animation states this character's kit actually exercises.
std::vector<std::string> neededClips(const CharacterDef& d) {
    std::vector<std::string> need = {"idle", "walk", "run", "attack",
                                     "death", "stunned"};
    auto hasKit = [](const SpellDef& s) {
        return !s.id.empty() || !s.name.empty();
    };
    if (hasKit(d.qAbility) || hasKit(d.fAbility) || hasKit(d.rAbility))
        need.push_back("cast");
    if (!d.rmbAbilityId.empty()) need.push_back("castwave");
    if (d.rAbility.effectKind == "summon") need.push_back("channel");
    return need;
}

bool stemMatches(const std::string& stem, const std::string& want) {
    // "castwave" must not satisfy "cast": match the longest names first
    // by checking whole-word-ish containment, preferring exact stems.
    if (stem == want) return true;
    if (want == "cast" &&
        (stem.find("castwave") != std::string::npos)) return false;
    return stem.find(want) != std::string::npos;
}

} // namespace

ValidationReport CharacterValidator::validate(const CharacterPackage& pkg) {
    ValidationReport r;
    if (!pkg.defOk) {
        r.errors.push_back("character.def: " + pkg.defError);
        r.ok = false;
        return r;
    }
    r.ok = true;

    // RMB kit: unknown ids fall back to the plain HeavyAttackDef numbers.
    if (!pkg.def.rmbAbilityId.empty() &&
        createRmbAbility(pkg.def.rmbAbilityId) == nullptr) {
        r.warnings.push_back("unknown RMB kit '" + pkg.def.rmbAbilityId +
                             "': falling back to HeavyAttackDef numbers");
    }

    if (!pkg.hasModel) {
        r.warnings.push_back("model slot empty: logic-only (no model.fbx/.glb)");
    } else if (pkg.rigMapping.mappedCount() == 0) {
        r.warnings.push_back("bone list unavailable: rig mapping deferred "
                             "until FBX import");
    }

    if (pkg.rigAutoMapped) {
        std::ostringstream ss;
        ss << "no rig.map: auto-mapped with "
           << static_cast<int>(pkg.rigMapping.overallConfidence() * 100)
           << "% confidence";
        r.warnings.push_back(ss.str());
    }
    for (const std::string& b : pkg.rigMapping.unmapped()) {
        r.warnings.push_back("bone '" + b + "' has no source match "
                             "(procedural anims still work)");
    }

    if (pkg.clipFiles.empty()) {
        r.warnings.push_back("no animations/: using procedural fallback "
                             "for: walk, run, idle, attack, death");
    }

    if (pkg.def.id != pkg.folderName) {
        r.warnings.push_back("def id '" + pkg.def.id + "' does not match "
                             "folder '" + pkg.folderName + "'");
    }
    return r;
}

std::string ValidationReport::summary() const {
    std::ostringstream ss;
    ss << (ok ? "OK" : "INVALID") << "\n";
    for (const auto& e : errors) ss << "  ERROR: " << e << "\n";
    for (const auto& w : warnings) ss << "  warn:  " << w << "\n";
    if (errors.empty() && warnings.empty()) ss << "  (clean)\n";
    return ss.str();
}

// ---------------------------------------------------------------------------
// Deep validation.
// ---------------------------------------------------------------------------

DeepValidationReport CharacterValidator::validateDeep(
    const CharacterPackage& pkg) {
    DeepValidationReport r;

    // --- 1. character.def schema ------------------------------------------
    if (!pkg.defOk) {
        r.errors.push_back("character.def: " + pkg.defError);
        r.grade = PackageGrade::Invalid;
        return r; // nothing else is meaningful without a def
    }
    const CharacterDef& d = pkg.def;
    if (d.id.empty()) {
        r.errors.push_back("def: missing required 'id'");
    } else if (d.id != pkg.folderName) {
        r.warnings.push_back("def id '" + d.id + "' does not match folder '" +
                             pkg.folderName + "' (loads anyway)");
    }
    if (d.displayName.empty())
        r.warnings.push_back("def: no display_name (UI shows the id)");
    if (d.maxHp <= 0.0f)
        r.errors.push_back("def: max_hp must be > 0");
    if (d.moveSpeed <= 0.0f)
        r.warnings.push_back("def: move_speed <= 0 (character cannot move)");
    if (d.maxStamina <= 0.0f)
        r.warnings.push_back("def: max_stamina <= 0 (no sprint/abilities)");

    const SpellDef* kits[3] = {&d.qAbility, &d.fAbility, &d.rAbility};
    const char* slots[3] = {"Q", "F", "R"};
    for (int i = 0; i < 3; ++i) {
        const SpellDef& s = *kits[i];
        if (s.id.empty() && s.name.empty()) continue; // slot unused
        const std::string tag = std::string("def [") + slots[i] + "]: ";
        if (s.name.empty())
            r.warnings.push_back(tag + "no name (UI shows the id)");
        if (s.cooldownSec < 0.0f)
            r.errors.push_back(tag + "cooldown must be >= 0");
        if (s.staminaCost < 0.0f || s.manaCost < 0.0f)
            r.errors.push_back(tag + "costs must be >= 0");
        if (!s.effectKind.empty() && !knownEffectKind(s.effectKind))
            r.warnings.push_back(tag + "unknown effect '" + s.effectKind +
                                 "' (engine binding may ignore it)");
    }
    if (d.meleeComboId.empty())
        r.warnings.push_back("def: no melee_combo (falls back to the "
                             "default combo chain)");
    if (!d.rmbAbilityId.empty() &&
        createRmbAbility(d.rmbAbilityId) == nullptr)
        r.warnings.push_back("def: unknown rmb_ability '" + d.rmbAbilityId +
                             "' (falls back to HeavyAttackDef numbers)");
    if (!d.rightClick.ccType.empty() && !knownCcType(d.rightClick.ccType))
        r.warnings.push_back("def [rightclick]: unknown cc '" +
                             d.rightClick.ccType +
                             "' (expected Stun/Slow/Root/Fear)");

    // --- 2. model file ------------------------------------------------------
    if (!pkg.hasModel) {
        r.warnings.push_back("model: slot empty (logic-only, no "
                             "model.fbx/.glb) — gameplay unaffected");
    } else {
        const std::string path = pkg.folderPath + "/" + pkg.modelFile;
        std::ifstream in(path, std::ios::binary);
        char magic[20] = {0};
        in.read(magic, sizeof(magic));
        const std::streamsize got = in.gcount();
        if (got <= 0) {
            r.errors.push_back("model: '" + pkg.modelFile +
                               "' unreadable (empty or locked)");
        } else {
            const std::string head(magic, static_cast<size_t>(got));
            bool plausible = false;
            if (pkg.modelFile == "model.glb")
                plausible = head.rfind("glTF", 0) == 0;
            else // model.fbx: binary ("Kaydara FBX Binary") or ASCII ("; FBX")
                plausible = head.find("Kaydara FBX Binary") !=
                                std::string::npos ||
                            head.rfind("; FBX", 0) == 0;
            if (!plausible)
                r.errors.push_back("model: '" + pkg.modelFile +
                                   "' does not look like an " +
                                   (pkg.modelFile == "model.glb"
                                        ? "glB (missing glTF magic)"
                                        : "FBX file"));
            else
                r.info.push_back("model: '" + pkg.modelFile + "' present, " +
                                 std::to_string(got > 1024 ? 1 : 0) +
                                 "KB+ header plausible");
        }
    }

    // --- 3. rig mapping -----------------------------------------------------
    for (const auto& w : pkg.rigMapWarnings)
        r.warnings.push_back("rig.map: " + w + " (line ignored)");
    if (pkg.hasModel || pkg.hasBonesList) {
        for (const std::string& line : pkg.rigMapping.diagnosticLines())
            r.info.push_back("rig: " + line);
        if (pkg.rigAutoMapped)
            r.info.push_back("rig: auto-mapped (no rig.map)");
        else if (pkg.hasRigMap)
            r.info.push_back("rig: explicit rig.map");
        for (const std::string& b : pkg.rigMapping.unmapped())
            r.warnings.push_back("rig: bone '" + b +
                                 "' has no source match (procedural anims "
                                 "still work; add a rig.map line)");
    } else if (!pkg.hasModel) {
        r.info.push_back("rig: no model, mapping deferred until import");
    }

    // --- 4. animation clips vs. ability-kit needs ---------------------------
    std::vector<std::string> stems;
    for (const auto& p : pkg.clipFiles) stems.push_back(clipStem(p));
    for (const std::string& need : neededClips(d)) {
        bool found = false;
        for (const std::string& s : stems)
            if (stemMatches(s, need)) {
                found = true;
                break;
            }
        if (found) {
            r.info.push_back("anim: custom clip for '" + need + "'");
        } else {
            r.warnings.push_back("anim: no '" + need +
                                 "' clip (procedural fallback)");
        }
    }

    r.grade = !r.errors.empty()   ? PackageGrade::Invalid
              : !r.warnings.empty() ? PackageGrade::Warnings
                                    : PackageGrade::Clean;
    return r;
}

std::string DeepValidationReport::summary() const {
    std::ostringstream ss;
    ss << "grade: " << gradeName(grade) << "\n";
    for (const auto& e : errors) ss << "  ERROR: " << e << "\n";
    for (const auto& w : warnings) ss << "  warn:  " << w << "\n";
    for (const auto& i : info) ss << "  info:  " << i << "\n";
    if (errors.empty() && warnings.empty() && info.empty())
        ss << "  (clean)\n";
    return ss.str();
}

} // namespace cultulhu
