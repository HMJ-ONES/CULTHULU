#pragma once

// Wave 16: achievements — the game's progression system. No currencies, no
// shops, no loot boxes: recognition for real deeds, tracked off the
// EventBus. Single-player achievements use lifetime counters; multiplayer
// ones are scoped to the local player and (where noted) to the match.
//
// The system never fakes a trigger: every unlock is driven by a real
// gameplay event published at its source system.

#include "core/EventBus.h"
#include "core/Events.h"
#include "entities/Entity.h" // FACTION_CTHULHU / FACTION_NEUTRAL

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace cultulhu {

struct GameState; // save/SaveSystem.h (include kept out of the header)

struct AchievementDef {
    std::string id;          // stable string id, e.g. "first_flesh"
    std::string name;        // display name, e.g. "First Flesh"
    std::string description; // one grim line
    // Locked-progress display: "<current>/<target> <unit>". Empty unit =
    // one-shot (no progress shown).
    std::string progressUnit;
    double progressTarget = 0.0;
    bool multiplayer = false;
};

class AchievementSystem {
public:
    explicit AchievementSystem(EventBus& bus);

    // Who "you" are for multiplayer achievements. faction defaults to
    // Cthulhu's; team -1 = unknown (team achievements stay locked).
    void setLocalPlayer(uint64_t entityId,
                        int faction = FACTION_CTHULHU, int team = -1);

    const std::vector<AchievementDef>& defs() const { return defs_; }
    bool isUnlocked(const std::string& id) const;
    std::vector<std::string> unlockedIds() const;

    // Progress toward a locked achievement: {current, target}. Returns
    // {-1, -1} for one-shot achievements with no progress display.
    std::pair<double, double> progress(const std::string& id) const;

    // Persistence through the save system.
    void saveTo(GameState& s) const;
    void loadFrom(const GameState& s);

    // Counter names used in save files (also handy for tests).
    static const char* kConvTotal;
    static const char* kConvDream;
    static const char* kDistrictsRazed;
    static const char* kBuildingsDestroyed;
    static const char* kCitiesDestroyed;
    static const char* kRelicsClaimed;
    static const char* kValeRelics;
    static const char* kChampionsSummoned;
    static const char* kDholesSlain;
    static const char* kPvpKillsLifetime;
    static const char* kMatchPvpKills;
    static const char* kMatchDeaths;
    static const char* kDiscoveries;      // wave 26: codex first-finds
    static const char* kNightDiscoveries; // wave 26: found under starlight

private:
    void onEvent(const GameEvent& e);
    void unlock(const std::string& id);
    double counter(const char* name) const;
    void bump(const char* name, double amount);

    EventBus& bus_;
    std::vector<AchievementDef> defs_;
    std::map<std::string, bool> unlocked_;
    std::map<std::string, double> counters_;

    uint64_t localId_ = 0;
    int localFaction_ = FACTION_CTHULHU;
    int localTeam_ = -1;
};

} // namespace cultulhu
