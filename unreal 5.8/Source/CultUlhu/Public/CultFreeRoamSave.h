#pragma once
#include "CoreMinimal.h"

class AActor;

// Versioned world snapshots, deliberately separate from transient combat FX.
// Slots live in Saved/SaveGames; progress never travels with the source repository.
namespace CultFreeRoamSave {
 bool Capture(UWorld* World,TArray<uint8>& Data,FString& Error);
 bool Restore(UWorld* World,const TArray<uint8>& Data,FString& Error);
 bool Inspect(const TArray<uint8>& Data,int32& Seed,FString& Character,TArray<int32>& Beliefs,FString& Error);
 bool Write(const TArray<uint8>& Data,FString& Error,const FString& Slot=TEXT("FreeRoam"));
 bool Read(TArray<uint8>& Data,FString& Error,const FString& Slot=TEXT("FreeRoam"));
 bool Exists(const FString& Slot=TEXT("FreeRoam"));
 void CaptureBody(AActor* Actor,TArray<uint8>& Data);
 void ApplyBody(AActor* Actor,const TArray<uint8>& Data);
}
