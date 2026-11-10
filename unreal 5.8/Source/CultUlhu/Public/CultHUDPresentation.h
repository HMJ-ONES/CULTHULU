#pragma once
#include "CoreMinimal.h"
class ACultUlhuPlayerPawn;
class ACultMatchDirector;

// Local presentation belongs to each HUD, not to a shared server/match timer.
struct FCultHUDPresentationMemory {
 TWeakObjectPtr<ACultUlhuPlayerPawn> Pawn;
 float ActiveAt=-1,PendingAt=0,ShownAt=0;
 FString PendingKey,ShownKey;
};
struct FCultHUDPresentation {
 bool Guidance=true,Expanded=false,ShowHint=false,ShowWorldGoal=true;
 FString Hint;
 float HintOpacity=1;
};
namespace CultHUDPresentation {
 FCultHUDPresentation Observe(ACultUlhuPlayerPawn* Pawn,ACultMatchDirector* Match,FCultHUDPresentationMemory& Memory,float Now);
}
