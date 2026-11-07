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
 using Accent=CultPrimary::EAccent;
 const auto Primary=CultPrimary::Find(CharacterId_);
 const float Time=GetWorld()->GetTimeSeconds();
 const auto Strike=CultPrimary::ReleaseGesture(Time-VesselCueAt_,VesselCue_==3?Primary.Windup:.16f,VesselCueDuration_);
 const float Weight=FMath::Clamp(GetVelocity().Size2D()/FMath::Max(1.f,WalkSpeed_),0.f,1.f);
 const float Wave=FMath::Sin(Time*(Primary.Accent==Accent::Maul?4.2f:6.6f));
 float Pitch=2.f*Weight, Roll=2.2f*Wave*Weight, Yaw=0;
 const float Gather=Strike.Gather, Snap=Strike.Snap;
 switch(Primary.Accent) {
  case Accent::Maul: Pitch+=-10*Gather+18*Snap; break;
  case Accent::Lunge: Pitch+=-15*Gather+23*Snap; break;
  case Accent::Rake: Roll+=-8*Gather+15*Snap; Pitch+=12*Snap; break;
  case Accent::Coil: Yaw=22*Gather-28*Snap; Roll+=6*Snap; break;
  case Accent::Chant: Pitch+=-8*Gather+9*Snap; Roll+=5*Gather*Wave; break;
  default: Pitch+=-8*Gather+13*Snap; break;
 }
 const float Bob=FMath::Abs(Wave)*3.f*Weight+FMath::Sin(Time*1.7f)*1.2f;
 const FTransform Gesture(FRotator(Pitch,Yaw,Roll),FVector(9*Snap,0,Bob-4*Gather),
                           FVector(1+.05f*Gather,1+.05f*Gather,1.f-.10f*Gather+.08f*Snap));
 for(const auto& Part:VesselRest_) if(auto* Visual=Part.Key.Get()) Visual->SetRelativeTransform(Part.Value*Gesture);
}
