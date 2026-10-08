#pragma once

// Wave 7: world zones. A Zone is a named rectangular region of the surface
// map carrying spawn points, ambient parameters, and relic spawn spots.
// Zones are data-driven: they build from plain ZoneDef structs (no file
// parsing in v1; a loader can fill ZoneDef later without touching this).

#include "core/RNG.h"
#include "core/Vec3.h"
#include "entities/Entity.h"

#include <string>
#include <vector>

namespace cultulhu {

struct SpawnPoint {
    Vec3 pos;
    EntityType entityType = EntityType::Civilian;
    int count = 1;
};

struct AmbientParams {
    float ambientFear = 0.0f;       // background fear 0..100 in this zone
    float relicSpawnChance = 0.0f;  // per-spot relic roll 0..1
};

struct ZoneDef {
    std::string name;
    Vec3 min;                       // bounds corner (inclusive)
    Vec3 max;                       // bounds corner (inclusive)
    std::vector<SpawnPoint> spawns;
    AmbientParams ambient;
    std::vector<Vec3> relicSpots;    // candidate relic spawn locations
};

class Zone {
public:
    explicit Zone(ZoneDef def) : def_(std::move(def)) {}

    const std::string& name() const { return def_.name; }
    const Vec3& min() const { return def_.min; }
    const Vec3& max() const { return def_.max; }
    const std::vector<SpawnPoint>& spawnPoints() const { return def_.spawns; }
    const AmbientParams& ambient() const { return def_.ambient; }
    const std::vector<Vec3>& relicSpots() const { return def_.relicSpots; }

    // Wave 9c: raise (or lower) the background fear in this zone, 0..100.
    // Graffiti of the Old One feeds this; fear decays elsewhere.
    void addAmbientFear(float amount) {
        def_.ambient.ambientFear += amount;
        if (def_.ambient.ambientFear < 0.0f) def_.ambient.ambientFear = 0.0f;
        if (def_.ambient.ambientFear > 100.0f) def_.ambient.ambientFear = 100.0f;
    }

    bool contains(Vec3 p) const {
        return p.x >= def_.min.x && p.x <= def_.max.x &&
               p.y >= def_.min.y && p.y <= def_.max.y &&
               p.z >= def_.min.z && p.z <= def_.max.z;
    }

    Vec3 center() const {
        return Vec3((def_.min.x + def_.max.x) * 0.5f,
                    (def_.min.y + def_.max.y) * 0.5f,
                    (def_.min.z + def_.max.z) * 0.5f);
    }

    // Uniform random point inside the bounds.
    Vec3 randomPoint(RNG& rng) const;

    // Pick a random relic spot, or (0,0,0) when the zone has none.
    Vec3 randomRelicSpot(RNG& rng) const;

    // Wave 9b: persistent BlightLand state. Once a BlightLand directive
    // completes on this zone it stays Blighted for the rest of the game
    // (the city layer reads this to keep civilian output suppressed).
    bool blighted() const { return blighted_; }
    void setBlighted(bool b = true) { blighted_ = b; }

private:
    ZoneDef def_;
    bool blighted_ = false;
};

} // namespace cultulhu
