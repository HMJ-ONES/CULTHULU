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

    static constexpr size_t MAX_OPERATIONS = 4;

private:
    void onResolved(const GameEvent& e);
    void spawn(std::unique_ptr<DirectiveOperation> op);

    EventBus& bus_;
    RNG& rng_;
    CultManager& cult_;
    std::vector<std::unique_ptr<DirectiveOperation>> ops_;
};

} // namespace cultulhu
