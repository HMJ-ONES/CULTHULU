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
        // Strip comments: '#' starts a comment only at line start or after
        // whitespace (so #rrggbb hex colors in values survive).
        for (size_t i = 0; i < line.size(); ++i) {
            if (line[i] == '#' &&
                (i == 0 || line[i - 1] == ' ' || line[i - 1] == '\t')) {
                line.erase(i);
                break;
            }
        }
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
            if (toks.size() < 8)
                throw MapParseError(
                    path, lineNo,
                    "'place' needs 7 arguments plus optional cull hints: "
                    "model x y z rotY scale zone [cull=<m>] [nevercull]");
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
            // Wave 38: optional per-placement culling hints.
            for (size_t i = 8; i < toks.size(); ++i) {
                const std::string& arg = toks[i];
                if (arg == "nevercull") {
                    p.neverCull = true;
                } else {
                    const size_t eq = arg.find('=');
                    const std::string key =
                        eq == std::string::npos ? arg : arg.substr(0, eq);
                    const std::string val =
                        eq == std::string::npos ? "" : arg.substr(eq + 1);
                    if (key == "cull" && !val.empty()) {
                        p.cullDist =
                            parseFloat(path, lineNo, val, "cull distance");
                        if (p.cullDist <= 0.0f)
                            throw MapParseError(
                                path, lineNo,
                                "cull distance must be > 0");
                    } else {
                        throw MapParseError(
                            path, lineNo,
                            "unknown 'place' hint '" + arg +
                                "' (expected cull=<m>|nevercull)");
                    }
                }
            }
            if (p.neverCull && p.cullDist > 0.0f)
                throw MapParseError(path, lineNo,
                                    "'cull=<m>' and 'nevercull' conflict");
            bool known = false;
            for (const auto& z : data.zones)
                if (z.name == p.zone) { known = true; break; }
            if (!known)
                throw MapParseError(path, lineNo,
                                    "unknown zone '" + p.zone + "'");
            data.placements.push_back(std::move(p));
        } else if (kw == "lod") {
            // Wave 38: per-model LOD distances for the engine binding.
            //   lod <model> hide=<m> [low=<m>]
            // hide= is required and must be > 0; low= is optional and must
            // be < hide. One rule per model (duplicates are an error).
            if (toks.size() < 3)
                throw MapParseError(path, lineNo,
                                    "'lod' needs a model and hide=<m>");
            if (!haveMap)
                throw MapParseError(path, lineNo,
                                    "'lod' before 'map' declaration");
            LodRule r;
            r.modelPath = toks[1];
            for (size_t i = 2; i < toks.size(); ++i) {
                const std::string& arg = toks[i];
                const size_t eq = arg.find('=');
                if (eq == std::string::npos)
                    throw MapParseError(path, lineNo,
                                        "lod arg must be key=value, got '" +
                                            arg + "'");
                const std::string key = arg.substr(0, eq);
                const std::string val = arg.substr(eq + 1);
                if (key == "hide") {
                    r.hideAt = parseFloat(path, lineNo, val, "hide distance");
                } else if (key == "low") {
                    r.lowAt = parseFloat(path, lineNo, val, "low distance");
                } else {
                    throw MapParseError(path, lineNo,
                                        "unknown lod key '" + key +
                                            "' (hide|low)");
                }
            }
            if (r.hideAt <= 0.0f)
                throw MapParseError(path, lineNo,
                                    "lod hide distance must be > 0");
            if (r.lowAt < 0.0f)
                throw MapParseError(path, lineNo,
                                    "lod low distance must be >= 0");
            if (r.lowAt > 0.0f && r.lowAt >= r.hideAt)
                throw MapParseError(
                    path, lineNo,
                    "lod low distance must be < hide distance");
            for (const auto& existing : data.lodRules)
                if (existing.modelPath == r.modelPath)
                    throw MapParseError(path, lineNo,
                                        "duplicate lod rule for '" +
                                            r.modelPath + "'");
            data.lodRules.push_back(std::move(r));
        } else if (kw == "atmosphere") {
            // Wave 32: per-zone lighting/mood data for the UE5 binding.
            //   atmosphere <zone> fog=#rrggbb,density ambient=#rrggbb,intensity
            //                      sky=#rrggbb stars=0..1
            // All keys optional; unset keys keep the dark defaults.
            if (toks.size() < 2)
                throw MapParseError(path, lineNo,
                                    "'atmosphere' needs a zone name");
            ZoneDef* zd = nullptr;
            for (auto& z : data.zones)
                if (z.name == toks[1]) { zd = &z; break; }
            if (!zd)
                throw MapParseError(path, lineNo,
                                    "atmosphere for unknown zone '" +
                                        toks[1] + "'");
            auto parseHex = [&](const std::string& h, float& r, float& g,
                                float& b) {
                if (h.size() != 7 || h[0] != '#')
                    throw MapParseError(path, lineNo,
                                        "color must be #rrggbb, got '" + h +
                                            "'");
                const int v = std::stoi(h.substr(1), nullptr, 16);
                r = ((v >> 16) & 255) / 255.0f;
                g = ((v >> 8) & 255) / 255.0f;
                b = (v & 255) / 255.0f;
            };
            for (size_t i = 2; i < toks.size(); ++i) {
                const std::string& kv = toks[i];
                const size_t eq = kv.find('=');
                if (eq == std::string::npos)
                    throw MapParseError(path, lineNo,
                                        "atmosphere arg must be key=value, got '" +
                                            kv + "'");
                const std::string key = kv.substr(0, eq);
                const std::string val = kv.substr(eq + 1);
                const size_t comma = val.find(',');
                const std::string first =
                    comma == std::string::npos ? val : val.substr(0, comma);
                const std::string second =
                    comma == std::string::npos ? "" : val.substr(comma + 1);
                if (key == "fog") {
                    parseHex(first, zd->atmosphere.fogR, zd->atmosphere.fogG,
                             zd->atmosphere.fogB);
                    if (!second.empty())
                        zd->atmosphere.fogDensity = parseFloat(
                            path, lineNo, second, "fog density");
                } else if (key == "ambient") {
                    parseHex(first, zd->atmosphere.ambR, zd->atmosphere.ambG,
                             zd->atmosphere.ambB);
                    if (!second.empty())
                        zd->atmosphere.ambIntensity = parseFloat(
                            path, lineNo, second, "ambient intensity");
                } else if (key == "sky") {
                    parseHex(first, zd->atmosphere.skyR, zd->atmosphere.skyG,
                             zd->atmosphere.skyB);
                } else if (key == "stars") {
                    zd->atmosphere.stars =
                        parseFloat(path, lineNo, first, "stars");
                    if (zd->atmosphere.stars < 0.0f ||
                        zd->atmosphere.stars > 1.0f)
                        throw MapParseError(path, lineNo,
                                            "stars must be 0..1");
                } else {
                    throw MapParseError(path, lineNo,
                                        "unknown atmosphere key '" + key +
                                            "' (fog|ambient|sky|stars)");
                }
            }
        } else {
            throw MapParseError(path, lineNo,
                                "unknown directive '" + kw +
                                    "' (expected map|zone|atmosphere|place|lod)");
        }
    }

    if (!haveMap)
        throw MapParseError(path, 0, "missing 'map' declaration");
    return data;
}

float MapData::cullDistanceFor(const PlacedProp& p) const {
    if (p.neverCull) return 0.0f;
    if (p.cullDist > 0.0f) return p.cullDist;
    for (const auto& r : lodRules)
        if (r.modelPath == p.modelPath) return r.hideAt;
    return 0.0f;
}

} // namespace cultulhu
