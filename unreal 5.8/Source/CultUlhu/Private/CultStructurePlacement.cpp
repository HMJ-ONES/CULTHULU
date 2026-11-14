#include "CultStructurePlacement.h"
#include "CultDestructible.h"
#include "CultFeralBeast.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/Pawn.h"

UStaticMesh* CultStructurePlacement::LoadMesh(const FString& Name) {
 return LoadObject<UStaticMesh>(nullptr,*(TEXT("/Game/CultUlhu/World/")+Name+TEXT("/StaticMeshes/")+Name));
}
FTransform CultStructurePlacement::MakeTransform(const UStaticMesh* Mesh,FVector Ground,FVector Dimensions,float Yaw) {
 if(!Mesh) return FTransform::Identity;
 const FVector Size=Mesh->GetBoundingBox().GetSize();
 const FVector Scale(Dimensions.X/FMath::Max(1.,Size.X),Dimensions.Y/FMath::Max(1.,Size.Y),Dimensions.Z/FMath::Max(1.,Size.Z));
 Ground.Z-=Mesh->GetBoundingBox().Min.Z*Scale.Z;
 return FTransform(FRotator(0,Yaw,0),Ground,Scale);
}
bool CultStructurePlacement::CharactersOccupy(UWorld* World,const UStaticMesh* Mesh,const FTransform& Transform) {
 if(!World||!Mesh) return true;
 const auto Bounds=Mesh->GetBoundingBox();
 const FVector Extent=Bounds.GetExtent()*Transform.GetScale3D().GetAbs()+FVector(4);
 FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_Pawn); Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
 FCollisionQueryParams Query(SCENE_QUERY_STAT(CultStructureCharacters),false); TArray<FOverlapResult> Hits;
 World->OverlapMultiByObjectType(Hits,Transform.TransformPosition(Bounds.GetCenter()),Transform.GetRotation(),Objects,FCollisionShape::MakeBox(Extent),Query);
 for(const auto& Hit:Hits) {
  if(Cast<APawn>(Hit.GetActor())) return true;
  if(auto* Beast=Cast<ACultFeralBeast>(Hit.GetActor())) if(!Beast->IsDead()) return true;
 }
 return false;
}
ACultDestructible* CultStructurePlacement::Spawn(UWorld* World,const FString& Name,FVector Ground,FVector Dimensions,float Yaw,int32 Faction,bool ProtectCharacters) {
 auto* Mesh=LoadMesh(Name); if(!World||!Mesh) return nullptr;
 const auto Transform=MakeTransform(Mesh,Ground,Dimensions,Yaw);
 if(ProtectCharacters&&CharactersOccupy(World,Mesh,Transform)) return nullptr;
 auto* Building=World->SpawnActorDeferred<ACultDestructible>(ACultDestructible::StaticClass(),Transform,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 if(!Building) return nullptr;
 Building->SetMesh(Mesh); Building->Tags.Add(FName(*FString::Printf(TEXT("Faction%d"),Faction))); Building->FinishSpawning(Transform); Building->SetReplicates(true); Building->bAlwaysRelevant=true; Building->ForceNetUpdate(); return Building;
}
