#pragma once

#include "animation/AnimationClip.h"

#include <string>

namespace cultulhu {

// Text save/load for animation clips, persisted under assets/animations/ as
// ".canim" files. The format is line-based and human readable/editable:
//
//   CLIP "name" <durationSeconds> <loop 0|1>
//   TRACK <bone> <nkeys>
//   KEY <time> <px> <py> <pz> <rx> <ry> <rz>
//   ...
//
// Bone names must not contain whitespace (the generic humanoid rig's names
// don't). Clip names are double-quoted so they may contain spaces. Floats are
// written with enough precision for an exact save->load round trip.
class ClipSerializer {
public:
    // Default directory for persisted clips, relative to the working dir.
    static const char* animationsDir() { return "assets/animations"; }

    // Write a clip to path. Returns false on I/O failure.
    static bool save(const AnimationClip& clip, const std::string& path);
    // Read a clip from path into out (out is left untouched on failure).
    // Returns false on I/O failure or malformed input.
    static bool load(const std::string& path, AnimationClip& out);
};

} // namespace cultulhu
