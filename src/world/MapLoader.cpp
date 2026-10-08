// Wave 11 art pass (PART B): .map text format parser. See MapLoader.h and
// the format documentation at the top of assets/maps/ruined_city.map.

#include "world/MapLoader.h"

#include <cctype>
#include <fstream>
#include <sstream>

namespace cultulhu {
namespace {

std::vector<std::string> tokenize(const std::string& line) {
    std::vector<std::string> out;
    std::istringstream in(line);
    std::string tok;
    while (in >> tok) out.push_back(tok);
    return out;
}

float parseFloat(const std::string& file, size_t lineNo,
                 const std::string& tok, const std::string& what) {
    try {
        size_t used = 0;
        float v = std::stof(tok, &used);
        if (used != tok.size())
            throw std::invalid_argument("trailing characters");
        return v;
    } catch (const std::exception&) {
        throw MapParseError(file, lineNo,
                            "bad " + what + ": '" + tok + "'");
    }
}

} // namespace

MapData MapLoader::load(const std::string& path) {
    std::ifstream in(path);
    if (!in)
        throw MapParseError(path, 0, "cannot open map file");

    MapData data;
    bool haveMap = false;
    size_t lineNo = 0;
    std::string line;
    while (std::getline(in, line)) {
        ++lineNo;
        // Strip comments.
        const size_t hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        const auto toks = tokenize(line);
        if (toks.empty()) continue;

        const std::string& kw = toks[0];
        if (kw == "map") {
            if (toks.size() != 2)
                throw MapParseError(path, lineNo,
                                    "'map' needs exactly 1 argument");
            if (haveMap)
                throw MapParseError(path, lineNo,
                                    "duplicate 'map' declaration");
            data.name = toks[1];
            haveMap = true;
        } else if (kw == "zone") {
            if (toks.size() != 9)
                throw MapParseError(
                    path, lineNo,
                    "'zone' needs 8 arguments: name minx miny minz "
                    "maxx maxy maxz ambientFear");
            if (!haveMap)
                throw MapParseError(path, lineNo,
                                    "'zone' before 'map' declaration");
            ZoneDef z;
            z.name = toks[1];
            z.min = Vec3(parseFloat(path, lineNo, toks[2], "minx"),
                         parseFloat(path, lineNo, toks[3], "miny"),
                         parseFloat(path, lineNo, toks[4], "minz"));
            z.max = Vec3(parseFloat(path, lineNo, toks[5], "maxx"),
                         parseFloat(path, lineNo, toks[6], "maxy"),
                         parseFloat(path, lineNo, toks[7], "maxz"));
            z.ambient.ambientFear =
                parseFloat(path, lineNo, toks[8], "ambientFear");
            if (z.ambient.ambientFear < 0.0f ||
                z.ambient.ambientFear > 100.0f)
                throw MapParseError(path, lineNo,
                                    "ambientFear must be 0..100");
            if (z.min.x > z.max.x || z.min.y > z.max.y || z.min.z > z.max.z)
                throw MapParseError(path, lineNo,
                                    "zone min corner exceeds max corner");
            data.zones.push_back(std::move(z));
        } else if (kw == "place") {
            if (toks.size() != 8)
                throw MapParseError(
                    path, lineNo,
                    "'place' needs 7 arguments: model x y z rotY scale zone");
            if (!haveMap)
                throw MapParseError(path, lineNo,
                                    "'place' before 'map' declaration");
            PlacedProp p;
            p.modelPath = toks[1];
            p.pos = Vec3(parseFloat(path, lineNo, toks[2], "x"),
                         parseFloat(path, lineNo, toks[3], "y"),
                         parseFloat(path, lineNo, toks[4], "z"));
            p.rotY = parseFloat(path, lineNo, toks[5], "rotY");
            p.scale = parseFloat(path, lineNo, toks[6], "scale");
            if (p.scale <= 0.0f)
                throw MapParseError(path, lineNo, "scale must be > 0");
            p.zone = toks[7];
            bool known = false;
            for (const auto& z : data.zones)
                if (z.name == p.zone) { known = true; break; }
            if (!known)
                throw MapParseError(path, lineNo,
                                    "unknown zone '" + p.zone + "'");
            data.placements.push_back(std::move(p));
        } else {
            throw MapParseError(path, lineNo,
                                "unknown directive '" + kw +
                                    "' (expected map|zone|place)");
        }
    }

    if (!haveMap)
        throw MapParseError(path, 0, "missing 'map' declaration");
    return data;
}

} // namespace cultulhu
