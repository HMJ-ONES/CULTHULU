#pragma once
class UWorld;
namespace CultDominationFinish {
 void Build(UWorld* World);
#if WITH_EDITOR
 int Compare(UWorld* World,bool Refined);
#endif
}
