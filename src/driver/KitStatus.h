#pragma once
// CULT-ULHU driver helper: the `status` kit line.
//
// Convenience pass (2026-10-13): the kit line used to read "kit: q READY
// f 3s r READY rmb READY" — cooldowns with no identity, so a new player
// (or anyone resuming a save) couldn't see WHO they walk as without typing
// `character select`. This helper formats the identity prefix; kept header-
// inline and dependency-free so the micro regression suite can pin it
// (main.cpp isn't linkable from tests — same pattern as SelectNudge.h).
#include "characters/CharacterDef.h"

#include <string>

namespace cultulhu {
namespace driver {

// Identity prefix for the `status` kit line.
//  - null kit      -> guidance toward picking one (never a bare "none").
//  - empty name    -> the raw kit id (a name is missing, not the kit).
inline std::string kitStatusName(const CharacterDef* kit,
                                 const std::string& kitId) {
    if (!kit)
        return "none (character select <id>; see 'chars')";
    if (kit->displayName.empty()) return kitId;
    return kit->displayName;
}

}  // namespace driver
}  // namespace cultulhu
