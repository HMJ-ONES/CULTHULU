#include "CultPopulation.h"
#include "ProfilingDebugging/CsvProfiler.h"
CSV_DECLARE_CATEGORY_EXTERN(CultRuntime);
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/Paths.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "CultHumanAnimation.h"

void ACultPopulation::RefreshAuthoredHuman()
{
 const FString Skin=bSummoned_?TEXT(""):bSorcerer_?TEXT("cultist_magus"):bCultist_?TEXT("cultist_hooded"):TEXT("civilian_villager");
 if(Skin==HumanSkin_) return; HumanSkin_=Skin;
 for(auto* Part:HumanPoseParts_) Part->DestroyComponent(); for(auto* Part:HumanAccessories_) Part->DestroyComponent();
 HumanPoseParts_.Reset(); HumanAccessories_.Reset(); HumanRestPose_.Reset(); HumanPose_=nullptr; if(Skin.IsEmpty()) return;
 const bool Mage=Skin==TEXT("cultist_magus"); const FString Prefix=Mage?TEXT("Mage_"):TEXT("Rogue_");
 const FString Folder=TEXT("/Game/CultUlhu/People/")+Skin+TEXT("/")+Skin;
 const TArray<FString> Pieces={TEXT("Body"),Skin==TEXT("cultist_hooded")?TEXT("Head_Hooded"):TEXT("Head"),TEXT("ArmLeft"),TEXT("ArmRight"),TEXT("LegLeft"),TEXT("LegRight")};
 FBox Bounds(ForceInit);
 for(const auto& Part:Pieces) {
  const FString Name=Prefix+Part; auto* Asset=LoadObject<USkeletalMesh>(nullptr,*(Folder+TEXT("/SkeletalMeshes/")+Name+TEXT(".")+Name)); if(!Asset) continue;
  auto* SkinPart=NewObject<UPoseableMeshComponent>(this); SkinPart->SetupAttachment(Appearance_); SkinPart->SetSkinnedAssetAndUpdate(Asset); SkinPart->SetCollisionEnabled(ECollisionEnabled::NoCollision); SkinPart->RegisterComponent(); HumanPoseParts_.Add(SkinPart); Bounds+=Asset->GetBounds().GetBox(); if(!HumanPose_) HumanPose_=SkinPart;
 }
 if(!HumanPose_) return;
 const float Scale=176.f/FMath::Max(1.f,Bounds.GetSize().Z); HumanPose_->SetRelativeScale3D(FVector(Scale)); HumanPose_->SetRelativeRotation(FRotator(0,-90,0)); HumanPose_->SetRelativeLocation(FVector(0,0,-88-Bounds.Min.Z*Scale));
 for(auto* SkinPart:HumanPoseParts_) if(SkinPart!=HumanPose_) { SkinPart->AttachToComponent(HumanPose_,FAttachmentTransformRules::SnapToTargetIncludingScale); SkinPart->SetLeaderPoseComponent(HumanPose_,true); }
 const TArray<FString> Accessories=Mage?TArray<FString>{TEXT("Mage_Cape"),TEXT("Mage_Hat"),TEXT("2H_Staff")}:TArray<FString>{TEXT("Rogue_Cape")};
 for(auto& Name:Accessories) if(auto* Asset=LoadObject<UStaticMesh>(nullptr,*(Folder+TEXT("/StaticMeshes/")+Name+TEXT(".")+Name),nullptr,LOAD_NoWarn)) {
  auto* SkinPart=NewObject<UStaticMeshComponent>(this); SkinPart->SetupAttachment(HumanPose_); SkinPart->SetStaticMesh(Asset); SkinPart->SetCollisionEnabled(ECollisionEnabled::NoCollision); SkinPart->RegisterComponent(); HumanAccessories_.Add(SkinPart);
 }
 HumanRestPose_=HumanPose_->GetBoneSpaceTransforms();
}
void ACultPopulation::AnimateAuthoredHuman(float Dt)
{
 CSV_SCOPED_TIMING_STAT(CultRuntime, PopulationAuthoredPose);
 if(!HumanPose_||HumanRestPose_.IsEmpty()) return;
 const float Now=GetWorld()->GetTimeSeconds(); float Rate=20;
 if(auto* PC=GetWorld()->GetFirstPlayerController()) if(PC->PlayerCameraManager) { const float Distance=FVector::Dist2D(PC->PlayerCameraManager->GetCameraLocation(),GetActorLocation()); Rate=Distance>70000?1:Distance>20000?4:20; }
 const float Phase=(GetUniqueID()%17)*.037f;
 if(FMath::FloorToInt((Now+Phase)*Rate)==FMath::FloorToInt((HumanPoseTime_+Phase)*Rate)) return;
 HumanPoseTime_=Now;
 FString Clip=TEXT("idle"); const bool Walking=GetVelocity().Size2D()>15;
 if(Walking) Clip=TEXT("walk");
 else if(Activity_==TEXT("Pray")||Activity_==TEXT("ChantingCircle")||Activity_==TEXT("OmenReading")||Activity_==TEXT("Necromancy")) Clip=TEXT("channel");
 else if(Activity_==TEXT("SacrificePerformer")) Clip=TEXT("sacrifice_performer");
 else if(Activity_==TEXT("SacrificeVictim")||bCaptive_) Clip=TEXT("sacrifice_victim");
 else if(Activity_==TEXT("Build")||Activity_==TEXT("Torture")||Activity_==TEXT("MendEffigy")) Clip=TEXT("work");
 else if(Activity_==TEXT("Preach")||Activity_==TEXT("Convert")||Activity_==TEXT("WhisperCampaign")) Clip=TEXT("cast");
 else if(Activity_==TEXT("Attack")||Activity_==TEXT("Raid")||Activity_==TEXT("WildsHunt")||(AttackTime_>.65f&&Activity_!=TEXT("Capture"))) Clip=TEXT("attack");
 if(Control_.Stun) Clip=TEXT("stunned"); if(Control_.Fear) Clip=TEXT("fearrun");
 const auto& Animation=CultHumanAnimation::Find(HumanSkin_,Clip); if(!Animation.hasTrackData()||Animation.durationSeconds<=0) return;
 HumanPose_->BoneSpaceTransforms=HumanRestPose_;
 const auto Pose=Animation.sampleAt(FMath::Fmod(HumanPoseTime_+GetUniqueID()*.031,Animation.durationSeconds));
 for(auto& Bone:Pose) {
  FString Name=UTF8_TO_TCHAR(Bone.first.c_str()); int Index=HumanPose_->GetBoneIndex(FName(*Name)); if(Index==INDEX_NONE) { Name.ReplaceInline(TEXT("."),TEXT("_")); Index=HumanPose_->GetBoneIndex(FName(*Name)); }
  if(!HumanRestPose_.IsValidIndex(Index)) continue; auto Transform=HumanRestPose_[Index]; const auto& Rot=Bone.second.rotEuler; const auto& P=Bone.second.pos;
  Transform.SetRotation((Transform.GetRotation()*FQuat::MakeFromEuler(FVector(Rot.x,Rot.y,Rot.z))).GetNormalized()); Transform.AddToTranslation(FVector(P.x,P.z,P.y)*100); HumanPose_->BoneSpaceTransforms[Index]=Transform;
 }
 HumanPose_->MarkRefreshTransformDirty(); HumanPose_->RefreshBoneTransforms();
}
