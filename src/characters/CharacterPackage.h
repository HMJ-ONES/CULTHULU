#pragma once

// CULT-ULHU character package (wave 7): everything one playable character
// needs, as a folder:
//
//   assets/characters/<name>/
//     character.def   (required: stats, ability kit, RMB, passive)
//     model.fbx       (optional: rigged model; logic-only without it)
//     model.glb       (optional: alternative to model.fbx; static or rigged)
//     rig.map         (optional: explicit "engine_bone = fbx_bone" lines)
//     bones.list      (optional: one FBX bone name per line; lets the
//                     auto-mapper run before the real FBX importer lands)
//     animations/     (optional: .canim clips; procedural fallback otherwise)
//
// Drop the folder in, the loader picks it up — no code changes.

#include "characters/CharacterDef.h"
#include "characters/RigMapper.h"

#include <string>
#include <vector>

namespace cultulhu {

struct CharacterPackage {
    std::string folderName; // <name>
    std::string folderPath; // full path to the folder

    CharacterDef def;
    bool defOk = false;
    std::string defError; // set when !defOk

    bool hasModel = false;
    std::string modelFile; // "model.fbx" or "model.glb", whichever was found
    bool hasRigMap = false;
    bool hasBonesList = false;
    std::vector<std::string> clipFiles; // .canim paths found

    // Warnings produced while parsing rig.map (unknown engine bones,
    // malformed lines). Surfaced by the deep validator.
    std::vector<std::string> rigMapWarnings;

    RigMapping rigMapping;   // explicit (rig.map) or auto-mapped
    bool rigAutoMapped = false;
};

} // namespace cultulhu
