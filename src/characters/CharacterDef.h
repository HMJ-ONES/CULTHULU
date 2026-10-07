#pragma once

// CULT-ULHU character archetype definitions (wave 7).
//
// A CharacterDef is pure data: the numbers that describe one playable
// avatar — vitals, the Q/F/R ability kit, the right-click heavy attack,
// which melee combo id it drives, and a passive trait hook.
//
// This module intentionally does NOT bind behavior: the engine layer
// (Unreal binding, later) maps SpellDef.effectKind strings to real
// spell logic, meleeComboId to the input combo system, and passiveId
// to trait hooks. Data here stays engine-agnostic C++17.

#include <string>

namespace cultulhu {

// One of the Q / F / R abilities.
struct SpellDef {
    std::string id;          // unique within the character, e.g. "tentacle_slam"
    std::string name;        // display name, e.g. "Tentacle Slam"
    std::string flavor;      // one-line flavor text
    float cooldownSec = 0.0f;
    float staminaCost = 0.0f;
    float manaCost = 0.0f;   // reserved; most avatars are stamina-driven
    // Effect kind understood by the engine binding, e.g. "aoe_damage",
    // "fear_aura", "summon", "buff". Keep vocabulary small and
    // documented when you add a new kind.
    std::string effectKind;
    float effectPower = 0.0f;  // kind-dependent: damage, aura seconds, etc.
    float range = 0.0f;        // meters; 0 = self-centered / no range
};

// Right-click heavy attack taxonomy. The engine binding decides what each
// kind does in the world; the numbers live here.
enum class HeavyAttackKind {
    MeleeHeavy,    // big slow swing, damage focus
    MindControl,   // ranged domination: low damage, control/CC focus
    AcidSpit,      // ranged projectile, damage over time flavor
    EldritchGrasp, // ranged pull/root: drags a victim toward the caster
};

inline const char* heavyAttackKindName(HeavyAttackKind k) {
    switch (k) {
        case HeavyAttackKind::MeleeHeavy:    return "MeleeHeavy";
        case HeavyAttackKind::MindControl:   return "MindControl";
        case HeavyAttackKind::AcidSpit:      return "AcidSpit";
        case HeavyAttackKind::EldritchGrasp: return "EldritchGrasp";
    }
    return "Unknown";
}

struct HeavyAttackDef {
    HeavyAttackKind kind = HeavyAttackKind::MeleeHeavy;
    std::string name;        // display name, e.g. "Dominate Mind"
    float damageMult = 1.0f; // multiplier on the character's base melee hit
    float range = 2.0f;      // meters
    // CC applied on hit, as a CCType name ("Stun","Slow","Root","Fear").
    // Empty = no crowd control.
    std::string ccType;
    float ccSeconds = 0.0f;  // CC duration; 0 = none
};

// Full definition of one playable character archetype.
struct CharacterDef {
    std::string id;          // registry key, e.g. "cthulhu_avatar"
    std::string displayName; // e.g. "Cthulhu, the Dreaming God"
    std::string flavor;      // one-line description

    // Vitals / locomotion.
    float maxHp = 100.0f;
    float moveSpeed = 5.0f;   // meters per second
    float maxStamina = 100.0f;

    // Ability kit.
    SpellDef qAbility;
    SpellDef fAbility;
    SpellDef rAbility;

    // Id of the timed melee combo this character uses (input combo
    // system; resolved by the engine binding, e.g. "eldritch_flurry").
    std::string meleeComboId;

    // Right-click heavy attack.
    HeavyAttackDef rightClick;

    // Optional state-machine RMB kit id, e.g. "wave_of_domination".
    // When non-empty, the game builds the ability via createRmbAbility()
    // (see characters/abilities/RmbAbility.h) instead of using the plain
    // HeavyAttackDef numbers. This is how per-character RMB kits plug in.
    std::string rmbAbilityId;

    // Passive trait hook: id+description only. Behavior binding arrives
    // with the engine (wave 7+); the id names the hook it will bind to.
    std::string passiveId;
    std::string passiveDesc;
};

} // namespace cultulhu
