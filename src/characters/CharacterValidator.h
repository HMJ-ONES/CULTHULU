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

// Grading for the deep validator: every package either loads or it
// doesn't, and everything in between is a documented fallback.
enum class PackageGrade {
    Invalid,   // errors: the package cannot load as-is
    Warnings,  // loads, but something falls back to procedural/default
    Clean,     // every piece present and sane
};

inline const char* gradeName(PackageGrade g) {
    switch (g) {
        case PackageGrade::Invalid:  return "INVALID";
        case PackageGrade::Warnings: return "OK WITH WARNINGS";
        case PackageGrade::Clean:    return "CLEAN";
    }
    return "?";
}

struct DeepValidationReport {
    PackageGrade grade = PackageGrade::Invalid;
    std::vector<std::string> errors;   // block loading
    std::vector<std::string> warnings; // playable; each names the fallback
    std::vector<std::string> info;     // diagnostics (rig detail, inventory)
    std::string summary() const;       // graded multi-section report
};

class CharacterValidator {
public:
    static ValidationReport validate(const CharacterPackage& pkg);

    // Deep validation (second shift): schema-checks character.def values,
    // probes the model file (format magic), audits rig.map coverage with
    // per-bone diagnostics, and cross-checks animation clips against what
    // the ability kit actually needs. Never throws; reads package files.
    static DeepValidationReport validateDeep(const CharacterPackage& pkg);
};

} // namespace cultulhu
