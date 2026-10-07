#include "characters/CharacterValidator.h"

#include "characters/abilities/RmbAbility.h"

#include <sstream>

namespace cultulhu {

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
        r.warnings.push_back("model slot empty: logic-only (no model.fbx)");
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

} // namespace cultulhu
