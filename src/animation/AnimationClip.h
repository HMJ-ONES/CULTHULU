#pragma once

#include "animation/BoneTrack.h"

#include <map>
#include <string>

namespace cultulhu {

// One animation clip. sourcePath is empty for procedural fallback clips (the
// state machine still advances logically; the engine binding plays a
// procedural pose or a default). Mixamo-standard names are used so downloaded
// packs bind with zero renaming (see AnimationStateMachine::mixamoClipName).
struct AnimationClip {
    std::string name;        // e.g. "Walking"
    double durationSeconds = 1.0;
    bool loop = true;
    std::string sourcePath;  // FBX/anim file; empty = procedural fallback

    // Per-bone keyframe tracks. Empty when no data was imported/generated
    // yet; procedural generators (ProceduralClips.h) fill these in, and
    // ClipSerializer persists them to assets/animations/*.canim.
    std::map<std::string, BoneTrack> tracks;

    bool isProcedural() const { return sourcePath.empty(); }
    bool hasTrackData() const { return !tracks.empty(); }

    // Sample the full pose at time t (seconds). Wraps t into
    // [0, durationSeconds) for looping clips; clamps to the ends for
    // one-shots. Returns an empty pose when there are no tracks or the
    // duration is not positive.
    Pose sampleAt(double t) const;
};

} // namespace cultulhu
