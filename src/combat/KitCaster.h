#pragma once

// CULT-ULHU wave 27: cast character-kit abilities (Q/F/R) through real
// combat. Resolves a SpellDef's effect kind into damage, crowd control,
// summons, movement, and support effects using the existing combat and
// crowd-control systems — so the 20 kits are playable, not just data.

#include "core/Vec3.h"

#include <string>
#include <utility>
#include <vector>

namespace cultulhu {

class ActiveEffects;
class BeliefSystem;
class Entity;
class EventBus;
struct SpellDef;

struct KitCastResult {
    bool ok = false;
    std::string message;   // one narration line for the driver/UI
    float damageDealt = 0.0f;
    // summon kinds: (species, count) — the caller spawns them.
    std::vector<std::pair<std::string, int>> summons;
    Vec3 dash{0, 0, 0};    // displacement for dash kinds
    float buffMult = 1.0f; // buff kinds: damage multiplier granted
    float buffSeconds = 0.0f;
};

// Cast spell from caster. target = single-target focus (may be null for
// self-centered kinds). foes = nearby hostiles for aura kinds. powerMult
// scales damage (belief/exertion pipeline). Unknown effect kinds fizzle
// with an explanatory message instead of crashing.
KitCastResult castKitSpell(const SpellDef& spell, Entity& caster,
                           Entity* target,
                           const std::vector<Entity*>& foes,
                           BeliefSystem& beliefs, EventBus& bus,
                           ActiveEffects& fx, float powerMult);

} // namespace cultulhu
