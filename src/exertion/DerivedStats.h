#pragma once

// Wave 4: stats derived from belief exertion, recomputed every tick.
// These are the numbers the rest of the game actually plays with:
// combat power, loyalty drift, and conversion success chance.

#include "beliefs/Belief.h"

namespace cultulhu {

struct DerivedStats {
    float combatPowerMult = 1.0f;   // multiplies all damage your faction deals
    float loyaltyDriftPerSec = 0.0f; // devotion change per cultist per second
    float conversionChance = 0.35f;  // per-attempt conversion success [0.05, 0.95]
};

// exertion: 12 belief exertion levels (0..100).
// insurrectionRisk01: CultManager risk / 100.
// Pure function (no side effects) so tests can drive it directly.
DerivedStats computeDerivedStats(const float exertion[12],
                                 float insurrectionRisk01);

} // namespace cultulhu
