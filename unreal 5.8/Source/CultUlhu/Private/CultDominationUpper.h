#pragma once
#include "CoreMinimal.h"
class ACultMatchDirector;
class ACultUlhuPlayerPawn;
struct FCultMatchFighter;
struct FCultDominationUpperMemory {
 enum class EPhase { Idle,Approach,Pressure,Exit };
 TWeakObjectPtr<ACultUlhuPlayerPawn> Pawn;
 FString Kit;
 int Deaths=-1,Side=1,Home=-1,Index=0;
 float Range=0,NextTry=0,Started=0,LastProgress=0,BestDistance=BIG_NUMBER,LastUseful=0,NextObserve=0;
 bool High=false;
 EPhase Phase=EPhase::Idle;
 TArray<FVector> Path;
 TWeakObjectPtr<AActor> Target;
};
namespace CultDominationUpper {
 bool Update(ACultMatchDirector* Match,FCultMatchFighter& Fighter,FCultDominationUpperMemory* Memories,int Slot,bool Rally,AActor*& Enemy,FVector& Goal,float& Stop);
}
