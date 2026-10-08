#pragma once

#include "core/Events.h"
#include "entities/Structures.h"

#include <cstdint>
#include <memory>
#include <unordered_set>
#include <vector>

namespace cultulhu {

class AnimationDirector;
class CultManager;
class EventBus;

// Wave 18 (extension): behavior hooks for WarBattle, Build and Repair.
// Delegates entity-id resolution to the AnimationDirector (the wave-18
// plumbing: every Entity owns its AnimationStateMachine, and the director
// tracks live entities by id), so this class only maps EVENTS to states:
//
//   WarBattle <- WarEngagement (both combatants; published by
//                combat::resolveAttack for cultist-on-cultist melee —
//                disciplined war fighting, not a brawl, not a beast maul)
//   Build     <- BuildStarted/BuildProgress name a construction site; the
//                site's builders hold Build until BuildCompleted -> Idle
//   Repair    <- the same site events when the site is a repair site, plus
//                the RebuildSanctum directive: SanctumRebuiltTick puts the
//                cult work crew on Repair, SanctumRebuilt stands them down
//
// Wiring: the game layer constructs one alongside the AnimationDirector,
// keeps it alive, and calls update() each tick (it reconciles active
// sites — builders attach after BuildStarted fires and top up mid-build).
// Animation machines themselves advance through AnimationDirector::tick().
class WarLaborAnimHooks {
public:
    // sites: the live construction/repair site list (owned by the caller;
    // must outlive these hooks). Build events name site ids; builders are
    // resolved from the site. cult: optional roster for the RebuildSanctum
    // directive path (no builder ids there; the whole cult is the work
    // crew). May be nullptr.
    WarLaborAnimHooks(
        EventBus& bus, AnimationDirector& director,
        const std::vector<std::unique_ptr<ConstructionSite>>& sites,
        CultManager* cult = nullptr);

    // Reconcile active build/repair sites (late-joining builders included).
    void update(double dt);

private:
    AnimationDirector& director_;
    const std::vector<std::unique_ptr<ConstructionSite>>& sites_;
    CultManager* cult_;
    std::unordered_set<uint64_t> activeSites_;
    std::unordered_set<uint64_t> sanctumCrew_;

    const ConstructionSite* findSite(uint64_t siteId) const;
    void setSiteBuilders(uint64_t siteId, bool toIdle);
    void assertSiteBuilders(uint64_t siteId);
    void onBuildEvent(const GameEvent& e);
    void onBuildCompleted(const GameEvent& e);
    void onSanctumTick(const GameEvent& e);
    void onSanctumDone(const GameEvent& e);
};

} // namespace cultulhu
