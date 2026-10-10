#pragma once

#include "animation/AnimationClip.h"
#include "assets/AssetManager.h" // RigDefinition

#include <string>
#include <vector>

namespace cultulhu {

// Hook for importing bone-animation clips from FBX files. This engine-agnostic
// core never links an FBX SDK; a future engine-side or editor-side
// implementation of this interface does the real parsing and converts the
// result into AnimationClips whose tracks are named after the rig's bones.
//
// TODO -- real implementation guide:
//   1. Implement this interface in the engine binding / tools target (Unreal,
//      or a standalone converter), linking the Autodesk FBX SDK there (or
//      assimp's FBX importer). Do NOT add the SDK to this core library.
//   2. Open the FBX file, walk its animation stacks/layers, and for each bone
//      node sample the translation + rotation animation curves over the take's
//      time span.
//   3. Map each FBX node name onto a RigDefinition bone name: exact match
//      first, then the rig's retargetProfile rename table (e.g. Mixamo
//      "mixamorig:Hips" -> "hips", "mixamorig:Spine" -> "spine").
//   4. Resample the curves into Keyframes (keep dense source keys, resample
//      sparse curves at ~30 Hz), convert rotations to euler degrees to match
//      the BoneTrack convention, and fill AnimationClip::tracks.
//   5. Persist every clip with ClipSerializer::save() to
//      assets/animations/<clip>.canim so the game loads them at runtime
//      without the SDK; set AnimationClip::sourcePath to the .canim path and
//      AnimationClip::name to the take name (or
//      AnimationStateMachine::mixamoClipName(state) when binding a Mixamo
//      pack per state).
//   6. Bind the result: machine.bindClip(state, importedClip). Clips bound
//      this way take precedence over bindProceduralFallbacks() output and
//      over the state machine's runtime procedural fallback.
class FbxClipImporter {
public:
    virtual ~FbxClipImporter() = default;

    // Import animation clips from an FBX file, mapping bone curves through
    // `rig` (see TODO above). Returns false -- leaving `out` untouched --
    // when the file cannot be read or contains no usable animation.
    virtual bool importFile(const std::string& path, const RigDefinition& rig,
                            std::vector<AnimationClip>& out) = 0;
};

} // namespace cultulhu
