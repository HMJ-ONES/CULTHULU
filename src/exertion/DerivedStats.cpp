#include "exertion/DerivedStats.h"

#include "beliefs/InteractionMatrix.h"

#include <algorithm>

namespace cultulhu {

namespace {
int bi(Belief b) { return static_cast<int>(b); }

float clampf(float v, float lo, float hi) {
    return v < lo ? lo : (v > hi ? hi : v);
}
} // namespace

DerivedStats computeDerivedStats(const float exertion[12],
                                 float insurrectionRisk01) {
    DerivedStats s;

    // Combat power: War and Onslaught exertion feed the war machine, Magic
    // sharpens it. Blood Frenzy (War x Onslaught synergy) adds a surge.
    s.combatPowerMult = 1.0f
        + 0.004f * (exertion[bi(Belief::War)] + exertion[bi(Belief::Onslaught)])
        + 0.002f * exertion[bi(Belief::Magic)];
    if (synergyActive(Belief::War, Belief::Onslaught, exertion))
        s.combatPowerMult += 0.15f;

    // Loyalty drift: devotional beliefs (Sacrifice, Dreams, Conversion) pull
    // cultists closer; Chaos erodes devotion; insurrection pressure drags
    // everyone down with it.
    s.loyaltyDriftPerSec =
          0.01f * (exertion[bi(Belief::Sacrifice)]
                 + exertion[bi(Belief::Dreams)]
                 + exertion[bi(Belief::Conversion)]) / 100.0f
        - 0.02f * exertion[bi(Belief::Chaos)] / 100.0f
        - 0.015f * insurrectionRisk01;

    // Conversion success: the Conversion creed does the heavy lifting, with
    // Trickery (infiltration) and Magic (ritual power) assisting. The
    // Infiltration synergy (Conversion x Trickery) opens doors; the Sabotage
    // conflict (Chaos x Conversion) has lunatics wrecking the campaigns.
    float conv = 0.35f
        + 0.004f * exertion[bi(Belief::Conversion)]
        + 0.002f * (exertion[bi(Belief::Trickery)]
                  + exertion[bi(Belief::Magic)]);
    if (synergyActive(Belief::Conversion, Belief::Trickery, exertion))
        conv += 0.10f;
    if (conflictActive(Belief::Chaos, Belief::Conversion, exertion))
        conv -= 0.10f;
    s.conversionChance = clampf(conv, 0.05f, 0.95f);

    return s;
}

} // namespace cultulhu
