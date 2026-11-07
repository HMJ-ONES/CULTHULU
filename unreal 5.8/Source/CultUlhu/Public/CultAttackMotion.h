#pragma once

#include "CoreMinimal.h"

namespace CultAttackMotion {
enum class EMove : uint8 {
 None, Primary, Heavy, Q, F, R, Melee, RockLift, RockThrow, Lower,
 MinionLift, MinionThrow, Wave, WaveHold, WaveThrow, Count
};

// Model-space channels. Feet and collision remain driven by CharacterMovement.
struct FFrame {
 FVector4 Body{0,0,0,0};     // pitch, yaw, forward shift (cm), compression (cm)
 FVector4 Limbs{0,0,0,0};    // shoulder pitch, outward sweep, elbow bend, jaw
 FVector4 Details{0,0,0,0};  // head pitch, appendage fan, follow-through, expansion
 float Lead=0;               // signed bias toward one side
};

struct FPlayback {
 EMove Move=EMove::None;
 uint8 Variant=0;
 float Began=0;
 float Contact=0;
 float Duration=0;
 FFrame Previous;
};

EMove AbilitySlot(int32 Slot);
inline bool IsHold(EMove Move) { return Move==EMove::RockLift||Move==EMove::MinionLift||Move==EMove::WaveHold; }
bool IsSurge(const FString& Id,EMove Move);
FFrame Blend(const FFrame& A,const FFrame& B,float Alpha);
FFrame Sample(const FString& Id,const FPlayback& Playback,float Now);
}
