#pragma once

// Wave 7: the surface world map. A named collection of Zones plus a
// zoneAt(pos) lookup. Zones may overlap; zoneAt returns the first zone
// (in insertion order) containing the point, so list more specific zones
// before general ones.

#include "world/Zone.h"

#include <string>
#include <vector>

namespace cultulhu {

class WorldMap {
public:
    explicit WorldMap(std::string name = "surface") : name_(std::move(name)) {}

    const std::string& name() const { return name_; }

    void addZone(ZoneDef def) { zones_.emplace_back(std::move(def)); }
    size_t zoneCount() const { return zones_.size(); }
    const Zone& zone(size_t i) const { return zones_.at(i); }

    // First zone containing p, or nullptr when p is outside all zones.
    // NOTE: the pointer dangles after addZone() (vector reallocation);
    // re-fetch it after adding zones.
    const Zone* zoneAt(Vec3 p) const {
        for (const auto& z : zones_)
            if (z.contains(p)) return &z;
        return nullptr;
    }

private:
    std::string name_;
    std::vector<Zone> zones_;
};

} // namespace cultulhu
