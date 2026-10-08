#pragma once

// Wave 18: extractor for animations EMBEDDED in .glb files (glTF 2.0
// "animations" array). Three creature models ship with 32 embedded clips
// each (dagon_spawn, risen_dead, wraith — Kenney/KayKit-style packs, CC0):
// the extractor reads the JSON chunk, resolves channel->node->joint names
// and sampler->accessor->BIN data, and builds AnimationClip+BoneTrack data
// in the engine's own conventions (rotEuler in degrees, XYZ order).
//
// Clip-selection contract (wins rule, same as bindProceduralFallbacks):
//   1. custom .canim clips from the entity's package dir always win,
//   2. embedded glTF clips bind only states that have no clip yet,
//   3. procedural fallbacks fill whatever is still unbound.
// So: bindPackCanimClips() first, then bindEmbeddedSpeciesClips(), then
// bindProceduralFallbacks(). bindEntityClips() below does exactly this
// ordering for a whole entity.
//
// Supported: rotation (quaternion) and translation channels, LINEAR
// interpolation, float accessors. Other paths/interpolations are skipped.

#include "animation/AnimationClip.h"
#include "animation/AnimationStateMachine.h"
#include "entities/Entity.h" // bindEntityClips takes Entity& (wave 18)

#include <map>
#include <string>

namespace cultulhu {

class Entity; // entities/Entity.h (forward: the header only needs the type)

// Bind custom .canim clips from a character package's animations dir
// (e.g. "assets/characters/cultist_hooded/animations"). Each file is loaded
// with ClipSerializer::load; the clip's CLIP name is matched
// case-insensitively against animationStateName(s) for every state, and
// bound on match. Missing/unreadable files are skipped silently (the
// procedural fallback covers the state). Only states without a clip are
// bound, so calling twice is safe. Returns the number of states bound.
int bindPackCanimClips(AnimationStateMachine& sm,
                       const std::string& animPackDir);

// Extract every embedded animation from a .glb file into AnimationClips
// keyed by glTF animation name. Returns false when the file is missing or
// not a readable GLB; err (when non-null) receives a short reason.
// clip.sourcePath is set to glbPath (never empty: these are real assets).
bool extractGlbAnimations(const std::string& glbPath,
                          std::map<std::string, AnimationClip>& out,
                          std::string* err = nullptr);

// Bind embedded clips for a creature species to animation states.
// Species with embedded clips: "dagon_spawn", "risen_dead", "wraith" (read
// from <assetsRoot>/creatures/<species>.glb). Only states without a clip
// are bound (custom .canim clips bound earlier win). Returns the number of
// states bound; 0 when the .glb is missing/unreadable or the species has
// none (skeleton-only models keep procedural fallbacks).
int bindEmbeddedSpeciesClips(AnimationStateMachine& sm,
                             const std::string& species,
                             const std::string& assetsRoot);

// Full clip-selection for one entity, in wins order:
//   1. package .canim clips (e.animPackDir(), set at spawn),
//   2. embedded glTF clips for its species (when any),
//   3. procedural fallbacks for the rest.
// The 9 skeleton-only models (no .canim dir, no embedded clips) end up
// fully procedural — verified by the wave-18 regression test.
void bindEntityClips(Entity& e, const std::string& assetsRoot);

} // namespace cultulhu
