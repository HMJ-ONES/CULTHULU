#pragma once
#include "CoreMinimal.h"
class ACultMatchUnit;
class UStaticMeshComponent;
namespace CultWardArchitecture {
 bool Enabled();
 UStaticMeshComponent* Create(ACultMatchUnit* Unit,bool Force=false);
 void Extinguish(ACultMatchUnit* Unit);
#if WITH_EDITOR
 void Compare(ACultMatchUnit* Unit,bool Refined);
#endif
}
