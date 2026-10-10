#include "animation/AnimationClip.h"

#include <cmath>

namespace cultulhu {

Pose AnimationClip::sampleAt(double t) const {
    Pose pose;
    if (tracks.empty() || durationSeconds <= 0.0) return pose;
    double tt = t;
    if (loop) {
        tt = std::fmod(tt, durationSeconds);
        if (tt < 0.0) tt += durationSeconds;
    } else {
        if (tt < 0.0) tt = 0.0;
        if (tt > durationSeconds) tt = durationSeconds;
    }
    for (const auto& kv : tracks) {
        pose[kv.first] = kv.second.sample(tt);
    }
    return pose;
}

} // namespace cultulhu
