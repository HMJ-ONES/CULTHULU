#include "CultUlhuPlayerPawn.h"

#include "CultPrimaryProfile.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"

void ACultUlhuPlayerPawn::CreateVesselArms() {
 auto* LimbMesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere"));
 if(!LimbMesh) return;
 for(int Index=0;Index<6;++Index) {
  auto* Part=NewObject<UStaticMeshComponent>(this);
  Part->SetupAttachment(GetCapsuleComponent());
  Part->SetStaticMesh(LimbMesh);
  Part->SetMaterial(0,VesselBody_->GetMaterial(0));
  Part->SetCollisionEnabled(ECollisionEnabled::NoCollision);
  Part->ComponentTags.Add(TEXT("CultCorpseBody"));
  Part->RegisterComponent();
  VesselArms_.Add(Part);
 }
}

bool ACultUlhuPlayerPawn::FallbackStrikeLimb(int Index,FVector& Elbow,FVector& Hand) const {
 if(VesselArms_.Num()!=6||Index<0||Index>1) return false;
 Elbow=VesselArms_[Index*3+1]->GetComponentTransform().TransformPosition(FVector(0,0,-45));
 Hand=VesselArms_[Index*3+2]->GetComponentLocation();
 return true;
}

void ACultUlhuPlayerPawn::UpdateVesselMotion(float DeltaSeconds) {
 if(!VesselBody_||VesselBody_->ComponentHasTag(TEXT("CultReplacedVessel"))) return;
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
 if(VesselArms_.Num()!=6) return;
 const auto& Motion=AttackPlayback();
 const float Age=Time-Motion.Began;
 const bool Striking=Motion.Move==CultAttackMotion::EMove::Melee&&Age>=0&&Age<Motion.Duration&&!IsIncapacitated();
 const float Contact=FMath::Max(.02f,Motion.Contact);
 const float Sweep=FMath::Clamp((Age-(Contact-.10f))/.20f,0.f,1.f);
 const float Recovery=1-FMath::SmoothStep(Motion.Duration-.09f,Motion.Duration,Age);
 const auto Facing=FRotator(0,AimRotation().Yaw-GetActorRotation().Yaw,0);
 for(int Index=0;Index<2;++Index) {
  const float Side=Index==0?-1.f:1.f;
  const bool Active=Striking&&((Motion.Variant&3)==3||Index==((Motion.Variant&3)==1?0:1));
  const FVector Shoulder(0,Side*28,30), Rest(15,Side*46,-30);
  const float Angle=FMath::DegreesToRadians(Side*FMath::Lerp(72.f,-38.f,Sweep));
  const FVector Strike(69*FMath::Cos(Angle),69*FMath::Sin(Angle),-8+32*FMath::Clamp(AimRotation().Pitch/55.f,-1.f,1.f));
  const FVector Hand=Active?FMath::Lerp(Rest,Facing.RotateVector(Strike),FMath::SmoothStep(0.f,.10f,Age)*Recovery):Rest;
  const FVector Elbow=FMath::Lerp(Shoulder,Hand,.49)+FVector(-6,Side*8,-5);
  const FVector Points[]={Shoulder,Elbow,Hand};
  for(int Segment=0;Segment<2;++Segment) {
   auto* Limb=VesselArms_[Index*3+Segment].Get();
   Limb->SetRelativeLocationAndRotation((Points[Segment]+Points[Segment+1])*.5,
     FRotationMatrix::MakeFromZ(Points[Segment+1]-Points[Segment]).Rotator());
   Limb->SetRelativeScale3D(FVector(.22,.22,FVector::Dist(Points[Segment],Points[Segment+1])/90));
   Limb->SetVisibility(IsThirdPerson());
  }
  VesselArms_[Index*3+2]->SetRelativeLocation(Hand);
  VesselArms_[Index*3+2]->SetRelativeScale3D(FVector(.29,.22,.19));
  VesselArms_[Index*3+2]->SetVisibility(IsThirdPerson());
 }
}
