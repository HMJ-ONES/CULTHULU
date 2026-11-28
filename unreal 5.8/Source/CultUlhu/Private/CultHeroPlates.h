#pragma once
#include "CoreMinimal.h"
class ACultUlhuPlayerPawn;
class ACultMatchDirector;
class ACultPlayHUD;
class UCanvas;
struct FCultHeroPlate {
 TWeakObjectPtr<ACultUlhuPlayerPawn> Hero;
 FString Name;
 FBox2D Bounds;
 FVector2D Anchor;
 float Health=0,MaxHealth=1;
 bool Friendly=false,Focused=false;
};
namespace CultHeroPlates {
 void Draw(ACultPlayHUD* HUD,UCanvas* Canvas,ACultUlhuPlayerPawn* Player,ACultMatchDirector* Match,const FBox2D& Safe,const TArray<FBox2D>& Reserved);
#if WITH_DEV_AUTOMATION_TESTS
 inline bool Probe=false;
 inline bool ProbeReady=false;
 inline FVector2D ProbeRequest,ProbeSize;
 inline TArray<FCultHeroPlate> Captured;
 inline FBox2D CapturedSafe;
#endif
}
