#pragma once

#include "animation/AnimationClip.h"

#include <string>
#include <vector>

namespace cultulhu {

// Procedural animation clips for the generic humanoid rig. These keep every
// entity animating (headless or pre-FBX) and serve as reference motion the
// FBX importer can later replace. Rotations are euler degrees, matching the
// BoneTrack convention.
AnimationClip makeWalk();         // ~1.0s loop: legs swing opposite phase,
                                  // arms counter-swing, hips bob
AnimationClip makeRun();          // ~0.6s loop: bigger swing, forward lean
AnimationClip makeIdle();         // ~2.0s loop: breathing, slight sway
AnimationClip makeAttackSwing();  // ~0.8s one-shot: overhead swing
AnimationClip makeDeath();        // ~1.2s one-shot: crumple to the ground

// Bone names every generator (and the FBX hook) agrees on.
const std::vector<std::string>& humanoidBones();

class AnimationStateMachine; // forward decl; defined in AnimationStateMachine.h

// Bind generated clips for every state that has no clip bound yet. Idle,
// Walk, Run, Attack and Death get procedural clips; Cast, Stunned and Channel
// intentionally stay unbound so the state machine's runtime procedural
// fallback covers them until real FBX/Mixamo clips are imported.
void bindProceduralFallbacks(AnimationStateMachine& sm);

} // namespace cultulhu
