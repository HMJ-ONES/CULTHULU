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
        // eldritch battlefield (wave 17): Kenney Graveyard/Nature + KayKit
        // Dungeon/Halloween, all CC0. See assets/world/MANIFEST.md.
        {"obelisk",             "assets/world/props/grave-obelisk.glb"},
        {"bone_pile",           "assets/world/props/bone-pile.glb"},
        {"dead_trunk",          "assets/world/props/dead-trunk.glb"},
        {"dead_trunk_long",     "assets/world/props/dead-trunk-long.glb"},
        {"tall_rocks",          "assets/world/props/rocks-tall.glb"},
        {"dead_pine_crooked",   "assets/world/props/pine-dead-crooked.glb"},
        {"ruined_crypt",        "assets/world/buildings/crypt-ruined.glb"},
        {"small_crypt",         "assets/world/buildings/crypt-small.glb"},
        {"cave_boulder",        "assets/world/props/cave-boulder-a.glb"},
        {"cave_boulder_large",  "assets/world/props/cave-boulder-c.glb"},
        {"cave_rock_tall",      "assets/world/props/cave-rock-tall-a.glb"},
        {"cave_spire",          "assets/world/props/cave-rock-tall-c.glb"},
        {"cave_wall_rock",      "assets/world/props/cave-wall-rock.glb"},
        {"dark_oak",            "assets/world/props/tree-dark-oak.glb"},
        {"dark_cone",           "assets/world/props/tree-dark-cone.glb"},
        {"dark_pine_tall",      "assets/world/props/tree-dark-tall.glb"},
        {"dungeon_pillar",      "assets/world/props/dungeon-pillar.glb"},
        {"torch_stand",         "assets/world/props/torch-stand.glb"},
        {"torch_wall",          "assets/world/props/torch-wall.glb"},
        {"old_chest",           "assets/world/props/chest-old.glb"},
        {"tattered_banner",     "assets/world/props/banner-tattered.glb"},
        {"spike_trap",          "assets/world/props/spike-trap.glb"},
        {"carved_tombstone",    "assets/world/props/tombstone-carved.glb"},
        {"weathered_tombstone", "assets/world/props/tombstone-weathered.glb"},
        {"bare_dead_tree",      "assets/world/props/tree-dead-bare.glb"},
        {"iron_lantern",        "assets/world/props/lantern-iron.glb"},
        {"candle_cluster",      "assets/world/props/candles-cluster.glb"},
        // eldritch battlefield, second dark-art pass (wave 19): more KayKit
        // Halloween Bits / Dungeon Remastered (CC0) + in-house procgen
        // atmosphere props (mist, scorch, embers, beams, dead bushes).
        // See assets/world/MANIFEST.md.
        {"stone_arch",      "assets/world/props/arch-ruined.glb"},
        {"dark_gate",       "assets/world/props/arch-gate.glb"},
        {"scattered_bones", "assets/world/props/bones-scattered.glb"},
        {"ribcage",         "assets/world/props/ribcage.glb"},
        {"skull",           "assets/world/props/skull.glb"},
        {"skull_candle",    "assets/world/props/skull-candle.glb"},
        {"stone_coffin",    "assets/world/props/coffin-stone.glb"},
        {"dead_tree_large", "assets/world/props/tree-dead-large.glb"},
        {"broken_fence",    "assets/world/props/fence-broken.glb"},
        {"ruined_stairs",   "assets/world/buildings/stairs-ruined.glb"},
        {"large_rubble",    "assets/world/props/rubble-large.glb"},
        {"broken_step",     "assets/world/props/broken-step.glb"},
        {"dungeon_arch",    "assets/world/buildings/arch-dungeon.glb"},
        {"broken_wall",     "assets/world/buildings/wall-broken.glb"},
        {"mist_bank",       "assets/world/props/mist-bank.glb"},
        {"scorched_earth",  "assets/world/terrain/scorched-patch.glb"},
        {"ember_cluster",   "assets/world/props/ember-cluster.glb"},
        {"collapsed_beams", "assets/world/props/beams-collapsed.glb"},
        {"dead_bush",       "assets/world/props/dead-bush.glb"},
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
