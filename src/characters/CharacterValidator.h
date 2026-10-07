#pragma once

// CULT-ULHU character package validator (wave 7).
//
// Checks a loaded package and reports problems loudly but never fatally:
// the game must always run, so a package with just a character.def and
// nothing else is fully playable. Missing pieces degrade gracefully:
//   - no model.fbx      -> "model slot empty, logic-only"
//   - no animations/    -> "using procedural fallback for: walk, run, ..."
//   - no rig.map        -> "auto-mapped with X% confidence" (when a bone
//                          list is available) or "mapping deferred"
//   - unknown rmb kit   -> falls back to the HeavyAttackDef numbers

#include "characters/CharacterPackage.h"

#include <string>
#include <vector>

namespace cultulhu {

struct ValidationReport {
    bool ok = false; // character.def parsed and sane
    std::vector<std::string> errors;   // package unusable
    std::vector<std::string> warnings; // degraded but playable
    std::string summary() const;       // human-readable multi-line report
};

class CharacterValidator {
public:
    static ValidationReport validate(const CharacterPackage& pkg);
};

} // namespace cultulhu
