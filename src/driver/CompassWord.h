#pragma once
// CULT-ULHU driver helper: the "east-south" direction dialect.
//
// Convenience pass (2026-10-15): `look` used to tell you the nearest target
// was "45m away" with no direction — half a sentence, unactionable without
// wandering. The game already has a direction dialect for this (x+ = east,
// z+ = south): the arrival survey's "nearest wonder" and the `interact`
// civilian rumor both spell it inline. This helper is now the single source
// of that dialect — new sites read the same way, and the degenerate
// due-axis case collapses to one word ("east") instead of a nonsense
// compound like "east-north".
//
// Header-inline and dependency-free (only Vec3) so the micro regression
// suite can pin it — main.cpp isn't linkable from tests, same pattern as
// KitStatus.h / SelectNudge.h.
#include "core/Vec3.h"

#include <string>

namespace cultulhu {
namespace driver {

// Direction of `to` as seen from `from`, in the game's compass words.
// Diagonal pairs keep the historical hyphenated form ("east-south"); a
// target due along one axis collapses to the single word.
inline std::string compassWord(const Vec3& from, const Vec3& to) {
    const float dx = to.x - from.x;
    const float dz = to.z - from.z;
    const char* ew = (dx > 0.0f) ? "east" : (dx < 0.0f ? "west" : nullptr);
    const char* ns = (dz > 0.0f) ? "south" : (dz < 0.0f ? "north" : nullptr);
    if (ew && ns) return std::string(ew) + "-" + ns;
    if (ew) return ew;
    if (ns) return ns;
    return "here";
}

}  // namespace driver
}  // namespace cultulhu
