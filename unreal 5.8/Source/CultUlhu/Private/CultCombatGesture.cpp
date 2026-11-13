#include "CultCharacterModel.h"

#include "CultFluidAnimInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/CharacterMovementComponent.h"

void ACultCharacterModel::UpdateCombatGesture(float DeltaSeconds) {
 auto Pose=Pawn_->AttackFrame();
 const float Lateral=FVector::DotProduct(GetActorRightVector(),Pawn_->GetVelocity())/FMath::Max(1.f,WalkSpeed_);
 GuardWeight_=FMath::FInterpTo(GuardWeight_,FMath::Clamp(Lateral,-1.f,1.f),DeltaSeconds,8.f);
 if(Pawn_->IsDashing()) {
  const float Direction=FVector::DotProduct(GetActorForwardVector(),Pawn_->GetVelocity().GetSafeNormal2D());
  Pose.Body.X+=12.f*Direction;
  Pose.Body.W-=2.5f;
  Pose.Details.Y+=Direction<-.2f?14.f:7.f;
 } else if(Pawn_->GetCharacterMovement()->IsFalling()||Pawn_->GetCharacterMovement()->IsFlying()) {
  const float Lift=FMath::Clamp(Pawn_->GetVelocity().Z/FMath::Max(1.f,WalkSpeed_),-1.f,1.f);
  Pose.Body.X-=Lift*8.f;
  Pose.Details.Y+=FMath::Abs(Lift)*8.f;
 }

 uint8 Rig=0;
 if(CharacterId_==TEXT("cthulhu_avatar")) Rig=1;
 if(CharacterId_==TEXT("mi_go")) Rig=2;
 if(CharacterId_==TEXT("yog_sothoth")) Rig=3;
 if(CharacterId_==TEXT("dagon")) Rig=4;
 if(CharacterId_==TEXT("nyarlathotep")) Rig=5;
 if(CharacterId_==TEXT("shoggoths")) Rig=6;
 const auto& Motion=Pawn_->AttackPlayback();
 const float Age=GetWorld()->GetTimeSeconds()-Motion.Began;
 const bool Melee=Motion.Move==CultAttackMotion::EMove::Melee&&Age>=0&&Age<Motion.Duration&&!Pawn_->IsIncapacitated();
 for(UMeshComponent* Part:Parts_) {
  auto* Skin=Cast<USkeletalMeshComponent>(Part);
  auto* Anim=Skin?Cast<UCultFluidAnimInstance>(Skin->GetAnimInstance()):nullptr;
  if(!Anim) continue;
  Anim->Attack=Pose;
  Anim->LateralLean=GuardWeight_;
  Anim->GestureRig=Rig;
  Anim->MeleeStage=Melee?Motion.Variant&3:0;
  Anim->MeleeProgress=(Age-Motion.Contact)/.10f;
  Anim->MeleePitch=FMath::Clamp(FRotator::NormalizeAxis(Pawn_->AimRotation().Pitch),-53.f,45.f);
 }

 if(bOriginalOrb_&&Parts_.Num()==3) {
  // The light compresses along the firing axis, then the mist catches up.
  const double Expansion=Pose.Details.W;
  Parts_[0]->SetRelativeScale3D(Parts_[0]->GetRelativeScale3D()*FVector(1+Expansion,1-.42*Expansion,1-.42*Expansion));
  Parts_[0]->AddRelativeLocation(FVector(Pose.Body.Z,0,Pose.Body.W*.75));
  for(int32 Index=1;Index<3;++Index) {
   const double Side=Index==1?1:-1;
   Parts_[Index]->AddRelativeLocation(FVector(Pose.Body.Z*.55,Side*Pose.Details.Y*.3,Pose.Body.W*.8));
   Parts_[Index]->AddRelativeRotation(FRotator(Pose.Body.X*.6,Pose.Body.Y+Side*Pose.Details.Y,Pose.Details.Z*Side));
  }
 }

 const bool Yig=CharacterId_==TEXT("yig");
 if(bOriginalOrb_) return;
 if(!Yig&&CharacterId_!=TEXT("nodens")) {
  if(StaticRest_.IsEmpty()) for(UMeshComponent* Part:Parts_) StaticRest_.Add(Part->GetRelativeTransform());
  const FTransform Gesture(FRotator(-Pose.Body.X*.7,Pose.Body.Y*.62,Pose.Details.Z*.32),
   FVector(Pose.Body.Z,0,Pose.Body.W*.7));
  for(int Index=0;Index<Parts_.Num();++Index) if(Cast<UStaticMeshComponent>(Parts_[Index]))
   Parts_[Index]->SetRelativeTransform(StaticRest_[Index]*Gesture);
  return;
 }
 if(StaticRest_.IsEmpty()) for(UMeshComponent* Part:Parts_) StaticRest_.Add(Part->GetRelativeTransform());
 const float Speed=Pawn_->GetVelocity().Size2D();
 const float WheelRoll=FMath::Sin(GetWorld()->GetTimeSeconds()*6.3f)*FMath::Clamp(Speed/800.f,0.f,1.f);
 // Yig's tail, armor and swords share the same pivot so the grip never opens.
 const FTransform Accent=Yig
  ?FTransform(FRotator(0,5*GuardWeight_+Pose.Body.Y*.42,0),FVector(Pose.Body.Z*.42,0,0))
  :FTransform(FRotator(-Pose.Body.X*.40,Pose.Body.Y*.54,1.6f*WheelRoll+Pose.Details.Z*.12),
              FVector(Pose.Body.Z*.68,0,2*WheelRoll+Pose.Body.W*.55));
 for(int32 Index=0;Index<Parts_.Num();++Index) Parts_[Index]->SetRelativeTransform(StaticRest_[Index]*Accent);
 if(Yig) {
  USkeletalMeshComponent* Torso=nullptr;
  USkeletalMeshComponent* Tail=nullptr;
  USkeletalMeshComponent* Weapons=nullptr;
  for(const auto& Part:Parts_) if(auto* Skin=Cast<USkeletalMeshComponent>(Part.Get())) {
   if(Skin->GetBoneIndex(TEXT("hips"))!=INDEX_NONE) Torso=Skin;
   if(Skin->GetBoneIndex(TEXT("tail01"))!=INDEX_NONE) Tail=Skin;
   if(Skin->GetBoneIndex(TEXT("weaponsroot"))!=INDEX_NONE) Weapons=Skin;
  }
  if(Torso&&Tail) {
   // Follow the torso's animated hip displacement while retaining the
   // assembled offset of the two meshes in the imported reference pose.
   const int Hips=Torso->GetBoneIndex(TEXT("hips"));
   const auto& Ref=Torso->GetSkeletalMeshAsset()->GetRefSkeleton();
   FTransform Rest=Ref.GetRefBonePose()[Hips];
   for(int Parent=Ref.GetParentIndex(Hips);Parent!=INDEX_NONE;Parent=Ref.GetParentIndex(Parent))
    Rest=Rest*Ref.GetRefBonePose()[Parent];
   const FVector HipDelta=Torso->GetBoneLocation(TEXT("hips"))-Torso->GetComponentTransform().TransformPosition(Rest.GetLocation());
   Tail->AddWorldOffset(HipDelta.GetClampedToMaxSize(Pawn_->GetSimpleCollisionRadius()*.33f));
   if(auto* Anim=Cast<UCultFluidAnimInstance>(Tail->GetAnimInstance())) {
    Anim->TailMotion=FMath::Clamp(Speed/FMath::Max(1.f,WalkSpeed_),0.f,1.f);
    Anim->TailPhase=GetWorld()->GetTimeSeconds()*6.f;
   }
  }
  if(Torso&&Weapons) if(auto* Anim=Cast<UCultFluidAnimInstance>(Weapons->GetAnimInstance())) {
   static const FName Hands[]={TEXT("lefthand"),TEXT("righthand"),TEXT("lefthand_001"),TEXT("righthand_001")};
   Anim->GripMask=0;
   for(int Index=0;Index<4;++Index) if(Torso->GetBoneIndex(Hands[Index])!=INDEX_NONE) {
    Anim->GripTargets[Index]=Weapons->GetComponentTransform().InverseTransformPosition(Torso->GetBoneLocation(Hands[Index]));
    Anim->GripMask|=1<<Index;
   }
  }
 }
}
