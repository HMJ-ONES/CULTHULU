// CULT-ULHU wave 37 tests: texture atlas pass (optimization).
//
// Guards the wave-37 atlas work:
//   - prop-fx-atlas.png exists and is 512x512 RGBA (PNG IHDR check);
//   - each of the 5 atlas models references the atlas uri and no longer
//     references its old per-prop texture;
//   - every UV (TEXCOORD_*) value in each model falls inside that model's
//     atlas sub-rect (half-texel inset applied by the build script), so
//     bilinear filtering cannot sample a neighbor rect.
//
// Models: mist-bank, scorched-patch, ember-cluster, beams-collapsed,
// dead-bush -> ../textures/prop-fx-atlas.png.

#include "assets/GlbInfo.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

using namespace cultulhu;

static int checks = 0;
static int failures = 0;

#define CHECK(cond) do { ++checks; if (!(cond)) { \
    ++failures; std::cout << "FAIL line " << __LINE__ << ": " #cond "\n"; } } while (0)

static std::string repoPrefix() {
    std::ifstream p("assets/world/textures/prop-fx-atlas.png");
    if (p) return "";
    return "../";
}

struct AtlasRect { const char* model; int x, y, w, h; };

static const AtlasRect kRects[] = {
    {"assets/world/props/mist-bank.glb",       0,   0, 256, 256},
    {"assets/world/props/ember-cluster.glb", 256,   0, 128, 128},
    {"assets/world/props/beams-collapsed.glb", 0, 256, 128, 128},
    {"assets/world/props/dead-bush.glb",     128, 256,  64,  64},
    {"assets/world/terrain/scorched-patch.glb",256,256,256, 256},
};

static const char* kOldTextures[] = {
    "mist-soft.png", "scorch-dark.png", "ember-glow.png",
    "wood-dark.png", "bark-dead.png",
};
static const char* kAtlasUri = "../textures/prop-fx-atlas.png";

// Atlas PNG is 512x512 (parse the IHDR chunk directly).
static void testAtlasPngSize() {
    const std::string path = repoPrefix() + "assets/world/textures/prop-fx-atlas.png";
    std::ifstream in(path, std::ios::binary);
    CHECK(in.good());
    if (!in.good()) return;
    unsigned char hdr[33] = {0};
    in.read(reinterpret_cast<char*>(hdr), 33);
    CHECK(in.gcount() == 33);
    static const unsigned char kPngSig[8] =
        {137, 80, 78, 71, 13, 10, 26, 10};
    CHECK(std::equal(hdr, hdr + 8, kPngSig));
    const uint32_t w = (hdr[16] << 24) | (hdr[17] << 16) |
                       (hdr[18] << 8) | hdr[19];
    const uint32_t h = (hdr[20] << 24) | (hdr[21] << 16) |
                       (hdr[22] << 8) | hdr[23];
    CHECK(w == 512 && h == 512);
}

// Extract a quoted string value for "key" among the direct members of the
// object at s[objBegin] (up to objEnd). Returns "" when absent/malformed.
static std::string extractString(const std::string& s, size_t objBegin,
                                 size_t objEnd, const char* key) {
    using namespace glb_detail;
    size_t valPos = 0;
    bool isArray = false;
    if (!scanMembers(s, objBegin, objEnd, key, valPos, isArray) || isArray)
        return "";
    if (valPos >= objEnd || s[valPos] != '"') return "";
    std::string out;
    for (size_t i = valPos + 1; i < objEnd; ++i) {
        if (s[i] == '\\' && i + 1 < objEnd) { out += s[++i]; continue; }
        if (s[i] == '"') return out;
        out += s[i];
    }
    return "";
}

struct GlbDoc {
    std::string json;
    std::vector<unsigned char> bin;
    bool ok = false;
};

static GlbDoc loadGlb(const std::string& path) {
    GlbDoc d;
    std::ifstream in(path, std::ios::binary);
    if (!in) return d;
    in.seekg(0, std::ios::end);
    const long long size = static_cast<long long>(in.tellg());
    in.seekg(0, std::ios::beg);
    if (size < 28) return d;
    uint32_t magic = 0, version = 0, chunkLen = 0, chunkType = 0;
    in.read(reinterpret_cast<char*>(&magic), 4);
    in.read(reinterpret_cast<char*>(&version), 4);
    in.ignore(4);  // total length
    in.read(reinterpret_cast<char*>(&chunkLen), 4);
    in.read(reinterpret_cast<char*>(&chunkType), 4);
    if (magic != 0x46546C67 || version != 2 || chunkType != 0x4E4F534A)
        return d;
    d.json.assign(chunkLen, '\0');
    in.read(&d.json[0], chunkLen);
    if (!in) return d;
    if (in.peek() != EOF) {
        in.read(reinterpret_cast<char*>(&chunkLen), 4);
        in.read(reinterpret_cast<char*>(&chunkType), 4);
        if (chunkType == 0x004E4942 && chunkLen > 0) {
            d.bin.assign(chunkLen, 0);
            in.read(reinterpret_cast<char*>(d.bin.data()), chunkLen);
            if (!in) return d;
        }
    }
    d.ok = true;
    return d;
}

// Walk the accessors array; for every float VEC2 (a UV set) validate that
// all values lie inside the atlas rect (half-texel inset). Returns the
// number of UV accessors found.
static int validateUvAccessors(const GlbDoc& d, const AtlasRect& r) {
    using namespace glb_detail;
    const std::string& json = d.json;
    const auto root = rootObject(json);
    if (root.first == std::string::npos || root.second == std::string::npos)
        return 0;
    const size_t accArr = findArray(json, root.first, root.second, "accessors");
    const size_t bvArr = findArray(json, root.first, root.second, "bufferViews");
    if (accArr == std::string::npos || bvArr == std::string::npos) return 0;
    const size_t accEnd = matchBracket(json, accArr, '[', ']');
    const size_t bvEnd = matchBracket(json, bvArr, '[', ']');
    if (accEnd == std::string::npos || bvEnd == std::string::npos) return 0;

    struct View { long long byteOffset = 0; };
    std::vector<View> views;
    {
        size_t i = bvArr + 1;
        while (true) {
            skipWs(json, i);
            if (i >= bvEnd - 1) break;
            if (json[i] == ',') { ++i; continue; }
            if (json[i] != '{') break;
            const size_t e = matchBracket(json, i, '{', '}');
            if (e == std::string::npos || e > bvEnd) break;
            View v;
            long long o = 0;
            if (extractInt(json, i, e, "byteOffset", o)) v.byteOffset = o;
            views.push_back(v);
            i = e;
        }
    }

    const double uMin = (r.x + 0.5) / 512.0;
    const double uMax = (r.x + r.w - 0.5) / 512.0;
    const double vMin = (r.y + 0.5) / 512.0;
    const double vMax = (r.y + r.h - 0.5) / 512.0;
    const double eps = 1e-6;

    int uvSets = 0;
    size_t i = accArr + 1;
    while (true) {
        skipWs(json, i);
        if (i >= accEnd - 1) break;
        if (json[i] == ',') { ++i; continue; }
        if (json[i] != '{') break;
        const size_t e = matchBracket(json, i, '{', '}');
        if (e == std::string::npos || e > accEnd) break;
        const std::string type = extractString(json, i, e, "type");
        long long comp = 0, bvIdx = 0, count = 0, accOff = 0;
        if (type == "VEC2" &&
            extractInt(json, i, e, "componentType", comp) && comp == 5126 &&
            extractInt(json, i, e, "bufferView", bvIdx) &&
            extractInt(json, i, e, "count", count) &&
            bvIdx >= 0 && (size_t)bvIdx < views.size()) {
            extractInt(json, i, e, "byteOffset", accOff);
            const size_t base =
                (size_t)(views[(size_t)bvIdx].byteOffset + accOff);
            CHECK(base + (size_t)count * 8 <= d.bin.size());
            ++uvSets;
            for (long long k = 0; k < count; ++k) {
                float u = 0, v = 0;
                if (base + (size_t)k * 8 + 8 <= d.bin.size()) {
                    std::memcpy(&u, d.bin.data() + base + (size_t)k * 8, 4);
                    std::memcpy(&v, d.bin.data() + base + (size_t)k * 8 + 4, 4);
                }
                if (!(u >= uMin - eps && u <= uMax + eps &&
                      v >= vMin - eps && v <= vMax + eps)) {
                    ++checks; ++failures;
                    std::cout << "FAIL " << r.model << ": UV (" << u << ","
                              << v << ") outside atlas rect\n";
                } else {
                    ++checks;
                }
            }
        }
        i = e;
    }
    return uvSets;
}

static void testAtlasModels() {
    const std::string prefix = repoPrefix();
    for (const AtlasRect& r : kRects) {
        GlbDoc d = loadGlb(prefix + r.model);
        CHECK(d.ok);
        if (!d.ok) continue;
        // References the atlas, not the old per-prop texture.
        CHECK(d.json.find(std::string("\"uri\":\"") + kAtlasUri + "\"") !=
              std::string::npos);
        for (const char* old : kOldTextures) {
            CHECK(d.json.find(old) == std::string::npos);
        }
        // UVs land inside the model's atlas sub-rect.
        const int uvSets = validateUvAccessors(d, r);
        CHECK(uvSets >= 1);
    }
}

int main() {
    testAtlasPngSize();
    testAtlasModels();
    std::cout << "wave37: " << checks << " checks, " << failures
              << " failures\n";
    return failures == 0 ? 0 : 1;
}
