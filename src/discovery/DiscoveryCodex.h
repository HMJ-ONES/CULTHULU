#pragma once

// CULT-ULHU wave 26: the Discovery Codex — the No-Man's-Sky heart of the
// game. The first time the avatar enters a landmark's radius, sights a new
// species, claims a named relic, or (later) delves a new dungeon depth, the
// codex logs it forever: name, flavor, where, when, and whether it happened
// under starlight. Discoveries grant power (knowledge is power, literally)
// and feed exploration achievements. Players can rename discoveries, NMS
// style; names persist through saves.
//
// The codex is feed-driven: call discover() (the driver polls landmarks,
// species, and relics each tick) or subscribe to its DiscoveryMade event.

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
    std::string id;      // "landmark:the_shattered_court" (stable)
    DiscoveryKind kind = DiscoveryKind::Landmark;
    std::string name;    // display name — player-renamable
    std::string flavor;  // one evocative line
    Vec3 pos;
    double gameTime = 0.0;
    bool night = false;  // discovered under starlight (richer reward)
    bool renamed = false;
};

class DiscoveryCodex {
public:
    explicit DiscoveryCodex(EventBus& bus);

    // Log a discovery. Returns true on FIRST discovery (publishes
    // DiscoveryMade: tag = id, amount = power granted, faction = night?1:0,
    // pos = location). Repeats return false and change nothing.
    bool discover(DiscoveryKind kind, const std::string& key,
                  const std::string& name, const std::string& flavor,
                  Vec3 pos, double gameTime, bool night);

    // NMS-style renaming. Returns false for unknown ids.
    bool rename(const std::string& id, const std::string& newName);

    const Discovery* find(const std::string& id) const;
    const std::vector<Discovery>& all() const { return discoveries_; }
    size_t count() const { return discoveries_.size(); }
    size_t countKind(DiscoveryKind k) const;

    void saveTo(GameState& s) const;
    void loadFrom(const GameState& s);

    // Power granted per discovery (night discoveries pay more).
    static float powerReward(bool night) { return night ? 25.0f : 15.0f; }

private:
    static std::string makeId(DiscoveryKind k, const std::string& key);

    EventBus& bus_;
    std::vector<Discovery> discoveries_;
};

} // namespace cultulhu
