#pragma once

#include "beliefs/Belief.h"

#include <cstddef>

namespace cultulhu {

class EventBus;
class RNG;
class BeliefSystem;
class CultManager;

// Chaos belief: cultists may become lunatic and perform actions that go
// against the other active beliefs. Lunatics misbehave on a cooldown; each
// act publishes LunaticActed plus the matching sabotage event, so the
// belief power rules react naturally (e.g. an interrupted sacrifice still
// costs power). Killing or punishing a lunatic to "align" them raises
// insurrection risk, per the doc.
class LunaticSystem {
public:
    // lunaticRate: expected new lunatics per cultist per game-second.
    // actInterval: game-seconds between misbehavior acts per lunatic wave.
    LunaticSystem(EventBus& bus, RNG& rng, BeliefSystem& beliefs,
                  CultManager& cult, double lunaticRate = 0.0005,
                  double actInterval = 60.0);

    // Roll for new lunatics (only while Chaos is active) and let existing
    // lunatics misbehave.
    void update(double dt);

    // Kill/punish the lunatic at cult roster index to align them with the
    // other beliefs. Publishes LunaticAligned and raises insurrection risk.
    void alignLunatic(size_t index);

    size_t lunaticCount() const;

private:
    EventBus& bus_;
    RNG& rng_;
    BeliefSystem& beliefs_;
    CultManager& cult_;
    double lunaticRate_;
    double actInterval_;
    double actTimer_ = 0.0;

    void misbehave();                 // one lunatic performs one act
    void misbehaveAgainst(Belief target);
};

} // namespace cultulhu
