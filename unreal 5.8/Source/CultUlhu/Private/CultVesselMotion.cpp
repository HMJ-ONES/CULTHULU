#include "CultUlhuPlayerPawn.h"

#include "CultPrimaryProfile.h"
#include "Components/StaticMeshComponent.h"

void ACultUlhuPlayerPawn::UpdateVesselMotion(float DeltaSeconds) {
 if(!VesselBody_||VesselBody_->ComponentHasTag(TEXT("CultReplacedVessel"))||!VesselBody_->IsVisible()) return;
 if(VesselRest_.IsEmpty()) {
  TArray<UStaticMeshComponent*> Parts;
  GetComponents(Parts);
  for(auto* Part:Parts) if(Part==VesselBody_||Part->ComponentHasTag(TEXT("VesselEye"))) {
   VesselRest_.Emplace(Part,Part->GetRelativeTransform());
  }
 }
 const auto Pose=AttackFrame();
 const bool Heavy=CultPrimary::Find(CharacterId_).Accent==CultPrimary::EAccent::Maul;
 const float Time=GetWorld()->GetTimeSeconds();
 const float Weight=FMath::Clamp(GetVelocity().Size2D()/FMath::Max(1.f,WalkSpeed_),0.f,1.f);
 const float Wave=FMath::Sin(Time*(Heavy?4.2f:6.6f));
 const double Roll=3.2f*Wave*Weight+Pose.Details.Z*.45;
 const double Bob=FMath::Abs(Wave)*4.4f*Weight+FMath::Sin(Time*1.7f)*1.2f;
 const double Compression=Pose.Body.W/85;
 const FTransform Gesture(FRotator(-Pose.Body.X+2*Weight,Pose.Body.Y,Roll),
  FVector(Pose.Body.Z,0,Bob+Pose.Body.W),
  FVector(1+Pose.Details.W*.60,1+Pose.Details.W*.60,1+Compression-Pose.Details.W*.22));
 for(const auto& Part:VesselRest_) if(auto* Visual=Part.Key.Get()) Visual->SetRelativeTransform(Part.Value*Gesture);
}
