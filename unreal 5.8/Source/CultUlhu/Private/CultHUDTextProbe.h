#pragma once
#if WITH_DEV_AUTOMATION_TESTS
#include "CoreMinimal.h"
namespace CultHUDTextProbe {
 struct Sample { FString Text; FVector2D Position,Drawn,Measured,MeasuredAtDPI,Canvas; float Points=0,DPI=1; int32 CacheType=0; };
 inline bool Enabled=false;
 inline uint64 Frame=0;
 inline TArray<Sample> Samples;
 inline uint64 CaptureFrame=MAX_uint64;
 inline TArray<Sample> Captured;
}
#endif
