#include "CultCharacterModel.h"
#include "CultFluidAnimInstance.h"
#include "CultPrimaryProfile.h"
#include "CultUlhuPlayerPawn.h"
#include "Components/SkeletalMeshComponent.h"

void ACultCharacterModel::UpdateCombatGesture(float DeltaSeconds) {
 const float Age=ActionSeconds_-(ActionUntil_-GetWorld()->GetTimeSeconds());
 CultPrimary::FGesture Pose;
 if(ActionRole_==TEXT("primary_fire")) Pose=CultPrimary::ReleaseGesture(Age,CultPrimary::Find(CharacterId_).Windup,ActionSeconds_);
 if(ActionRole_==TEXT("rock_throw")) Pose=CultPrimary::ReleaseGesture(Age,.10f,ActionSeconds_);
 if(ActionRole_==TEXT("cast_light")||ActionRole_==TEXT("cast_heavy")) Pose=CultPrimary::ReleaseGesture(Age,ActionRole_==TEXT("cast_heavy")?.37f:.23f,ActionSeconds_);
 if(ActionRole_.StartsWith(TEXT("melee_"))&&CharacterId_!=TEXT("cthulhu_avatar")) Pose=CultPrimary::ReleaseGesture(Age,.22f,ActionSeconds_);
 if(Pawn_->IsIncapacitated()||Pawn_->IsDashing()) Pose={};
 const float Lateral=FVector::DotProduct(GetActorRightVector(),Pawn_->GetVelocity())/FMath::Max(1.f,WalkSpeed_);
 const float Lean=FMath::Clamp(Lateral,-1.f,1.f);
 const bool Cthulhu=CharacterId_==TEXT("cthulhu_avatar");
 GuardWeight_=FMath::FInterpTo(GuardWeight_,Cthulhu?(Pawn_->HoldingRock()?1.f:0.f):Lean,DeltaSeconds,7.5f);

 uint8 Rig=0;
 if(Cthulhu) Rig=1;
 if(CharacterId_==TEXT("mi_go")) Rig=2;
 if(CharacterId_==TEXT("yog_sothoth")) Rig=3;
 if(CharacterId_==TEXT("dagon")) Rig=4;
 if(CharacterId_==TEXT("nyarlathotep")) Rig=5;
 if(CharacterId_==TEXT("shoggoths")) Rig=6;
 for(UMeshComponent* Part:Parts_) {
  auto* Skin=Cast<USkeletalMeshComponent>(Part);
  auto* Anim=Skin?Cast<UCultFluidAnimInstance>(Skin->GetAnimInstance()):nullptr;
  if(!Anim) continue;
  Anim->Gather=Pose.Gather;
  Anim->Snap=Pose.Snap;
  Anim->Guard=GuardWeight_;
  Anim->GestureRig=Rig;
 }
 if(bOriginalOrb_&&Parts_.Num()==3) {
  // The Colour contracts before firing and surges through its mist.
  Parts_[0]->SetRelativeScale3D(Parts_[0]->GetRelativeScale3D()*FVector(
   1.f-.16f*Pose.Gather+.29f*Pose.Snap,1.f+.12f*Pose.Gather-.16f*Pose.Snap,1.f+.12f*Pose.Gather-.16f*Pose.Snap));
  for(int32 I=1;I<3;++I) {
   Parts_[I]->AddRelativeLocation(FVector(21*Pose.Snap-7*Pose.Gather,0,0));
   Parts_[I]->AddRelativeRotation(FRotator(0,(I==1?1:-1)*(17*Pose.Gather+30*Pose.Snap),0));
  }
 }
 const bool Yig=CharacterId_==TEXT("yig");
 if(!Yig&&CharacterId_!=TEXT("nodens")) return;
 if(StaticRest_.IsEmpty()) for(UMeshComponent* Part:Parts_) StaticRest_.Add(Part->GetRelativeTransform());
 const float Speed=Pawn_->GetVelocity().Size2D();
 const float Roll=FMath::Sin(GetWorld()->GetTimeSeconds()*6.3f)*FMath::Clamp(Speed/800.f,0.f,1.f);
 // Rotate the tail, armor and every sword as a group so their grips stay joined.
 const FTransform Accent=Yig?FTransform(FRotator(0,5*GuardWeight_+4*Pose.Gather-5*Pose.Snap,0))
  :FTransform(FRotator(3*Pose.Gather-7*Pose.Snap,0,1.6f*Roll),FVector(10*Pose.Snap,0,2*Roll));
 for(int32 I=0;I<Parts_.Num();++I) Parts_[I]->SetRelativeTransform(StaticRest_[I]*Accent);
}
