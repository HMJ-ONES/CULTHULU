#include "CultMeleeGeometry.h"
#include "CultUlhuPlayerPawn.h"
#include "Components/CapsuleComponent.h"

CultMeleeGeometry::FProfile CultMeleeGeometry::Describe(const ACultUlhuPlayerPawn* Pawn,float ReachOverride) {
 const float Height=Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*2;
 return {Pawn->FeetLocation()+FVector(0,0,Height*.48f),ReachOverride>0?ReachOverride:Height*.9f,FMath::Clamp(Height*.035f,25.f,120.f)};
}
bool CultMeleeGeometry::InReach(const ACultUlhuPlayerPawn* Pawn,AActor* Target) {
 if(!Pawn||!IsValid(Target)) return false;
 const auto Profile=Describe(Pawn); FVector Closest;
 if(auto* Body=Target->FindComponentByClass<UCapsuleComponent>()) {
  const float Distance=Body->GetClosestPointOnCollision(Profile.Origin,Closest);
  return Distance>=0&&Distance<=Profile.Reach+Profile.Radius;
 }
 const FBox Bounds=Target->GetComponentsBoundingBox(true);
 return Bounds.IsValid&&FVector::Dist(Profile.Origin,Bounds.GetClosestPointTo(Profile.Origin))<=Profile.Reach+Profile.Radius;
}
float CultMeleeGeometry::ApproachDistance(const ACultUlhuPlayerPawn* Pawn,AActor* Target) {
 if(!Pawn||!IsValid(Target)) return 400;
 const auto Profile=Describe(Pawn);
 auto* Body=Target->FindComponentByClass<UCapsuleComponent>(); if(!Body) return 400;
 const float Radius=Body->GetScaledCapsuleRadius(),Spine=Body->GetScaledCapsuleHalfHeight()-Radius;
 const float Vertical=FMath::Max(0.f,float(FMath::Abs(Profile.Origin.Z-Body->GetComponentLocation().Z))-Spine);
 const float Reach=Profile.Reach+Profile.Radius+Radius;
 const float Horizontal=FMath::Sqrt(FMath::Max(0.f,Reach*Reach-Vertical*Vertical));
 // Aim inside plausible contact range rather than stopping on its exact edge.
 // Physics still walks the real route; the swing remains a committed sweep.
 return FMath::Clamp(Horizontal*.8f,200.f,1800.f);
}
