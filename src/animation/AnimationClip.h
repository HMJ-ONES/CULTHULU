#pragma once

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

    bool isProcedural() const { return sourcePath.empty(); }
};

} // namespace cultulhu
