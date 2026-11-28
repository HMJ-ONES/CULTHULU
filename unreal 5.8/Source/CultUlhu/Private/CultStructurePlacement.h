#pragma once
#include "CoreMinimal.h"
class UWorld;
class UStaticMesh;
class ACultDestructible;

namespace CultStructurePlacement {
 UStaticMesh* LoadMesh(const FString& Name);
 FTransform MakeTransform(const UStaticMesh* Mesh,FVector Ground,FVector Dimensions,float Yaw);
 bool CharactersOccupy(UWorld* World,const UStaticMesh* Mesh,const FTransform& Transform);
 ACultDestructible* Spawn(UWorld* World,const FString& Name,FVector Ground,FVector Dimensions,float Yaw,int32 Faction=-1,bool ProtectCharacters=false);
}
