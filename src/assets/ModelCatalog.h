#pragma once

// Wave 11 art pass (PART B): logical-name -> model-path lookup for world
// art. Map files and game systems refer to logical names ("altar",
// "ruined_wall", ...); this table resolves them to repo-relative .glb
// paths under assets/world/<category>/ (see assets/world/MANIFEST.md for
// the real CC0 downloads). The core stays engine-agnostic: paths are
// plain strings the Unreal/Unity binding consumes.

#include <string>
#include <unordered_map>

namespace cultulhu {

class ModelCatalog {
public:
    // Full logical-name -> path table (stable iteration for tooling).
    static const std::unordered_map<std::string, std::string>& worldModels();

    // Path for a logical name, or "" when unknown.
    static std::string lookup(const std::string& logical);

    // Wave 12: creature/NPC model table. Keyed by Creature/Monstrosity
    // species string ("deep_one", "tindalos_hound", ...) plus reserved
    // role keys for humanoid NPC entity types: "cultist", "civilian",
    // "adventurer", "sorcerer". Paths live under assets/creatures/ or
    // assets/characters/<pkg>/model.glb.
    static const std::unordered_map<std::string, std::string>& creatureModels();

    // Path for a species/role key, or "" when unknown.
    static std::string creatureModel(const std::string& species);

    // Category of a model path: the directory under assets/world/
    // ("terrain", "buildings", "props", "cult"), assets/creatures/
    // ("creatures"), or assets/characters/<pkg>/ ("characters/<pkg>"),
    // or "" when not a known asset path.
    static std::string categoryOf(const std::string& modelPath);

private:
    ModelCatalog() = delete;
};

} // namespace cultulhu
