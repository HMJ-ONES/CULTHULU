#pragma once

#include "core/EventBus.h"
#include "core/Events.h"
#include "core/RNG.h"
#include "core/Vec3.h"

#include <memory>
#include <string>
#include <vector>

namespace cultulhu {

class CultManager;
class ExertionSystem;
class PowerSystem;
class WorldMap;

// Wave 9b: optional game-layer context for directive follow-through
// operations. The executor works without it (operations degrade
// gracefully), but the new directives need live systems:
//   power      - GrandSummoning consumes 300 power at operation start
//                (fails when power < 300); BlightLand trickles power.
//   exertion   - AssassinateProphet reads live Trickery exertion for the
//                per-tick strike chance.
//   worldMap   - BlightLand resolves the zone at the target position and
//                flags it Blighted on completion.
//   targetEntityId - enemy leader entity id for AssassinateProphet (set
//                by the game layer right before issuing the directive;
//                the spawn reads it synchronously during issueCommand).
struct DirectiveContext {
    PowerSystem* power = nullptr;
    ExertionSystem* exertion = nullptr;
    WorldMap* worldMap = nullptr;
    uint64_t targetEntityId = 0;
};

// A live, ticking follow-through for an obeyed eldritch directive.
//
// The executor subscribes to DirectiveResolved on the bus. Obeyed and
// PartiallyObeyed outcomes spawn the matching operation; Refused and
// SparksInsurrection spawn nothing. Each operation ticks over game time and
// emits ordinary gameplay events (RaidPerformed, EnemyCultistSlain,
// ConversionPerformed, SacrificeCompleted, ArtifactTriggered, ...) so the
// existing exertion/power pipeline picks them up automatically — there is
// no parallel power path.
//
// Concrete operations (PartiallyObeyed halves magnitudes AND durations):
//   GoToWar        -> WarOperation:       skirmish ticks, EnemyCultistSlain
//   RaidCity       -> RaidOperation:      raid ticks, RaidPerformed (0.1-0.3)
//   ConvertCampaign-> ConvertOperation:   conversion ticks, ConversionPerformed
//   MassSacrifice  -> SacrificeOperation: 30s ritual; SacrificeCompleted, or
//                                          SacrificeInterrupted on interruption
//   Defend         -> DefendOperation:     duration buff window; progress +
//                                          completion only (see defenseActive)
//   GatherRelic    -> RelicOperation:      travel ticks, then 60% chance of
//                                          ArtifactTriggered (tag "relic")
//
// Wave 9b:
//   AssassinateProphet -> AssassinateOperation: infiltration approach ticks;
//                          per-tick strike roll from live Trickery exertion
//                          and cult-to-target distance. Success =
//                          LeaderAssassinated + EnemyMoraleShocked (120s);
//                          exposure = AssassinExposed (target escapes) +
//                          insurrection nudge + CombatStarted "deity_noticed".
//                          Resolves early on success/exposure; a quiet
//                          timeout just completes.
//   BlightLand       -> BlightOperation:      120 corruption ticks (1s);
//                          ZoneBlightTick per tick (the city system reads it
//                          to cut civilian output), Fear exertion rises, and
//                          cultists in the zone trickle power. Completion
//                          flags the zone Blighted via the context world map.
//   GrandSummoning   -> SummoningOperation:   90s ritual; 300 power consumed
//                          at start (fails when power < 300, no operation).
//                          2% interruption per tick, no refund. Completion =
//                          ChampionSummoned (the game layer spawns a boosted
//                          Monstrosity: the dread champion, 1200 HP).
//
// Wave 15:
//   OneiricHarvest   -> DreamHarvestOperation: 60s mass dream-rite; each
//                          tick the cult's loyal dreamers channel a vision
//                          (DreamShared tag "directive_dream"). Completion =
//                          OneiricHarvestCompleted + harvested dreams are
//                          distilled into power (3 per dreamer; skipped when
//                          no PowerSystem is attached).
//   RebuildSanctum   -> RebuildOperation:      60s rebuilding; per-tick
//                          SanctumRebuiltTick, then on completion a
//                          BuildingRebuilt (reads as restored infrastructure
//                          to every listener) + SanctumRebuilt + a devotion
//                          bump for the whole cult.
//
// Operation context (power/exertion/world map/target entity) is attached via
// DirectiveExecutor::setContext; unset entries degrade gracefully:
//   - no PowerSystem: GrandSummoning always fails (insufficient_power);
//     BlightLand skips the power trickle. OneiricHarvest skips the
//     dream-power distillation.
//   - no ExertionSystem: AssassinateProphet strikes at base chance.
//   - no WorldMap: BlightLand corrupts "the wilds" around the target point
//     and ZoneBlighted carries amount 0 (no persistent flag).
//
// Buff hook: the executor does NOT apply a defense buff itself. While any
// Defend operation is live, defenseActive() returns true; game layers
// (driver, engine binding) consult it to apply whatever buff they want
// (e.g. halve incoming damage) for the duration.
class DirectiveOperation {
public:
    DirectiveOperation(EventBus& bus, RNG& rng, CultManager& cult,
                       std::string directiveName, double durationSeconds,
                       double tickIntervalSeconds, bool partial);
    virtual ~DirectiveOperation() = default;

    // Advance the operation. Drives the tick timer (onTick per interval),
    // publishes DirectiveProgress, and on natural completion runs
    // onComplete() then publishes DirectiveCompleted.
    virtual void update(double dt);
    virtual bool done() const { return finished_; }
    // e.g. "RaidCity 45% (partial)"
    virtual std::string describe() const;

    const std::string& name() const { return name_; }
    float progress() const; // 0..1

protected:
    virtual void onTick() = 0;      // one gameplay tick of the operation
    virtual void onComplete() = 0;  // ran its full duration
    // End the operation WITHOUT onComplete / DirectiveCompleted
    // (e.g. a sacrifice ritual being interrupted).
    void finishEarly();

    float magnitudeScale() const { return partial_ ? 0.5f : 1.0f; }
    bool partial() const { return partial_; }

    EventBus& bus_;
    RNG& rng_;
    CultManager& cult_;

private:
    void publishProgress() const;

    std::string name_;
    double duration_;
    double tickInterval_;
    double elapsed_ = 0.0;
    double tickAcc_ = 0.0;
    bool partial_;
    bool finished_ = false;
};

class DirectiveExecutor {
public:
    DirectiveExecutor(EventBus& bus, RNG& rng, CultManager& cult);

    // Advance all live operations; finished ones are retired.
    void update(double dt);
    size_t activeCount() const { return ops_.size(); }

    // Buff hook: true while any Defend directive is being carried out.
    bool defenseActive() const;

    // Wave 9b: attach game-layer context (power/exertion/world map) used
    // by the new directive operations. Optional; unset entries degrade
    // gracefully (documented per operation).
    void setContext(DirectiveContext ctx) { ctx_ = ctx; }
    const DirectiveContext& context() const { return ctx_; }

    static constexpr size_t MAX_OPERATIONS = 4;

private:
    void onResolved(const GameEvent& e);
    void spawn(std::unique_ptr<DirectiveOperation> op);

    EventBus& bus_;
    RNG& rng_;
    CultManager& cult_;
    DirectiveContext ctx_;
    std::vector<std::unique_ptr<DirectiveOperation>> ops_;
};

} // namespace cultulhu
