#pragma once

// CULT-ULHU rig auto-mapper (wave 7).
//
// Matches an FBX's bone-name list against the engine's expected 11-bone
// humanoid rig (see animation/ProceduralClips.cpp humanoidBones()).
// Matching is case-insensitive with alias tables for common conventions
// (Mixamo "mixamorig:*", Blender Rigify, generic DCC names). Every bone
// gets a confidence score; unmatched bones are reported explicitly —
// never a silent failure.
//
// In production the bone list comes from the FBX importer (hook in
// animation/FbxClipImporter.h). Until that lands, packages may ship an
// optional `bones.list` sidecar (one bone name per line) so mapping can
// be exercised without an FBX SDK.

#include <string>
#include <vector>

namespace cultulhu {

struct BoneMap {
    std::string engineBone; // e.g. "upperArmL"
    std::string fbxBone;    // matched source name, "" when unmapped
    float confidence = 0.0f; // 1.0 exact, 0.9 alias, 0.0 unmapped
    bool mapped = false;
};

struct RigMapping {
    std::vector<BoneMap> bones; // one entry per engine bone, in rig order

    // Mean confidence over mapped bones (0 when nothing mapped).
    float overallConfidence() const;
    // Engine bone names with no source match.
    std::vector<std::string> unmapped() const;
    int mappedCount() const;
};

class RigMapper {
public:
    // The engine's canonical 11-bone rig, in order.
    static const std::vector<std::string>& engineBones();

    // Match fbxBones against the engine rig. First candidate claiming an
    // engine bone wins (deterministic).
    static RigMapping mapBones(const std::vector<std::string>& fbxBones);

    // Parse an explicit rig.map file: lines of "engine_bone = fbx_bone".
    // Returns the mapping with confidence 1.0 on every parsed line and
    // fills warnings for unknown engine bones / malformed lines.
    static RigMapping parseRigMap(const std::string& text,
                                  std::vector<std::string>& warnings);

private:
    // Lowercase, strip "mixamorig:" prefix, drop non-alphanumerics.
    static std::string normalize(const std::string& name);
    // Normalized aliases per engine bone (excluding the engine name
    // itself, which always matches at 1.0 case-insensitively).
    static const std::vector<std::string>& aliasesFor(
        const std::string& engineBone);
};

} // namespace cultulhu
