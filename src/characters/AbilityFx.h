#pragma once

// CULT-ULHU ability visual-effect (particle) data (wave 36).
//
// Engine-agnostic particle descriptions for the 20 character kits'
// Q/F/R abilities. The core game only *describes* effects; the engine
// binding (Unreal, later) turns an FxPreset into a real Niagara /
// UParticleSystem. All numbers are tuned for low-spec hosts: per-burst
// particle counts stay small (<= 256, <= 128 for additive glows,
// <= 64 for looping emitters), additive blending is reserved for magic
// glows, and emitters are cheap primitives (point/cone/ring/sphere/
// beam/wall) — no GPU-heavy solvers, no per-particle textures beyond a
// small shared hint set.
//
// Resolution rule: a SpellDef with fxPreset set uses that preset id;
// otherwise the preset whose id matches the spell's effectKind is used.
// assets/fx/ability_fx.def ships one preset per effect kind, so every
// kit gets visuals with zero per-character authoring; character authors
// can still override with an explicit `fx = <id>` line in [q]/[f]/[r].

#include "characters/CharacterDef.h"

#include <string>
#include <vector>

namespace cultulhu {

enum class FxEmitter {
    Point,  // single spawn point (trails, healing motes)
    Cone,   // directional cone (projectiles, downpour debuffs)
    Ring,   // expanding flat ring (summons, buffs)
    Sphere, // volumetric burst (AoE impacts, shields, auras)
    Beam,   // line between caster and target (stuns, pulls)
    Wall,   // vertical plane (reserved; e.g. shockwave curtains)
};

inline const char* fxEmitterName(FxEmitter e) {
    switch (e) {
        case FxEmitter::Point:  return "point";
        case FxEmitter::Cone:   return "cone";
        case FxEmitter::Ring:   return "ring";
        case FxEmitter::Sphere: return "sphere";
        case FxEmitter::Beam:   return "beam";
        case FxEmitter::Wall:   return "wall";
    }
    return "unknown";
}

// One named particle recipe. Distances are in meters; the roster plays
// at kaiju scale (20-30 m entities), so sizes read large on purpose.
struct FxPreset {
    std::string id;          // e.g. "aoe_damage"; matches effect kinds
    std::string displayName; // e.g. "Eldritch Impact"
    FxEmitter emitter = FxEmitter::Point;
    int count = 32;          // particles per burst / alive in a loop
    float lifetimeMin = 0.3f, lifetimeMax = 0.8f; // seconds per particle
    float speedMin = 1.0f, speedMax = 4.0f;       // meters per second
    float spreadDeg = 90.0f; // emission cone, 0 = straight, 180 = all
    float sizeMin = 0.4f, sizeMax = 1.0f;         // particle size, meters
    std::string color0 = "000000"; // hex RRGGBB, start color
    std::string color1 = "ffffff"; // hex RRGGBB, end color
    bool additive = false;   // additive blend (magic glow; fill-rate cost)
    float gravity = 0.0f;    // m/s^2; negative = rises
    // Small shared texture vocabulary: none|smoke|spark|rune|mist|ichor.
    std::string texture = "none";
    float durationSec = 1.0f; // how long the effect plays (one-shot)
    bool loop = false;        // keep emitting while the source is active
};

struct FxLibrary {
    std::vector<FxPreset> presets;
    const FxPreset* find(const std::string& id) const;
};

struct FxLoadResult {
    FxLibrary library;
    bool ok = false;
    std::string error; // set when !ok
};

// Load an ability_fx.def file. Never throws; reports the first problem.
FxLoadResult loadFxLibrary(const std::string& path);

// Parse ability_fx.def text directly (for tests / embedded data).
FxLoadResult parseFxLibraryText(const std::string& text);

// Resolve the preset id a spell should use: its explicit fxPreset, or
// its effectKind as the default (the catalog ships one preset per kind).
// Returns nullptr when no preset matches.
const FxPreset* fxForSpell(const SpellDef& spell, const FxLibrary& lib);

} // namespace cultulhu
