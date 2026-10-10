#pragma once

// Wave 11 art pass (PART B): text map loader. Parses assets/maps/*.map
// (see ruined_city.map for the documented format) into plain structs.
// Zone defs feed the existing Zone/WorldMap classes unchanged; placements
// are data for the engine binding (Unreal/Unity) to instantiate. The core
// stays engine-agnostic: model paths are just strings.
//
// Errors fail loudly: MapParseError carries the file, line number and a
// message. There is no lenient mode.

#include "core/Vec3.h"
#include "world/WorldMap.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace cultulhu {

// One prop instance from a `place` line.
struct PlacedProp {
    std::string modelPath;  // repo-relative, e.g. assets/world/cult/altar.glb
    Vec3 pos;
    float rotY = 0.0f;      // degrees, Y-up yaw
    float scale = 1.0f;     // uniform
    std::string zone;       // zone name; must be declared in the same file

    // Wave 38: per-placement culling hints (data only — the engine binding
    // decides what "hide" means; the headless core never renders).
    float cullDist = -1.0f; // hide override in meters; <0 = use LOD rule
    bool neverCull = false; // pin visible at any distance (hero landmarks)
};

// Wave 38: one LOD rule from a `lod` line — per-model render-distance
// hints authored in the .map file. hideAt is the distance past which the
// engine may skip rendering the model entirely; lowAt (0 = none) is the
// distance past which the engine may swap to a low-detail stand-in.
// Both are meters from the camera. The core stores them; the Unreal/Unity
// binding consumes them (see unreal/Docs/SystemMapping.md §LOD).
struct LodRule {
    std::string modelPath;
    float hideAt = 0.0f;
    float lowAt = 0.0f;
};

struct MapData {
    std::string name;
    std::vector<ZoneDef> zones;
    std::vector<PlacedProp> placements;
    std::vector<LodRule> lodRules;  // wave 38: per-model LOD distances

    // Feed the zone defs into an existing WorldMap (insertion order =
    // file order, so list specific zones before general ones).
    void applyZones(WorldMap& map) const {
        for (const auto& z : zones) map.addZone(z);
    }

    // Wave 38: effective hide distance for a placement, in meters.
    // Resolution order: neverCull -> 0 (infinite); explicit cullDist ->
    // cullDist; the model's lod rule hideAt; no rule -> 0 (always visible).
    float cullDistanceFor(const PlacedProp& p) const;
};

class MapParseError : public std::runtime_error {
public:
    MapParseError(const std::string& file, size_t line,
                  const std::string& msg)
        : std::runtime_error(file + ":" + std::to_string(line) + ": " + msg),
          file_(file), line_(line) {}

    const std::string& file() const { return file_; }
    size_t line() const { return line_; }

private:
    std::string file_;
    size_t line_;
};

class MapLoader {
public:
    // Throws MapParseError on any problem (missing file, bad directive,
    // wrong arity, non-numeric value, unknown zone, duplicate map decl).
    static MapData load(const std::string& path);

private:
    MapLoader() = delete;
};

} // namespace cultulhu
