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
};

struct MapData {
    std::string name;
    std::vector<ZoneDef> zones;
    std::vector<PlacedProp> placements;

    // Feed the zone defs into an existing WorldMap (insertion order =
    // file order, so list specific zones before general ones).
    void applyZones(WorldMap& map) const {
        for (const auto& z : zones) map.addZone(z);
    }
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
