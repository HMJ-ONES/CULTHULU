#pragma once
#include "CoreMinimal.h"
class ACultUlhuPlayerPawn;
class AActor;
// The physical arm sweep and bot reach decisions share one size definition.
namespace CultMeleeGeometry {
struct FProfile { FVector Origin; float Reach,Radius; };
FProfile Describe(const ACultUlhuPlayerPawn* Pawn,float ReachOverride=0);
bool InReach(const ACultUlhuPlayerPawn* Pawn,AActor* Target);
float ApproachDistance(const ACultUlhuPlayerPawn* Pawn,AActor* Target);
}
