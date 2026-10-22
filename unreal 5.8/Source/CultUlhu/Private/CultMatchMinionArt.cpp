#include "CultMatchUnit.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/SkeletalMesh.h"
#include "Misc/Paths.h"
#include "CultHumanAnimation.h"
#include "CultHumanMesh.h"
#include <map>
void ACultMatchUnit::CreateArtBody() {
 Skin=Kind==1?TEXT("cultist_magus"):TEXT("cultist_hooded"); const bool Mage=Kind==1;
 FString Folder=TEXT("/Game/CultUlhu/People/")+Skin+TEXT("/")+Skin;
 FString Prefix=Mage?TEXT("Mage_"):TEXT("Rogue_"); FBox Bounds(ForceInit);
 if(auto* Combined=CultHumanMesh::Enabled()?CultHumanMesh::Find(Skin):nullptr) {
  auto* C=NewObject<UPoseableMeshComponent>(this); C->SetupAttachment(RootComponent); C->SetSkinnedAssetAndUpdate(Combined); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->RegisterComponent(); ArtParts.Add(C); Bounds=Combined->GetBounds().GetBox(); ArtBody=C;
 }
 else for(const TCHAR* Part:{TEXT("Body"),Mage?TEXT("Head"):TEXT("Head_Hooded"),TEXT("ArmLeft"),TEXT("ArmRight"),TEXT("LegLeft"),TEXT("LegRight")}) {
  FString Name=Prefix+Part; auto* Asset=LoadObject<USkeletalMesh>(nullptr,*(Folder+TEXT("/SkeletalMeshes/")+Name+TEXT(".")+Name)); if(!Asset) continue;
  auto* C=NewObject<UPoseableMeshComponent>(this); C->SetupAttachment(RootComponent); C->SetSkinnedAssetAndUpdate(Asset); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->RegisterComponent(); ArtParts.Add(C); Bounds+=Asset->GetBounds().GetBox(); if(!ArtBody) ArtBody=C;
 }
 if(!ArtBody) return;
 const float Scale=280.f/FMath::Max(1.f,Bounds.GetSize().Z); ArtBody->SetRelativeScale3D(FVector(Scale)); ArtBody->SetRelativeRotation(FRotator(0,-90,0)); ArtBody->SetRelativeLocation(FVector(0,0,-150-Bounds.Min.Z*Scale));
 for(auto* Part:ArtParts) if(Part!=ArtBody) { Part->AttachToComponent(ArtBody,FAttachmentTransformRules::SnapToTargetIncludingScale); Part->SetLeaderPoseComponent(ArtBody,true); }
 RestPose=ArtBody->GetBoneSpaceTransforms();
 for(auto* Part:Parts) Part->SetVisibility(false);
 // Allegiance remains legible without replacing authored character materials.
 auto* Ring=NewObject<UStaticMeshComponent>(this); Ring->SetupAttachment(RootComponent); Ring->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder"))); Ring->SetMaterial(0,Parts[0]->GetMaterial(0)); Ring->SetRelativeScale3D(FVector(2.4f,2.4f,.04f)); Ring->SetRelativeLocation(FVector(0,0,-146)); Ring->SetCollisionEnabled(ECollisionEnabled::NoCollision); Ring->SetCastShadow(false); Ring->RegisterComponent();
}
void ACultMatchUnit::AnimateArtBody(float Dt,bool Walking) {
 if(!ArtBody||RestPose.IsEmpty()) return; float Before=ArtTime; ArtTime+=Dt;
 if(FMath::FloorToInt(Before*15)==FMath::FloorToInt(ArtTime*15)) return;
 const FString Clip=Control.Stun?TEXT("stunned"):Walking?TEXT("walk"):AttackCooldown>.6f?TEXT("attack"):TEXT("idle");
 const auto& A=CultHumanAnimation::Find(Skin,Clip); if(!A.hasTrackData()||A.durationSeconds<=0) return;
 ArtBody->BoneSpaceTransforms=RestPose;
 auto Pose=A.sampleAt(FMath::Fmod(ArtTime+GetUniqueID()*.031,A.durationSeconds));
 for(auto& Bone:Pose) { FString Name=UTF8_TO_TCHAR(Bone.first.c_str()); int I=ArtBody->GetBoneIndex(FName(*Name)); if(I==INDEX_NONE) { Name.ReplaceInline(TEXT("."),TEXT("_")); I=ArtBody->GetBoneIndex(FName(*Name)); } if(!RestPose.IsValidIndex(I)) continue;
  auto T=RestPose[I]; auto& Rot=Bone.second.rotEuler; auto& P=Bone.second.pos; T.SetRotation((T.GetRotation()*FQuat::MakeFromEuler(FVector(Rot.x,Rot.y,Rot.z))).GetNormalized()); T.AddToTranslation(FVector(P.x,P.z,P.y)*100); ArtBody->BoneSpaceTransforms[I]=T;
 }
 ArtBody->MarkRefreshTransformDirty(); ArtBody->RefreshBoneTransforms();
}
