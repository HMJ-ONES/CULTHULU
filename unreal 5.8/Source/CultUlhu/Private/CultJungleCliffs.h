#pragma once
class UWorld;
class AActor;
namespace CultJungleCliffs {
 AActor* Build(UWorld* World,int Camp);
#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
 int LegacyComparison(UWorld* World,bool Legacy);
#endif
}
