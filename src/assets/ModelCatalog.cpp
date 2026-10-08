// Wave 11 art pass (PART B): world model catalog. See ModelCatalog.h.

#include "assets/ModelCatalog.h"

namespace cultulhu {

const std::unordered_map<std::string, std::string>&
ModelCatalog::worldModels() {
    // Reconciled 2026-10-07 with assets/world/MANIFEST.md: real CC0
    // (Kenney) downloads. Logical names describe the role in the scene;
    // paths are the actual files.
    static const std::unordered_map<std::string, std::string> kModels = {
        // cult base
        {"altar",       "assets/world/cult/altar-stone.glb"},
        {"shelter",     "assets/world/cult/wood-structure.glb"},
        {"brazier",     "assets/world/cult/fire-basket.glb"},
        {"cult_banner", "assets/world/cult/banner.glb"},
        // ruined city
        {"ruined_wall",     "assets/world/buildings/stone-wall-damaged.glb"},
        {"collapsed_tower", "assets/world/buildings/tower-square-base.glb"},
        {"rubble",          "assets/world/props/debris.glb"},
        {"broken_column",   "assets/world/buildings/stone-wall-column.glb"},
        {"ruined_arch",     "assets/world/buildings/wall-doorway.glb"},
        // dark props
        {"tombstone",  "assets/world/props/gravestone-cross.glb"},
        {"dead_tree",  "assets/world/props/pine-crooked.glb"},
        {"iron_fence", "assets/world/props/iron-fence.glb"},
    };
    return kModels;
}

std::string ModelCatalog::lookup(const std::string& logical) {
    const auto& m = worldModels();
    const auto it = m.find(logical);
    return it != m.end() ? it->second : std::string();
}

const std::unordered_map<std::string, std::string>&
ModelCatalog::creatureModels() {
    // Wave 12: filled in as CC0 creature/NPC assets land. Keys are
    // Creature/Monstrosity species strings plus the reserved humanoid
    // role keys ("cultist", "civilian", "adventurer", "sorcerer").
    // See assets/creatures/MANIFEST.md for sources and licenses.
    static const std::unordered_map<std::string, std::string> kCreatures = {
        // reserved role keys (humanoid NPCs via character packages)
        {"cultist",   "assets/characters/cultist_hooded/model.glb"},
        {"civilian",  "assets/characters/civilian_villager/model.glb"},
        {"adventurer","assets/characters/civilian_guard/model.glb"},
        {"sorcerer",  "assets/characters/cultist_magus/model.glb"},
        // wave 12 bestiary: KayKit Skeletons (CC0), animation clips
        // stripped for size; skeletons kept, procedural anims drive them.
        {"pale_wight",      "assets/creatures/pale_wight.glb"},
        {"ossified_brute",  "assets/creatures/ossified_brute.glb"},
        {"charnel_imp",     "assets/creatures/charnel_imp.glb"},
        {"skittering_ghoul","assets/creatures/skittering_ghoul.glb"},
        // Kenney Graveyard Kit (CC0) — static (no skeleton), same
        // low-poly style as the wave-11 ruined city.
        {"wraith",          "assets/creatures/wraith.glb"},
        {"risen_dead",      "assets/creatures/risen_dead.glb"},
        // Kenney Mini Dungeon (CC0) — green brute, re-themed as a
        // deep-one hybrid.
        {"dagon_spawn",     "assets/creatures/dagon_spawn.glb"},
        // Wave 13: dhole model slot. No CC0 dhole model exists in any
        // surveyed source; the empty path is INTENTIONAL — creatureModel()
        // returns "" and the engine uses the procedural serpent/worm-like
        // fallback (see Dhole in entities/Units.h and the README
        // bestiary). tests_wave12 skips empty placeholder paths.
        {"dhole",           ""},
    };
    return kCreatures;
}

std::string ModelCatalog::creatureModel(const std::string& species) {
    const auto& m = creatureModels();
    const auto it = m.find(species);
    return it != m.end() ? it->second : std::string();
}

std::string ModelCatalog::categoryOf(const std::string& modelPath) {
    const std::string worldPrefix = "assets/world/";
    if (modelPath.compare(0, worldPrefix.size(), worldPrefix) == 0) {
        const size_t slash = modelPath.find('/', worldPrefix.size());
        if (slash == std::string::npos) return {};
        return modelPath.substr(worldPrefix.size(), slash - worldPrefix.size());
    }
    const std::string creaturesPrefix = "assets/creatures/";
    if (modelPath.compare(0, creaturesPrefix.size(), creaturesPrefix) == 0)
        return "creatures";
    const std::string charsPrefix = "assets/characters/";
    if (modelPath.compare(0, charsPrefix.size(), charsPrefix) == 0) {
        const size_t slash = modelPath.find('/', charsPrefix.size());
        if (slash == std::string::npos) return {};
        return "characters/" +
               modelPath.substr(charsPrefix.size(), slash - charsPrefix.size());
    }
    return {};
}

} // namespace cultulhu
