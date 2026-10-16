#pragma once

// Wave 11 art pass (PART B): minimal GLB 2.0 header reader. Counts
// triangles across all mesh primitives so the art budget (every model
// < 5000 triangles) can be enforced without an engine or a full glTF
// parser. Reads only the JSON chunk; the BIN chunk is never touched.
//
// Supported: indexed and non-indexed primitives, modes TRIANGLES (4),
// TRIANGLE_STRIP (5), TRIANGLE_FAN (6). Draco-compressed or extension
// primitives report ok=false (counts are not readable from the header).
// Header-only so both the driver (`mapinfo`) and tests can use it.

#include <cstdint>
#include <fstream>
#include <string>
#include <vector>

namespace cultulhu {

struct GlbStats {
    bool ok = false;               // false when the file is not a readable GLB
    uint64_t triangles = 0;        // summed over all primitives
    uint64_t primitives = 0;       // primitives examined
    std::string error;             // set when !ok
};

namespace glb_detail {

// --- tiny JSON scanners (brace-aware, string-aware) ---

inline void skipWs(const std::string& s, size_t& i) {
    while (i < s.size() &&
           (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r'))
        ++i;
}

// Advance i past a JSON string starting at s[i] == '"'. Returns false on
// unterminated string.
inline bool skipString(const std::string& s, size_t& i) {
    ++i;  // opening quote
    while (i < s.size()) {
        if (s[i] == '\\') { i += 2; continue; }
        if (s[i] == '"') { ++i; return true; }
        ++i;
    }
    return false;
}

// Find the index just past the matching close for the bracket at openPos.
// open/close are e.g. '{'/'}'. Strings are skipped. npos on mismatch.
inline size_t matchBracket(const std::string& s, size_t openPos,
                           char open, char close) {
    if (openPos >= s.size() || s[openPos] != open) return std::string::npos;
    size_t depth = 0;
    for (size_t i = openPos; i < s.size(); ++i) {
        if (s[i] == '"') { if (!skipString(s, i)) return std::string::npos; --i; continue; }
        if (s[i] == open) ++depth;
        else if (s[i] == close && --depth == 0) return i + 1;
    }
    return std::string::npos;
}

// Scan the direct members of the object at s[objBegin] == '{' (up to
// objEnd, just past its '}'). Shared walker for extractInt/findArray.
inline bool scanMembers(const std::string& s, size_t objBegin, size_t objEnd,
                        const char* key, size_t& valPos, bool& isArray) {
    const std::string pat = std::string("\"") + key + "\"";
    size_t i = objBegin + 1;
    while (i < objEnd) {
        skipWs(s, i);
        if (i >= objEnd || s[i] == '}') break;
        if (s[i] == ',') { ++i; continue; }
        if (s[i] != '"') return false;  // malformed member
        const size_t ks = i;
        size_t j = i;
        if (!skipString(s, j)) return false;
        const bool isKey = (s.compare(ks, pat.size(), pat) == 0);
        size_t k = j;
        skipWs(s, k);
        if (k >= objEnd || s[k] != ':') return false;
        ++k; skipWs(s, k);
        if (isKey) {
            valPos = k;
            isArray = (k < objEnd && s[k] == '[');
            return true;
        }
        // Skip this member's value.
        if (k < objEnd && s[k] == '"') {
            size_t t = k;
            if (!skipString(s, t)) return false;
            i = t;
        } else if (k < objEnd && (s[k] == '{' || s[k] == '[')) {
            const size_t e =
                matchBracket(s, k, s[k], s[k] == '{' ? '}' : ']');
            if (e == std::string::npos || e > objEnd) return false;
            i = e;
        } else {
            i = k;
            while (i < objEnd && s[i] != ',' && s[i] != '}') ++i;
        }
    }
    return false;
}

// Extract a signed integer value for "key" among the direct members of
// the object at s[objBegin] == '{'. Nested objects/arrays are skipped,
// so e.g. a nested "count" is never picked up. False when absent.
inline bool extractInt(const std::string& s, size_t objBegin, size_t objEnd,
                       const char* key, long long& out) {
    size_t valPos = 0;
    bool isArray = false;
    if (!scanMembers(s, objBegin, objEnd, key, valPos, isArray) || isArray)
        return false;
    size_t k = valPos;
    const size_t numStart = k;
    if (k < objEnd && (s[k] == '-' || s[k] == '+')) ++k;
    while (k < objEnd && s[k] >= '0' && s[k] <= '9') ++k;
    if (k == numStart) return false;
    try {
        out = std::stoll(s.substr(numStart, k - numStart));
    } catch (...) { return false; }
    return true;
}

// Locate the array value of "key" among the direct members of the object
// at s[objBegin] == '{'; returns the position of '[' or npos.
inline size_t findArray(const std::string& s, size_t objBegin, size_t objEnd,
                        const char* key) {
    size_t valPos = 0;
    bool isArray = false;
    if (!scanMembers(s, objBegin, objEnd, key, valPos, isArray) || !isArray)
        return std::string::npos;
    return valPos;
}

// Bounds of the JSON root object; {npos, npos} when malformed.
inline std::pair<size_t, size_t> rootObject(const std::string& s) {
    const size_t b = s.find('{');
    if (b == std::string::npos) return {b, b};
    const size_t e = matchBracket(s, b, '{', '}');
    return {b, e};
}

} // namespace glb_detail

// Count triangles in a .glb file. Never throws.
inline GlbStats readGlbStats(const std::string& path) {
    using namespace glb_detail;
    GlbStats st;

    std::ifstream in(path, std::ios::binary);
    if (!in) { st.error = "cannot open file"; return st; }
    in.seekg(0, std::ios::end);
    const long long size = static_cast<long long>(in.tellg());
    in.seekg(0, std::ios::beg);
    if (size < 20) { st.error = "file too small for GLB header"; return st; }

    uint32_t magic = 0, version = 0, length = 0;
    in.read(reinterpret_cast<char*>(&magic), 4);
    in.read(reinterpret_cast<char*>(&version), 4);
    in.read(reinterpret_cast<char*>(&length), 4);
    if (magic != 0x46546C67) { st.error = "bad magic (not a GLB)"; return st; }
    if (version != 2) { st.error = "unsupported glTF version"; return st; }

    uint32_t chunkLen = 0, chunkType = 0;
    in.read(reinterpret_cast<char*>(&chunkLen), 4);
    in.read(reinterpret_cast<char*>(&chunkType), 4);
    if (chunkType != 0x4E4F534A) { st.error = "first chunk is not JSON"; return st; }
    std::string json(chunkLen, '\0');
    in.read(&json[0], chunkLen);
    if (!in) { st.error = "truncated JSON chunk"; return st; }

    // 1) accessor counts, in array order (index == position).
    std::vector<uint64_t> accessorCounts;
    {
        const auto root = rootObject(json);
        if (root.first == std::string::npos ||
            root.second == std::string::npos) {
            st.error = "malformed JSON root";
            return st;
        }
        const size_t arr = findArray(json, root.first, root.second, "accessors");
        if (arr == std::string::npos) { st.error = "no accessors array"; return st; }
        const size_t arrEnd = matchBracket(json, arr, '[', ']');
        if (arrEnd == std::string::npos) { st.error = "unterminated accessors"; return st; }
        size_t i = arr + 1;
        while (true) {
            skipWs(json, i);
            if (i >= arrEnd - 1) break;
            if (json[i] == ',') { ++i; continue; }
            if (json[i] != '{') { st.error = "bad accessor entry"; return st; }
            const size_t objEnd = matchBracket(json, i, '{', '}');
            if (objEnd == std::string::npos || objEnd > arrEnd) {
                st.error = "unterminated accessor"; return st;
            }
            long long count = 0;
            if (!extractInt(json, i, objEnd, "count", count) || count < 0) {
                st.error = "accessor without count"; return st;
            }
            accessorCounts.push_back(static_cast<uint64_t>(count));
            i = objEnd;
        }
    }

    // 2) meshes -> primitives.
    const auto root = rootObject(json);
    const size_t meshesArr = findArray(json, root.first, root.second, "meshes");
    if (meshesArr == std::string::npos) { st.error = "no meshes array"; return st; }
    const size_t meshesEnd = matchBracket(json, meshesArr, '[', ']');
    if (meshesEnd == std::string::npos) { st.error = "unterminated meshes"; return st; }

    size_t i = meshesArr + 1;
    while (true) {
        skipWs(json, i);
        if (i >= meshesEnd - 1) break;
        if (json[i] == ',') { ++i; continue; }
        if (json[i] != '{') { st.error = "bad mesh entry"; return st; }
        const size_t meshEnd = matchBracket(json, i, '{', '}');
        if (meshEnd == std::string::npos || meshEnd > meshesEnd) {
            st.error = "unterminated mesh"; return st;
        }
        const size_t primArr = findArray(json, i, meshEnd, "primitives");
        if (primArr != std::string::npos) {
            const size_t primEnd = matchBracket(json, primArr, '[', ']');
            if (primEnd == std::string::npos || primEnd > meshEnd) {
                st.error = "unterminated primitives"; return st;
            }
            size_t p = primArr + 1;
            while (true) {
                skipWs(json, p);
                if (p >= primEnd - 1) break;
                if (json[p] == ',') { ++p; continue; }
                if (json[p] != '{') { st.error = "bad primitive entry"; return st; }
                const size_t pEnd = matchBracket(json, p, '{', '}');
                if (pEnd == std::string::npos || pEnd > primEnd) {
                    st.error = "unterminated primitive"; return st;
                }
                long long mode = 4;  // default TRIANGLES
                extractInt(json, p, pEnd, "mode", mode);
                long long idx = -1;
                uint64_t n = 0;
                bool haveCount = false;
                if (extractInt(json, p, pEnd, "indices", idx)) {
                    if (idx < 0 || static_cast<size_t>(idx) >= accessorCounts.size()) {
                        st.error = "primitive indices out of range"; return st;
                    }
                    n = accessorCounts[static_cast<size_t>(idx)];
                    haveCount = true;
                } else {
                    // Non-indexed: count POSITION vertices. "attributes"
                    // is an object, not an array, so use scanMembers
                    // directly instead of findArray.
                    size_t attrPos = 0;
                    bool attrIsArray = false;
                    if (scanMembers(json, p, pEnd, "attributes", attrPos,
                                    attrIsArray) &&
                        !attrIsArray) {
                        const size_t attrEnd =
                            matchBracket(json, attrPos, '{', '}');
                        long long posIdx = -1;
                        if (attrEnd != std::string::npos &&
                            extractInt(json, attrPos, attrEnd, "POSITION",
                                       posIdx) &&
                            posIdx >= 0 &&
                            static_cast<size_t>(posIdx) <
                                accessorCounts.size()) {
                            n = accessorCounts[static_cast<size_t>(posIdx)];
                            haveCount = true;
                        }
                    }
                }
                if (!haveCount) { st.error = "primitive has no countable vertices"; return st; }
                if (mode == 4) st.triangles += n / 3;
                else if (mode == 5 || mode == 6) st.triangles += (n >= 3 ? n - 2 : 0);
                // modes 0..3 (points/lines) contribute no triangles
                ++st.primitives;
                p = pEnd;
            }
        }
        i = meshEnd;
    }

    st.ok = true;
    return st;
}

} // namespace cultulhu
