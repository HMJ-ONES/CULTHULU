#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "CoreMinimal.h"
class UWorld;
class AActor;
class ACameraActor;
class APlayerController;
struct FCultMatchLightingProbe {
 bool Tick(UWorld* World,bool Host,int Mode);
 ~FCultMatchLightingProbe();
private:
 int View=0,Phase=0;
 float At=0;
 bool Done=false,ShowHUD=true;
 TWeakObjectPtr<APlayerController> Controller;
 TWeakObjectPtr<AActor> Original;
 TWeakObjectPtr<ACameraActor> Camera;
 TArray<TWeakObjectPtr<AActor>> Fixtures;
 FString Prefix;
 void Restore();
};
#endif
