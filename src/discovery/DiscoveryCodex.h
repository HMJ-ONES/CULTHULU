#pragma once

// CULT-ULHU wave 26 (revised wave 31): the Discovery Codex — a pure
// exploration journal. The first time the avatar enters a landmark's
// radius, sights a new species, or claims a named relic, the codex logs it
// forever: a game-invented R'lyehian name, a flavor line, where, when, and
// whether it happened under starlight. Discoveries grant no power and
// cannot be renamed — they are simply neat things to find. The codex feeds
// exploration achievements and persists through saves.
//
// The codex is feed-driven: call discover() (the driver polls landmarks,
// species, and relic claims each tick) or subscribe to its DiscoveryMade
// event.

#include "core/EventBus.h"
#include "core/Events.h"
#include "core/Vec3.h"

#include <string>
#include <vector>

namespace cultulhu {

struct GameState; // save/SaveSystem.h (include kept out of the header)

enum class DiscoveryKind {
    Zone,      // first step into a named zone
    Landmark,  // standing stones, drowned towers, shattered courts...
    Species,   // first sighting of a creature species
    Relic,     // claiming a named relic
    Dungeon,   // first descent into a dungeon / depth milestone
    Count
};

inline const char* discoveryKindName(DiscoveryKind k) {
    switch (k) {
        case DiscoveryKind::Zone: return "zone";
        case DiscoveryKind::Landmark: return "landmark";
        case DiscoveryKind::Species: return "species";
        case DiscoveryKind::Relic: return "relic";
        case DiscoveryKind::Dungeon: return "dungeon";
        case DiscoveryKind::Count: break;
    }
    return "unknown";
}

struct Discovery {
    std::string id;      // "landmark:r_lyeh" (stable)
    DiscoveryKind kind = DiscoveryKind::Landmark;
    std::string name;    // game-invented R'lyehian name (not player-renamable)
    std::string flavor;  // one evocative line
    Vec3 pos;
    double gameTime = 0.0;
    bool night = false;  // discovered under starlight
};

class DiscoveryCodex {
public:
    explicit DiscoveryCodex(EventBus& bus);

    // Log a discovery. Returns true on FIRST discovery (publishes
    // DiscoveryMade: tag = id, faction = night?1:0, pos = location).
    // Repeats return false and change nothing. Discoveries grant no power —
    // the log itself is the reward.
    bool discover(DiscoveryKind kind, const std::string& key,
                  const std::string& name, const std::string& flavor,
                  Vec3 pos, double gameTime, bool night);

    const Discovery* find(const std::string& id) const;
    const std::vector<Discovery>& all() const { return discoveries_; }
    size_t count() const { return discoveries_.size(); }
    size_t countKind(DiscoveryKind k) const;

    void saveTo(GameState& s) const;
    void loadFrom(const GameState& s);

    // Stable id for a kind+key (used to check "already discovered?").
    static std::string idFor(DiscoveryKind k, const std::string& key) {
        return makeId(k, key);
    }

private:
    static std::string makeId(DiscoveryKind k, const std::string& key);

    EventBus& bus_;
    std::vector<Discovery> discoveries_;
};

} // namespace cultulhu
