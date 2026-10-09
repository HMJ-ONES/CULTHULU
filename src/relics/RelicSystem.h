#pragma once

#include "core/Vec3.h"
#include "entities/Entity.h"

#include <cstdint>
#include <string>
#include <vector>

namespace cultulhu {

class EventBus;

// Relics amplify Cthulhu's power gains; cursed artifacts can be left armed
// as traps for opponents (Trickery synergy: ArtifactTriggered feeds fear).
class RelicSystem {
public:
    struct CursedArtifactTrap {
        uint64_t id = 0;
        Vec3 pos;
        FactionId owner = FACTION_NEUTRAL;
        bool armed = true;
    };

    explicit RelicSystem(EventBus& bus);

    // Wave 28: relics are named records with identity, not anonymous
    // floats. The total multiplier is capped at 3x — no unbounded
    // numbers-go-up hole.
    struct HeldRelic {
        std::string name;
        float amplifier = 0.0f;
    };
    static constexpr float MAX_MULTIPLIER = 3.0f;

    // amplifier: e.g. 0.25 => +25% on all power gains while held.
    void addRelic(float amplifier, const std::string& name = "");
    void removeRelic(float amplifier);
    float powerMultiplier() const; // 1 + sum of amplifiers, capped at 3x
    float applyAmplifier(float baseDelta) const {
        return baseDelta * powerMultiplier();
    }
    const std::vector<HeldRelic>& heldRelics() const { return relics_; }

    uint64_t placeCursedArtifact(Vec3 pos, FactionId owner);
    // Springs when an entity of a *different* faction comes within radius.
    // Returns true if the artifact triggered (single-use).
    bool checkTrigger(uint64_t id, const std::vector<const Entity*>& entities,
                      float radius = 6.0f);
    size_t artifactCount() const { return artifacts_.size(); }

    // Wave 16: seize a cursed relic (grants its amplifier) and announce it.
    // Publishes RelicClaimed (sourceId = claimer). This is the hook behind
    // the "Price of Power" achievement.
    void seizeRelic(float amplifier, uint64_t claimerId,
                    const std::string& relicName = "");

private:
    EventBus& bus_;
    std::vector<HeldRelic> relics_;
    std::vector<CursedArtifactTrap> artifacts_;
    uint64_t nextId_ = 1;
};

} // namespace cultulhu
