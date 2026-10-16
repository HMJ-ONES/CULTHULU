#include "animation/WarLaborAnimHooks.h"

#include "animation/AnimationDirector.h"
#include "animation/AnimationStateMachine.h"
#include "core/EventBus.h"
#include "cult/CultManager.h"

namespace cultulhu {

WarLaborAnimHooks::WarLaborAnimHooks(
    EventBus& bus, AnimationDirector& director,
    const std::vector<std::unique_ptr<ConstructionSite>>& sites,
    CultManager* cult)
    : director_(director), sites_(sites), cult_(cult) {
    // Disciplined war fighting: both combatants take the WarBattle loop.
    bus.subscribe(EventType::WarEngagement, [this](const GameEvent& e) {
        director_.requestState(e.sourceId, AnimationState::WarBattle);
        if (e.targetId != 0 && e.targetId != e.sourceId)
            director_.requestState(e.targetId, AnimationState::WarBattle);
    });
    // Construction/repair sites: track while active; builders are
    // resolved from the site (BuildStarted fires before the first
    // builder attaches, so update() reconciles every tick).
    bus.subscribe(EventType::BuildStarted,
                  [this](const GameEvent& e) { onBuildEvent(e); });
    bus.subscribe(EventType::BuildProgress,
                  [this](const GameEvent& e) { onBuildEvent(e); });
    bus.subscribe(EventType::BuildCompleted,
                  [this](const GameEvent& e) { onBuildCompleted(e); });
    // RebuildSanctum directive: no builder ids — the whole cult is the
    // work crew. Ticks hold Repair; SanctumRebuilt stands the crew down.
    bus.subscribe(EventType::SanctumRebuiltTick,
                  [this](const GameEvent& e) { onSanctumTick(e); });
    bus.subscribe(EventType::SanctumRebuilt,
                  [this](const GameEvent& e) { onSanctumDone(e); });
}

const ConstructionSite*
WarLaborAnimHooks::findSite(uint64_t siteId) const {
    for (const auto& site : sites_) {
        if (site && site->id() == siteId) return site.get();
    }
    return nullptr;
}

void WarLaborAnimHooks::assertSiteBuilders(uint64_t siteId) {
    const ConstructionSite* site = findSite(siteId);
    if (!site) return;
    // Repair sites mend; build sites erect.
    const AnimationState labor =
        site->isRepair() ? AnimationState::Repair : AnimationState::Build;
    for (uint64_t builderId : site->builderIds())
        director_.requestState(builderId, labor);
}

void WarLaborAnimHooks::setSiteBuilders(uint64_t siteId, bool toIdle) {
    const ConstructionSite* site = findSite(siteId);
    if (!site) return;
    for (uint64_t builderId : site->builderIds())
        director_.requestState(builderId,
                               toIdle ? AnimationState::Idle
                                      : (site->isRepair() ? AnimationState::Repair
                                                          : AnimationState::Build));
}

void WarLaborAnimHooks::onBuildEvent(const GameEvent& e) {
    activeSites_.insert(e.sourceId);
    assertSiteBuilders(e.sourceId);
}

void WarLaborAnimHooks::onBuildCompleted(const GameEvent& e) {
    setSiteBuilders(e.sourceId, true);
    activeSites_.erase(e.sourceId);
}

void WarLaborAnimHooks::onSanctumTick(const GameEvent& e) {
    (void)e;
    if (!cult_) return;
    for (size_t i = 0; i < cult_->size(); ++i) {
        const Cultist& c = cult_->at(i);
        if (!c.alive()) continue;
        if (director_.requestState(c.id(), AnimationState::Repair))
            sanctumCrew_.insert(c.id());
    }
}

void WarLaborAnimHooks::onSanctumDone(const GameEvent& e) {
    (void)e;
    // Stand down only the crew we put on Repair; dead/unknown ids are
    // safe no-ops through the director.
    for (uint64_t id : sanctumCrew_)
        director_.requestState(id, AnimationState::Idle);
    sanctumCrew_.clear();
}

void WarLaborAnimHooks::update(double dt) {
    (void)dt;
    // Builders attach after BuildStarted and top up mid-build, so
    // re-assert every active site's labor state each tick.
    for (uint64_t siteId : activeSites_) assertSiteBuilders(siteId);
}

} // namespace cultulhu
