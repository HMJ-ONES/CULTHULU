#pragma once
#include "CoreMinimal.h"
class UWorld;
namespace CultDominationArt {
 void Build(UWorld* World);
 FString RouteHint(FVector Feet);
#if WITH_EDITOR
 void Compare(UWorld* World,bool Refined);
#endif
}
