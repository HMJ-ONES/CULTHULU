#pragma once

// CULT-ULHU wave 42: shared roster helpers.
// The 6 NPC packages (civilians, cultists, echo) are minions and ambient
// life — never match fighters and never wearable player bodies. Only the
// 20 playable roster entities fight in matches.

#include <string>
#include <vector>

namespace cultulhu {

inline const std::vector<std::string>& npcPackageIds() {
    static const std::vector<std::string> kNpcIds = {
        "civilian_guard", "civilian_laborer", "civilian_villager",
        "cultist_hooded", "cultist_magus", "echo_of_the_void"};
    return kNpcIds;
}

inline bool isNpcPackage(const std::string& id) {
    for (const auto& n : npcPackageIds())
        if (id == n) return true;
    return false;
}

} // namespace cultulhu
