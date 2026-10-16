#include "assets/GlbAnimExtractor.h"

#include "animation/ClipSerializer.h"
#include "animation/ProceduralClips.h"
#include "entities/Entity.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <vector>

namespace cultulhu {
namespace {

// ---------- minimal JSON DOM (objects/arrays/strings/numbers only) ----------

struct JVal {
    enum class T { Null, Bool, Num, Str, Arr, Obj } type = T::Null;
    bool b = false;
    double num = 0.0;
    std::string str;
    std::vector<JVal> arr;
    std::vector<std::pair<std::string, JVal>> obj;

    const JVal* get(const std::string& key) const {
        if (type != T::Obj) return nullptr;
        for (const auto& kv : obj)
            if (kv.first == key) return &kv.second;
        return nullptr;
    }
    std::string s(const std::string& key,
                  const std::string& dflt = "") const {
        const JVal* v = get(key);
        return (v && v->type == T::Str) ? v->str : dflt;
    }
    double n(const std::string& key, double dflt = 0.0) const {
        const JVal* v = get(key);
        return (v && v->type == T::Num) ? v->num : dflt;
    }
    int i(const std::string& key, int dflt = 0) const {
        return static_cast<int>(n(key, static_cast<double>(dflt)));
    }
    const JVal* at(size_t k) const {
        if (type != T::Arr || k >= arr.size()) return nullptr;
        return &arr[k];
    }
    size_t size() const { return type == T::Arr ? arr.size() : 0; }
};

struct JParser {
    const char* p;
    const char* end;
    std::string err;

    explicit JParser(const std::string& s)
        : p(s.data()), end(s.data() + s.size()) {}

    void ws() {
        while (p < end && (*p == ' ' || *p == '\t' || *p == '\n' ||
                           *p == '\r'))
            ++p;
    }
    bool lit(const char* w) {
        size_t n = std::strlen(w);
        if (static_cast<size_t>(end - p) < n || std::strncmp(p, w, n))
            return false;
        p += n;
        return true;
    }
    bool parseStr(std::string& out) {
        if (p >= end || *p != '"') return false;
        ++p;
        out.clear();
        while (p < end) {
            char c = *p++;
            if (c == '"') return true;
            if (c == '\\' && p < end) {
                char e = *p++;
                switch (e) {
                    case 'n': out += '\n'; break;
                    case 't': out += '\t'; break;
                    case 'r': out += '\r'; break;
                    default: out += e; break;
                }
            } else {
                out += c;
            }
        }
        return false;
    }
    bool parseVal(JVal& v) {
        ws();
        if (p >= end) return false;
        if (*p == '{') {
            ++p;
            v.type = JVal::T::Obj;
            ws();
            if (p < end && *p == '}') { ++p; return true; }
            while (true) {
                ws();
                std::string key;
                JVal val;
                if (!parseStr(key) || (ws(), p >= end) || *p != ':' ||
                    (++p, !parseVal(val)))
                    return false;
                v.obj.emplace_back(std::move(key), std::move(val));
                ws();
                if (p >= end) return false;
                if (*p == ',') { ++p; continue; }
                if (*p == '}') { ++p; return true; }
                return false;
            }
        }
        if (*p == '[') {
            ++p;
            v.type = JVal::T::Arr;
            ws();
            if (p < end && *p == ']') { ++p; return true; }
            while (true) {
                JVal val;
                if (!parseVal(val)) return false;
                v.arr.push_back(std::move(val));
                ws();
                if (p >= end) return false;
                if (*p == ',') { ++p; continue; }
                if (*p == ']') { ++p; return true; }
                return false;
            }
        }
        if (*p == '"') {
            v.type = JVal::T::Str;
            return parseStr(v.str);
        }
        if (lit("true")) { v.type = JVal::T::Bool; v.b = true; return true; }
        if (lit("false")) { v.type = JVal::T::Bool; v.b = false; return true; }
        if (lit("null")) { v.type = JVal::T::Null; return true; }
        // number
        const char* s = p;
        if (p < end && (*p == '-' || *p == '+')) ++p;
        while (p < end && ((*p >= '0' && *p <= '9') || *p == '.' ||
                           *p == 'e' || *p == 'E' || *p == '-' ||
                           *p == '+'))
            ++p;
        if (p == s) return false;
        try {
            v.type = JVal::T::Num;
            v.num = std::stod(std::string(s, p));
        } catch (...) {
            return false;
        }
        return true;
    }
};

// ---------- GLB container ----------

struct GlbFile {
    std::string jsonText;
    std::vector<uint8_t> bin;
    std::string err;
};

bool readGlb(const std::string& path, GlbFile& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        out.err = "cannot open file";
        return false;
    }
    uint32_t magic = 0, version = 0, length = 0;
    f.read(reinterpret_cast<char*>(&magic), 4);
    f.read(reinterpret_cast<char*>(&version), 4);
    f.read(reinterpret_cast<char*>(&length), 4);
    if (magic != 0x46546C67u || version != 2) {
        out.err = "not a glTF 2.0 binary (.glb)";
        return false;
    }
    while (f) {
        uint32_t chunkLen = 0, chunkType = 0;
        f.read(reinterpret_cast<char*>(&chunkLen), 4);
        f.read(reinterpret_cast<char*>(&chunkType), 4);
        if (!f) break;
        std::vector<uint8_t> chunk(chunkLen);
        f.read(reinterpret_cast<char*>(chunk.data()),
               static_cast<std::streamsize>(chunkLen));
        if (!f) {
            out.err = "truncated chunk";
            return false;
        }
        if (chunkType == 0x4E4F534A) { // "JSON"
            out.jsonText.assign(chunk.begin(), chunk.end());
        } else if (chunkType == 0x004E4942) { // "BIN\0"
            out.bin = std::move(chunk);
        }
    }
    if (out.jsonText.empty()) {
        out.err = "no JSON chunk";
        return false;
    }
    return true;
}

// ---------- accessor -> float data ----------

bool readAccessorFloats(const JVal& root, const std::vector<uint8_t>& bin,
                        int accIndex, std::vector<float>& out) {
    const JVal* accs = root.get("accessors");
    const JVal* acc = accs ? accs->at(static_cast<size_t>(accIndex)) : nullptr;
    if (!acc) return false;
    if (acc->i("componentType") != 5126) return false; // FLOAT only
    const std::string type = acc->s("type");
    int comps = 0;
    if (type == "SCALAR") comps = 1;
    else if (type == "VEC3") comps = 3;
    else if (type == "VEC4") comps = 4;
    else return false;
    const int count = acc->i("count");
    const int bvIndex = acc->i("bufferView");
    const JVal* bvs = root.get("bufferViews");
    const JVal* bv = bvs ? bvs->at(static_cast<size_t>(bvIndex)) : nullptr;
    if (!bv) return false;
    const size_t bvOff = static_cast<size_t>(bv->i("byteOffset"));
    const size_t accOff = static_cast<size_t>(acc->i("byteOffset"));
    const size_t base = bvOff + accOff;
    const size_t stride = static_cast<size_t>(
        bv->n("byteStride", static_cast<double>(comps * 4)));
    const size_t need = base + static_cast<size_t>(count - 1) * stride +
                        static_cast<size_t>(comps * 4);
    if (count <= 0 || need > bin.size()) return false;
    out.resize(static_cast<size_t>(count) * static_cast<size_t>(comps));
    for (int k = 0; k < count; ++k) {
        const uint8_t* src = bin.data() + base +
                             static_cast<size_t>(k) * stride;
        float* dst = out.data() + static_cast<size_t>(k) *
                                        static_cast<size_t>(comps);
        std::memcpy(dst, src, static_cast<size_t>(comps) * 4);
    }
    return true;
}

// Quaternion (x,y,z,w) -> euler degrees, XYZ order (matches BoneTrack).
Vec3 quatToEulerDeg(float x, float y, float z, float w) {
    // Normalize defensively (glTF quats should already be unit).
    const float n =
        std::sqrt(x * x + y * y + z * z + w * w);
    if (n > 1e-6f) {
        x /= n; y /= n; z /= n; w /= n;
    }
    const float xx = x * x, yy = y * y, zz = z * z;
    const float m00 = 1.0f - 2.0f * (yy + zz);
    const float m01 = 2.0f * (x * y - z * w);
    const float m02 = 2.0f * (x * z + y * w);
    const float m12 = 2.0f * (y * z - x * w);
    const float m22 = 1.0f - 2.0f * (xx + yy);
    constexpr float kRad2Deg = 180.0f / 3.14159265358979323846f;
    Vec3 e;
    e.y = std::asin(std::max(-1.0f, std::min(1.0f, m02))) * kRad2Deg;
    e.x = std::atan2(-m12, m22) * kRad2Deg;
    e.z = std::atan2(-m01, m00) * kRad2Deg;
    return e;
}

// glTF animation name -> engine AnimationState. Unlisted names stay
// unbound (no matching engine state: jump/fall/crouch/sit/pick-up,
// emotes, holding/shooting, drive, wheelchair, interact, left-side
// attack alternates).
bool mapClipToState(const std::string& clipName, AnimationState& state) {
    if (clipName == "idle") { state = AnimationState::Idle; return true; }
    if (clipName == "walk") { state = AnimationState::Walk; return true; }
    if (clipName == "sprint") { state = AnimationState::Run; return true; }
    if (clipName == "die") { state = AnimationState::Death; return true; }
    if (clipName == "attack-melee-right") {
        state = AnimationState::Attack;
        return true;
    }
    if (clipName == "attack-kick-right") {
        state = AnimationState::Maul;
        return true;
    }
    return false;
}

} // namespace

bool extractGlbAnimations(const std::string& glbPath,
                          std::map<std::string, AnimationClip>& out,
                          std::string* err) {
    out.clear();
    GlbFile glb;
    if (!readGlb(glbPath, glb)) {
        if (err) *err = glb.err;
        return false;
    }
    JParser parser(glb.jsonText);
    JVal root;
    if (!parser.parseVal(root) || root.type != JVal::T::Obj) {
        if (err) *err = "JSON chunk did not parse";
        return false;
    }
    const JVal* anims = root.get("animations");
    const JVal* nodes = root.get("nodes");
    if (!anims || anims->type != JVal::T::Arr) {
        if (err) *err = "no animations array";
        return false;
    }
    for (size_t ai = 0; ai < anims->size(); ++ai) {
        const JVal* a = anims->at(ai);
        if (!a) continue;
        const std::string name =
            a->s("name", "anim" + std::to_string(ai));
        const JVal* channels = a->get("channels");
        const JVal* samplers = a->get("samplers");
        if (!channels || !samplers) continue;

        AnimationClip clip;
        clip.name = name;
        clip.sourcePath = glbPath;
        clip.loop = (name != "die"); // death is a one-shot; the rest loop
        double duration = 0.0;

        for (size_t ci = 0; ci < channels->size(); ++ci) {
            const JVal* ch = channels->at(ci);
            if (!ch) continue;
            const JVal* target = ch->get("target");
            const JVal* smp = samplers->at(
                static_cast<size_t>(ch->i("sampler")));
            if (!target || !smp) continue;
            const std::string path = target->s("path");
            if (path != "rotation" && path != "translation") continue;

            const int nodeIdx = target->i("node", -1);
            std::string joint = "node" + std::to_string(nodeIdx);
            if (nodeIdx >= 0 && nodes) {
                const JVal* node = nodes->at(
                    static_cast<size_t>(nodeIdx));
                if (node) joint = node->s("name", joint);
            }

            std::vector<float> times, values;
            if (!readAccessorFloats(root, glb.bin, smp->i("input"),
                                    times) ||
                times.empty())
                continue;
            if (!readAccessorFloats(root, glb.bin, smp->i("output"),
                                    values))
                continue;
            const size_t keyCount = times.size();
            const size_t comps = (path == "rotation") ? 4 : 3;
            if (values.size() < keyCount * comps) continue;

            BoneTrack track;
            track.bone = joint;
            for (size_t k = 0; k < keyCount; ++k) {
                Keyframe kf;
                kf.time = static_cast<double>(times[k]);
                if (kf.time > duration) duration = kf.time;
                const float* v = values.data() +
                                 static_cast<size_t>(k) * comps;
                if (path == "rotation") {
                    kf.rotEuler = quatToEulerDeg(v[0], v[1], v[2], v[3]);
                } else {
                    kf.pos = Vec3(v[0], v[1], v[2]);
                }
                track.keys.push_back(kf);
            }
            if (track.keys.empty()) continue;
            // Merge: a joint may have separate rotation/translation
            // channels — union the keys by time.
            BoneTrack& dst = clip.tracks[joint];
            if (dst.bone.empty()) dst.bone = joint;
            for (const Keyframe& kf : track.keys) {
                bool merged = false;
                for (Keyframe& ex : dst.keys) {
                    if (std::fabs(ex.time - kf.time) < 1e-6) {
                        if (path == "rotation")
                            ex.rotEuler = kf.rotEuler;
                        else
                            ex.pos = kf.pos;
                        merged = true;
                        break;
                    }
                }
                if (!merged) dst.keys.push_back(kf);
            }
        }
        // Sort merged keys by time (BoneTrack::sample requires order).
        for (auto& kv : clip.tracks) {
            std::sort(kv.second.keys.begin(), kv.second.keys.end(),
                      [](const Keyframe& a, const Keyframe& b) {
                          return a.time < b.time;
                      });
        }
        if (clip.tracks.empty()) continue;
        clip.durationSeconds = duration > 0.0 ? duration : 1.0;
        out[name] = std::move(clip);
    }
    if (out.empty()) {
        if (err) *err = "no usable animation channels";
        return false;
    }
    return true;
}

int bindEmbeddedSpeciesClips(AnimationStateMachine& sm,
                             const std::string& species,
                             const std::string& assetsRoot) {
    const std::string glbPath =
        assetsRoot + "/creatures/" + species + ".glb";
    std::map<std::string, AnimationClip> clips;
    if (!extractGlbAnimations(glbPath, clips)) return 0; // none: procedural
    int bound = 0;
    for (const auto& kv : clips) {
        AnimationState state;
        if (!mapClipToState(kv.first, state)) continue; // no engine state
        if (sm.hasClip(state)) continue; // custom .canim bound earlier wins
        sm.bindClip(state, kv.second);
        ++bound;
    }
    return bound;
}

// Case-insensitive name match against animationStateName() for every
// state. Returns true and sets 'state' on match.
bool matchStateName(const std::string& clipName, AnimationState& state) {
    std::string lower;
    lower.reserve(clipName.size());
    for (char c : clipName)
        lower += static_cast<char>(std::tolower(
            static_cast<unsigned char>(c)));
    for (int i = 0; i < static_cast<int>(AnimationState::Count); ++i) {
        const AnimationState s = static_cast<AnimationState>(i);
        std::string want = animationStateName(s);
        for (char& c : want)
            c = static_cast<char>(std::tolower(
                static_cast<unsigned char>(c)));
        if (lower == want) {
            state = s;
            return true;
        }
    }
    return false;
}

int bindPackCanimClips(AnimationStateMachine& sm,
                       const std::string& animPackDir) {
    if (animPackDir.empty()) return 0;
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::is_directory(animPackDir, ec)) return 0; // headless-safe
    int bound = 0;
    for (const auto& entry :
         fs::directory_iterator(animPackDir, ec)) {
        if (ec) break;
        if (!entry.is_regular_file(ec)) continue;
        if (entry.path().extension() != ".canim") continue;
        AnimationClip clip;
        if (!ClipSerializer::load(entry.path().string(), clip))
            continue; // unreadable: skipped silently, procedural covers it
        // ClipSerializer leaves sourcePath empty; mark the origin so
        // isProcedural() reports false for these real authored clips.
        clip.sourcePath = entry.path().string();
        AnimationState state;
        if (!matchStateName(clip.name, state)) continue; // e.g. "Cheer"
        if (sm.hasClip(state)) continue; // first binding wins
        sm.bindClip(state, std::move(clip));
        ++bound;
    }
    return bound;
}

void bindEntityClips(Entity& e, const std::string& assetsRoot) {
    // Wins order: (1) package .canim clips, (2) embedded glTF clips for
    // the species, (3) procedural fallbacks for the rest.
    bindPackCanimClips(e.anim(), e.animPackDir());
    const std::string species = e.species();
    if (!species.empty())
        bindEmbeddedSpeciesClips(e.anim(), species, assetsRoot);
    bindProceduralFallbacks(e.anim());
}

} // namespace cultulhu
