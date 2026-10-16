#include "world/Zone.h"

namespace cultulhu {

Vec3 Zone::randomPoint(RNG& rng) const {
    return Vec3(rng.uniform(def_.min.x, def_.max.x),
                rng.uniform(def_.min.y, def_.max.y),
                rng.uniform(def_.min.z, def_.max.z));
}

Vec3 Zone::randomRelicSpot(RNG& rng) const {
    if (def_.relicSpots.empty()) return Vec3();
    size_t i = static_cast<size_t>(
        rng.intRange(0, static_cast<int>(def_.relicSpots.size()) - 1));
    return def_.relicSpots[i];
}

} // namespace cultulhu
