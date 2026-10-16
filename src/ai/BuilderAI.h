#pragma once

// Wave 7: construction AI. Idle, loyal-enough cultists autonomously start
// and assist builds near their settlement. What gets built is weighted by
// belief exertion: Reconstruction favors repairs, War favors Walls/Barracks,
// Magic favors Altars, Breeding favors Portals, Trickery favors Traps.
// "Loyalty" here is the cultist's devotion (Cultist::devotion(), 0..100):
// cultists below LOYALTY_THRESHOLD shirk and stay idle. When the cult's
// insurrection risk runs hot, nobody feels like building.
//
// The AI polls its construction sites each tick and emits the wave-7 build
// events: BuildStarted when a site breaks ground, BuildProgress throttled
// to 10% steps, BuildCompleted when a site finishes. Finished buildings
// are queued internally; the driver drains them with
// takeFinishedBuildings() and spawns them into the world.

#include "beliefs/Belief.h"
#include "beliefs/BeliefSystem.h"
#include "core/EventBus.h"
#include "core/Events.h"
#include "core/RNG.h"
#include "core/Vec3.h"
#include "cult/CultManager.h"
#include "entities/Structures.h"
#include "entities/Units.h"
#include "exertion/ExertionSystem.h"

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace cultulhu {

struct BuildOrder {
    BuildingType type = BuildingType::Wall;
    Vec3 pos;
    FactionId faction = FACTION_CTHULHU;
    float priority = 1.0f;
    // Repair order: heal an existing building instead of raising a new one.
    bool repair = false;
    uint64_t repairTargetId = 0;
};

struct Settlement {
    Vec3 center;
    float radius = 60.0f; // build orders land inside this disc
    std::vector<BuildOrder> queue;
};

class BuilderAI {
public:
    // Seconds between build decisions, jittered +-25% each pass.
    static constexpr double BUILD_DECISION_INTERVAL = 20.0;
    // Devotion below this: the cultist shirks (stays idle).
    static constexpr float LOYALTY_THRESHOLD = 40.0f;
    // Exertion at/above this counts as "high" for belief weighting.
    static constexpr float HIGH_EXERTION = 50.0f;
    static constexpr size_t MAX_BUILDERS_PER_SITE = 4;
    static constexpr size_t MAX_ACTIVE_SITES_PER_SETTLEMENT = 3;
    // Insurrection risk above this: all construction halts.
    static constexpr float UNREST_HALT_RISK = 70.0f;

    BuilderAI(EventBus& bus, RNG& rng, BeliefSystem& beliefs,
              ExertionSystem& exertion, CultManager& cult);

    // cultists: live roster (usually gathered from CultManager).
    // sites: active construction sites; finished ones are removed here.
    // settlements: build queues live on these (orders are consumed).
    // buildings: existing buildings, for repair targeting.
    void update(double dt, std::vector<Cultist*>& cultists,
                std::vector<std::unique_ptr<ConstructionSite>>& sites,
                std::vector<Settlement>& settlements,
                const std::vector<Building*>& buildings);

    // Drain buildings completed since the last call. The driver spawns
    // these into the world (repair sites produce nothing here).
    std::vector<std::unique_ptr<Building>> takeFinishedBuildings();

    // Exposed for tests: one weighted building-type pick.
    BuildingType pickBuildingType();

private:
    bool eligible(const Cultist* c) const;
    bool assigned(uint64_t cultistId) const;
    size_t activeSitesNear(const std::vector<std::unique_ptr<ConstructionSite>>& sites,
                           const Settlement& s) const;
    bool repairQueuedOrActive(uint64_t targetId, const Settlement& s,
                              const std::vector<std::unique_ptr<ConstructionSite>>& sites) const;
    void decisionPass(std::vector<Cultist*>& cultists,
                      std::vector<std::unique_ptr<ConstructionSite>>& sites,
                      std::vector<Settlement>& settlements,
                      const std::vector<Building*>& buildings);
    void pollSites(std::vector<std::unique_ptr<ConstructionSite>>& sites,
                   const std::vector<Building*>& buildings);
    void emitBuild(EventType type, const ConstructionSite& site, float amount);

    EventBus& bus_;
    RNG& rng_;
    BeliefSystem& beliefs_;
    ExertionSystem& exertion_;
    CultManager& cult_;

    double decisionTimer_ = 0.0;
    double decisionInterval_ = BUILD_DECISION_INTERVAL;

    // cultist ids currently swinging hammers.
    std::unordered_map<uint64_t, uint64_t> builderToSite_;
    // site id -> last emitted progress decile (0..10).
    std::unordered_map<uint64_t, int> progressDecile_;

    std::vector<std::unique_ptr<Building>> finishedBuildings_;
};

} // namespace cultulhu
