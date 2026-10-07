#include "beliefs/InteractionMatrix.h"

namespace cultulhu {

const char* relationName(BeliefRelation r) {
    switch (r) {
        case BeliefRelation::None:     return "None";
        case BeliefRelation::Synergy:  return "Synergy";
        case BeliefRelation::Conflict: return "Conflict";
    }
    return "Unknown";
}

namespace {

// Symmetric 12x12 relation table. Row/col order follows the Belief enum.
constexpr int N = static_cast<int>(Belief::Count);

struct RawPair {
    Belief a, b;
    BeliefRelation rel;
    const char* name;
};

// Defined interactions (creative-liberty design; see README for the full
// table and each pair's gameplay effect).
constexpr RawPair PAIRS[] = {
    // --- SYNERGIES ---
    {Belief::Fear,          Belief::Torture,        BeliefRelation::Synergy,  "Terror"},
    {Belief::Dreams,        Belief::Chaos,          BeliefRelation::Synergy,  "Nightmare Surge"},
    {Belief::War,           Belief::Onslaught,      BeliefRelation::Synergy,  "Blood Frenzy"},
    {Belief::Sacrifice,     Belief::Magic,          BeliefRelation::Synergy,  "Dark Rites"},
    {Belief::Conversion,    Belief::Trickery,        BeliefRelation::Synergy,  "Infiltration"},
    {Belief::Breeding,      Belief::Fear,           BeliefRelation::Synergy,  "Dread Broods"},
    {Belief::War,           Belief::Fear,           BeliefRelation::Synergy,  "Shock and Awe"},
    {Belief::Sacrifice,     Belief::Dreams,         BeliefRelation::Synergy,  "Martyrs' Visions"},
    {Belief::Magic,         Belief::Dreams,         BeliefRelation::Synergy,  "Oneiromancy"},
    {Belief::Torture,       Belief::Chaos,          BeliefRelation::Synergy,  "Cruelty Unbound"},
    // --- CONFLICTS ---
    {Belief::Reconstruction, Belief::Onslaught,     BeliefRelation::Conflict, "Ashes and Scaffolds"},
    {Belief::Chaos,         Belief::Conversion,     BeliefRelation::Conflict, "Sabotage"},
    {Belief::Chaos,         Belief::Sacrifice,      BeliefRelation::Conflict, "Desecrated Rites"},
    {Belief::Reconstruction, Belief::War,           BeliefRelation::Conflict, "Builders vs Warriors"},
    {Belief::Torture,       Belief::Sacrifice,      BeliefRelation::Conflict, "Waste Not"},
};

constexpr int PAIR_COUNT = sizeof(PAIRS) / sizeof(PAIRS[0]);

} // namespace

BeliefRelation beliefRelation(Belief a, Belief b) {
    if (a == b) return BeliefRelation::None;
    for (int i = 0; i < PAIR_COUNT; ++i) {
        if ((PAIRS[i].a == a && PAIRS[i].b == b) ||
            (PAIRS[i].a == b && PAIRS[i].b == a))
            return PAIRS[i].rel;
    }
    return BeliefRelation::None;
}

namespace {
// ExertionSystem::SYNERGY_THRESHOLD duplicated here to keep this module
// dependency-free (both must stay 50; asserted in the wave-4 tests).
constexpr float THRESHOLD = 50.0f;

bool pairHot(Belief a, Belief b, BeliefRelation want, const float exertion[12]) {
    if (beliefRelation(a, b) != want) return false;
    return exertion[static_cast<int>(a)] > THRESHOLD &&
           exertion[static_cast<int>(b)] > THRESHOLD;
}
} // namespace

bool synergyActive(Belief a, Belief b, const float exertion[12]) {
    return pairHot(a, b, BeliefRelation::Synergy, exertion);
}

bool conflictActive(Belief a, Belief b, const float exertion[12]) {
    return pairHot(a, b, BeliefRelation::Conflict, exertion);
}

namespace {
BeliefPair toPair(const RawPair& p) { return {p.a, p.b, p.name}; }
} // namespace

const BeliefPair* synergyPairs() {
    static BeliefPair out[PAIR_COUNT];
    static bool built = false;
    if (!built) {
        int n = 0;
        for (int i = 0; i < PAIR_COUNT; ++i)
            if (PAIRS[i].rel == BeliefRelation::Synergy) out[n++] = toPair(PAIRS[i]);
        built = true;
    }
    return out;
}

int synergyPairCount() {
    int n = 0;
    for (int i = 0; i < PAIR_COUNT; ++i)
        if (PAIRS[i].rel == BeliefRelation::Synergy) ++n;
    return n;
}

const BeliefPair* conflictPairs() {
    static BeliefPair out[PAIR_COUNT];
    static bool built = false;
    if (!built) {
        int n = 0;
        for (int i = 0; i < PAIR_COUNT; ++i)
            if (PAIRS[i].rel == BeliefRelation::Conflict) out[n++] = toPair(PAIRS[i]);
        built = true;
    }
    return out;
}

int conflictPairCount() {
    int n = 0;
    for (int i = 0; i < PAIR_COUNT; ++i)
        if (PAIRS[i].rel == BeliefRelation::Conflict) ++n;
    return n;
}

} // namespace cultulhu
