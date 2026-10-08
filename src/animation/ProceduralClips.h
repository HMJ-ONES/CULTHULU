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
AnimationClip makeCast();          // ~1.0s one-shot: arms thrust forward,
                                  // channel the spell, recover
AnimationClip makeStunned();       // ~0.9s one-shot: reel back, arms flail,
                                  // head shake
AnimationClip makeChannel();       // ~2.0s loop: arms raised overhead,
                                  // held with a tremble (sustained casts)
AnimationClip makeCastWave();      // ~1.2s one-shot: wide sweeping gesture
                                  // for RMB kits (Wave of Domination)
AnimationClip makeFearRun();       // ~0.55s loop: panicked sprint — faster/
                                  // harder than Run, hunched torso, flailing
                                  // arms, jerking head, high hip bob with
                                  // lateral jitter
AnimationClip makeBrawl();         // ~0.9s one-shot: alternating wild
                                  // haymakers, torso twist, midsection
                                  // grapple-shake, stagger back at the end
AnimationClip makeSacrificePerformer(); // ~2.0s loop: ceremonial — arms
                                  // raised overhead pulsing slowly up-down,
                                  // slight torso sway, head tilted up
AnimationClip makeSacrificeVictim(); // ~1.6s loop: kneeling victim, hips low,
                                  // torso hunched, arms bound behind, periodic
                                  // struggle bursts (~every 0.5s)
AnimationClip makeMaul();          // ~1.0s one-shot: crouch-pounce forward,
                                  // alternating raking arm tears, head snap,
                                  // ends in a low crouch
AnimationClip makeWarBattle();     // ~1.1s loop: disciplined war fighting —
                                  // clean measured weapon arcs, shield-block
                                  // raises, advancing steps. Trained
                                  // soldiers, not a bar brawl
AnimationClip makeBuild();         // ~1.8s loop: the construction cycle —
                                  // overhead hammering, bend-lift-carry,
                                  // crouch-place-stand
AnimationClip makeRepair();        // ~1.6s loop: kneeling repair work —
                                  // small hammering/fitting motions at waist
                                  // height, inspection pauses (lean back,
                                  // head tilt)

// Bone names every generator (and the FBX hook) agrees on.
const std::vector<std::string>& humanoidBones();

class AnimationStateMachine; // forward decl; defined in AnimationStateMachine.h

// Bind generated clips for every state that has no clip bound yet. Idle,
// Walk, Run, Attack, Death, Cast, Stunned, Channel, CastWave, FearRun, Brawl,
// SacrificePerformer, SacrificeVictim, Maul, WarBattle, Build and Repair get
// procedural clips; custom .canim clips bound earlier always win.
// Levitate/Launch/Levitated stay unbound: they are victim-side states
// driven at runtime by the Wave of Domination logic.
void bindProceduralFallbacks(AnimationStateMachine& sm);

} // namespace cultulhu
