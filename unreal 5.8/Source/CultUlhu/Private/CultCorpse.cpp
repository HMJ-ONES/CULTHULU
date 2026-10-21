#include "CultCorpse.h"
#include "CultUlhuPlayerPawn.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PoseableMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Engine/SkeletalMesh.h"
ACultCorpse::ACultCorpse() { PrimaryActorTick.bCanEverTick=false; RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Corpse")); Tags.Add(TEXT("Corpse")); }
ACultCorpse* ACultCorpse::Leave(AActor* Body) {
 if(!IsValid(Body)||Body->ActorHasTag(TEXT("CorpseRecorded"))) return nullptr; Body->Tags.Add(TEXT("CorpseRecorded"));
 auto* C=Body->GetWorld()->SpawnActor<ACultCorpse>(Body->GetActorLocation(),Body->GetActorRotation()); if(!C) return nullptr;
 TArray<UMeshComponent*> Parts; Body->GetComponents(Parts);
 for(auto* P:Parts) {
  // Hidden editor camera proxies can sit far behind a giant entity. Copying
  // them corrupts the resting bounds and exposes an editor helper as a corpse.
  if(P->IsEditorOnly()||P->ComponentHasTag(TEXT("VesselEye"))) continue;
  const bool BodyMesh=P->ComponentHasTag(TEXT("CultCorpseBody"));
  if(!BodyMesh&&(!P->IsVisible()||P->bHiddenInGame)) continue;
  UMeshComponent* Copy=nullptr;
  if(auto* S=Cast<UPoseableMeshComponent>(P)) { auto* N=NewObject<UPoseableMeshComponent>(C); N->SetSkinnedAssetAndUpdate(S->GetSkinnedAsset()); N->SetupAttachment(C->GetRootComponent()); N->RegisterComponent(); auto* Leader=Cast<UPoseableMeshComponent>(S->LeaderPoseComponent.Get()); N->BoneSpaceTransforms=(Leader?Leader:S)->GetBoneSpaceTransforms(); N->MarkRefreshTransformDirty(); N->RefreshBoneTransforms(); Copy=N; }
  else if(auto* StaticPart=Cast<UStaticMeshComponent>(P)) { auto* N=NewObject<UStaticMeshComponent>(C); N->SetStaticMesh(StaticPart->GetStaticMesh()); N->SetupAttachment(C->GetRootComponent()); N->RegisterComponent(); Copy=N; }
  if(!Copy) continue; Copy->SetWorldTransform(P->GetComponentTransform()); Copy->SetComponentTickEnabled(false); Copy->SetCollisionEnabled(ECollisionEnabled::NoCollision); for(int I=0;I<P->GetNumMaterials();++I) Copy->SetMaterial(I,P->GetMaterial(I));
 }
 FVector Floor=Body->GetActorLocation(); if(auto* Cap=Body->FindComponentByClass<UCapsuleComponent>()) Floor.Z-=Cap->GetScaledCapsuleHalfHeight();
 FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(CorpseGround),false,Body); Q.AddIgnoredActor(C); if(Body->GetWorld()->LineTraceSingleByObjectType(H,Floor+FVector(0,0,80),Floor-FVector(0,0,20000),FCollisionObjectQueryParams(ECC_WorldStatic),Q)) Floor=H.ImpactPoint;
 C->SetActorLocation(Floor); C->SetActorRotation(FRotator(90,Body->GetActorRotation().Yaw,0));
 // Ground the posed render bounds after rotation. A fixed offset scales badly
 // across civilians, jungle variants and giant entities, burying or floating art.
 FBox Bounds(ForceInit); TArray<UMeshComponent*> Copies; C->GetComponents(Copies);
 for(auto* Part:Copies) { Part->UpdateBounds(); Bounds+=Part->Bounds.GetBox(); }
 if(Bounds.IsValid) C->AddActorWorldOffset(FVector(0,0,Floor.Z+3-Bounds.Min.Z));
 return C;
}
