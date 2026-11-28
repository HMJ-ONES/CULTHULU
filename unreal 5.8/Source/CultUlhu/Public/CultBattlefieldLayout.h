#pragma once
#include "CoreMinimal.h"
class UWorld;
namespace CultBattlefieldLayout {
 inline FVector2D HalfExtent(bool Moba) { return Moba?FVector2D(60000,34500):FVector2D(48000,45000); }
 inline float OuterPassageY(bool Moba) { return Moba?26500.f:35000.f; }
 void Build(UWorld* World,bool Moba);
}
