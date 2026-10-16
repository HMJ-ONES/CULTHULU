#pragma once

#include "beliefs/Belief.h"
#include "core/Vec3.h"

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace cultulhu {

// Plain-data snapshot of the game state, serializable to / from a text file.
struct GameState {
    struct EntityRec {
        uint64_t id = 0;
        int type = 0;      // EntityType as int
        int faction = -1;
        Vec3 pos;
        float hp = 0.0f;
        float maxHp = 0.0f;
    };

    double clockTime = 0.0;
    float power = 0.0f;
    std::vector<Belief> activeBeliefs;
    float insurrectionRisk = 0.0f;
    std::vector<EntityRec> entities;
    // Wave 16: achievements — unlocked ids + named progress counters.
    std::vector<std::string> unlockedAchievements;
    std::map<std::string, double> achievementProgress;
    // Wave 26: discovery codex records (wave 31: no `renamed` field).
    struct DiscoveryRec {
        std::string id;
        int kind = 0; // DiscoveryKind as int
        std::string name;
        std::string flavor;
        Vec3 pos;
        double gameTime = 0.0;
        bool night = false;
    };
    std::vector<DiscoveryRec> discoveries;
};

// Simple human-readable text format (see SaveSystem.cpp header comment).
class SaveSystem {
public:
    static bool save(const GameState& s, const std::string& path);
    static bool load(const std::string& path, GameState& out);
};

} // namespace cultulhu
