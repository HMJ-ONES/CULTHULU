#pragma once

// CULT-ULHU wave 40: directional kit combat for 5v5 matches.
//
// Every match fighter now wields a full character kit (Q/F/R + RMB), the
// same kits playable in free-roam. All attacks are directional — melee is
// a frontal cone, and each ability resolves through an explicit shape
// (narrow cone, wide cone, radial, or self). Combat plays the same in
// Domination, DOTGOO, and free-roam.

#include "core/Vec3.h"

#include <string>
#include <vector>

namespace cultulhu {

class CharacterRegistry;

// Yaw (radians, atan2 convention) pointing from `from` toward `to`.
float yawTo(const Vec3& from, const Vec3& to);

// True when targetPos lies within `range` meters of the caster AND inside
// the frontal cone centered on `facingYaw` with the given half-angle.
// This is what makes attacks directional: standing behind someone is safe.
bool inAttackCone(const Vec3& casterPos, float facingYaw, const Vec3& targetPos,
                  float range, float halfAngleDeg);

// Load every character kit under <base>/characters into the registry.
// Tries <base> itself, then ../assets, then ./assets, so both the driver
// (repo root) and the test binaries (build/) find the data. Returns the
// number of kits loaded.
int loadMatchKits(CharacterRegistry& reg, const std::string& base);

} // namespace cultulhu
