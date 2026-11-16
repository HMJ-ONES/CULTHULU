#pragma once
#include "CoreMinimal.h"
class UWorld;
namespace CultMatchBackdrop {
void Build(UWorld* World,bool Moba);
#if WITH_EDITOR
void Compare(UWorld* World,bool Refined);
#endif
}
