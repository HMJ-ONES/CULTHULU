#include "animation/ClipSerializer.h"

#include <fstream>
#include <iomanip>
#include <sstream>

namespace cultulhu {

bool ClipSerializer::save(const AnimationClip& clip, const std::string& path) {
    std::ofstream out(path, std::ios::trunc);
    if (!out.is_open()) return false;
    out << std::setprecision(9) << std::fixed;
    out << "CLIP " << std::quoted(clip.name) << ' ' << clip.durationSeconds
        << ' ' << (clip.loop ? 1 : 0) << '\n';
    for (const auto& kv : clip.tracks) {
        const BoneTrack& track = kv.second;
        out << "TRACK " << track.bone << ' ' << track.keys.size() << '\n';
        for (const Keyframe& k : track.keys) {
            out << "KEY " << k.time << ' ' << k.pos.x << ' ' << k.pos.y << ' '
                << k.pos.z << ' ' << k.rotEuler.x << ' ' << k.rotEuler.y << ' '
                << k.rotEuler.z << '\n';
        }
    }
    out.flush();
    return static_cast<bool>(out);
}

bool ClipSerializer::load(const std::string& path, AnimationClip& out) {
    std::ifstream in(path);
    if (!in.is_open()) return false;

    AnimationClip clip;
    std::string token;
    if (!(in >> token) || token != "CLIP") return false;
    int loopInt = 0;
    if (!(in >> std::quoted(clip.name) >> clip.durationSeconds >> loopInt))
        return false;
    clip.loop = (loopInt != 0);
    if (clip.durationSeconds < 0.0 ||
        !(clip.durationSeconds >= 0.0))  // NaN duration: corrupt file
        return false;

    while (in >> token) {
        if (token != "TRACK") return false;
        BoneTrack track;
        size_t nkeys = 0;
        if (!(in >> track.bone >> nkeys)) return false;
        // Wave 9d: nkeys comes from the file. Without a cap, a corrupt
        // file claiming SIZE_MAX keys makes reserve() throw length_error
        // (uncaught -> terminate). Real clips have hundreds of keys.
        if (nkeys > 1000000) return false;
        track.keys.reserve(nkeys);
        for (size_t i = 0; i < nkeys; ++i) {
            if (!(in >> token) || token != "KEY") return false;
            Keyframe k;
            float px, py, pz, rx, ry, rz;
            if (!(in >> k.time >> px >> py >> pz >> rx >> ry >> rz))
                return false;
            k.pos = Vec3(px, py, pz);
            k.rotEuler = Vec3(rx, ry, rz);
            track.keys.push_back(k);
        }
        clip.tracks[track.bone] = std::move(track);
    }
    if (in.bad()) return false;
    out = std::move(clip);
    return true;
}

} // namespace cultulhu
