#pragma once

// Wave 4: the 12x12 belief interaction matrix. Beliefs do not exist in
// isolation — when two beliefs both burn with high exertion they either
// amplify each other (SYNERGY) or grind against each other (CONFLICT).
// Relations are symmetric. "Active" means both exertions are above the
// synergy/conflict threshold (50); see ExertionSystem.

#include "beliefs/Belief.h"

namespace cultulhu {

enum class BeliefRelation {
    None,
    Synergy,   // both beliefs high -> they amplify each other
    Conflict,  // both beliefs high -> they suppress each other + tension
};

const char* relationName(BeliefRelation r);

// Symmetric relation lookup.
BeliefRelation beliefRelation(Belief a, Belief b);

// True when the pair has the given relation AND both exertions exceed
// ExertionSystem::SYNERGY_THRESHOLD.
bool synergyActive(Belief a, Belief b, const float exertion[12]);
bool conflictActive(Belief a, Belief b, const float exertion[12]);

struct BeliefPair {
    Belief a;
    Belief b;
    const char* name; // flavor name, e.g. "Terror"
};

// Every defined synergy pair (for documentation / README generation).
const BeliefPair* synergyPairs();
int synergyPairCount();

// Every defined conflict pair (used for suppression + tension events).
const BeliefPair* conflictPairs();
int conflictPairCount();

} // namespace cultulhu
