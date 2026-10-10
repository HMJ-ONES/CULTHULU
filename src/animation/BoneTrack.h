#pragma once

#include "core/Vec3.h"

#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace cultulhu {

// Transform of a single bone, expressed as an offset from the rig's rest
// pose. rotEuler is in degrees, XYZ order; the engine binding (Unreal later)
// converts to its own rotation type.
struct BoneTransform {
    Vec3 pos;       // position offset
    Vec3 rotEuler;  // degrees
};

inline BoneTransform lerpBoneTransform(const BoneTransform& a,
                                       const BoneTransform& b, double t) {
    const float ft = static_cast<float>(t);
    BoneTransform out;
    out.pos = a.pos + (b.pos - a.pos) * ft;
    out.rotEuler = a.rotEuler + (b.rotEuler - a.rotEuler) * ft;
    return out;
}

// One keyframe on a bone track.
struct Keyframe {
    double time = 0.0;  // seconds from clip start
    Vec3 pos;
    Vec3 rotEuler;      // degrees
};

// All keyframes for one named bone. Keys must be in ascending time order
// (generators and the serializer guarantee this).
struct BoneTrack {
    std::string bone;
    std::vector<Keyframe> keys;

    bool empty() const { return keys.empty(); }

    // Linear interpolation between surrounding keyframes. Clamps to the
    // first/last key outside the key range; returns identity for no keys.
    BoneTransform sample(double t) const {
        BoneTransform out;
        if (keys.empty()) return out;
        const Keyframe& first = keys.front();
        const Keyframe& last = keys.back();
        if (t <= first.time) {
            out.pos = first.pos;
            out.rotEuler = first.rotEuler;
            return out;
        }
        if (t >= last.time) {
            out.pos = last.pos;
            out.rotEuler = last.rotEuler;
            return out;
        }
        for (size_t i = 1; i < keys.size(); ++i) {
            if (t <= keys[i].time) {
                const Keyframe& a = keys[i - 1];
                const Keyframe& b = keys[i];
                const double span = b.time - a.time;
                const double f = span > 1e-9 ? (t - a.time) / span : 0.0;
                const float ff = static_cast<float>(f);
                out.pos = a.pos + (b.pos - a.pos) * ff;
                out.rotEuler = a.rotEuler + (b.rotEuler - a.rotEuler) * ff;
                return out;
            }
        }
        out.pos = last.pos;
        out.rotEuler = last.rotEuler;
        return out;
    }
};

// A full-body pose: bone name -> transform. std::map keeps serialization
// order deterministic.
using Pose = std::map<std::string, BoneTransform>;

// Union-blend of two poses by t in [0,1]. Bones present in both are lerped;
// a bone missing from A snaps to B's value (and vice versa), so blending
// into a clip that animates extra bones just works.
inline Pose blendPoses(const Pose& a, const Pose& b, double t) {
    Pose out;
    for (const auto& kv : a) {
        const auto it = b.find(kv.first);
        out[kv.first] = (it == b.end())
                            ? kv.second
                            : lerpBoneTransform(kv.second, it->second, t);
    }
    for (const auto& kv : b) {
        if (a.find(kv.first) == a.end()) out[kv.first] = kv.second;
    }
    return out;
}

} // namespace cultulhu
