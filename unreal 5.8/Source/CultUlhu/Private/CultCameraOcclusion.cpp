#include "CultUlhuPlayerController.h"
#include "CultUlhuPlayerPawn.h"
#include "CultCharacterModel.h"
#include "CultPopulation.h"
#include "CultMatchUnit.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/MeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"

void ACultUlhuPlayerController::UpdateHiddenComponents(const FVector& Eye,TSet<FPrimitiveComponentId>& HiddenComponents) {
 Super::UpdateHiddenComponents(Eye,HiddenComponents);
 const auto* ViewerPawn=Cast<ACultUlhuPlayerPawn>(GetPawn()); if(!ViewerPawn) return;
 const FVector Direction=(PlayerCameraManager?PlayerCameraManager->GetCameraRotation():GetControlRotation()).Vector();
 const float Radius=ViewerPawn->GetCapsuleComponent()->GetScaledCapsuleRadius();
 const float BodyPlane=FVector::DotProduct(ViewerPawn->GetActorLocation()-Eye,Direction);
 const float Reach=ViewerPawn->IsThirdPerson()?FMath::Max(Radius*1.5f,BodyPlane+Radius*.25f):Radius*1.5f;
 const FVector End=Eye+Direction*Reach;
 const float Margin=FMath::Max(20.f,Radius*.12f);
 auto HideMeshes=[&](AActor* Actor,bool Force) {
  TArray<UMeshComponent*> Meshes; Actor->GetComponents(Meshes);
  for(auto* Mesh:Meshes) {
   const FBox Box=Mesh->Bounds.GetBox().ExpandBy(Margin);
   if(Force||Box.IsInsideOrOn(Eye)||FMath::LineBoxIntersection(Box,Eye,End,End-Eye)) HiddenComponents.Add(Mesh->GetPrimitiveSceneId());
  }
 };
 for(TActorIterator<AActor> It(GetWorld());It;++It) {
  AActor* Actor=*It; bool Friendly=false;
  if(auto* Hero=Cast<ACultUlhuPlayerPawn>(Actor)) Friendly=Hero->GetFaction()==ViewerPawn->GetFaction();
  else if(auto* Person=Cast<ACultPopulation>(Actor)) Friendly=(Person->IsCultist()||Person->IsSummoned())&&Person->GetFaction()==ViewerPawn->GetFaction();
  else if(auto* Unit=Cast<ACultMatchUnit>(Actor)) Friendly=!Unit->IsStructure()&&Unit->Team==ViewerPawn->GetFaction();
  if(!Friendly||(Actor==ViewerPawn&&ViewerPawn->IsThirdPerson())) continue;
  // Long tails, wings and held weapons can reach the eye even when a hero root is farther away.
  if(!Actor->IsA<ACultUlhuPlayerPawn>()&&FVector::DistSquared(Actor->GetActorLocation(),Eye)>FMath::Square(Reach+Radius*5.f)) continue;
  const bool OwnFirstPerson=Actor==ViewerPawn&&!ViewerPawn->IsThirdPerson();
  HideMeshes(Actor,OwnFirstPerson);
  TArray<AActor*> Attached; Actor->GetAttachedActors(Attached);
  for(auto* Child:Attached) if(Child->IsA<ACultCharacterModel>()) HideMeshes(Child,OwnFirstPerson);
 }
 // This list belongs to one rendered view. Actor visibility, collision, aim traces,
 // enemy meshes and other players' views remain independent.
}
