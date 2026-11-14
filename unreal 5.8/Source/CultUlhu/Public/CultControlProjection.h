#pragma once
#include "CoreMinimal.h"
#include "Misc/App.h"

// Short-lived presentation snapshots of the core's effects, keyed by caster.
// One caster expiring an effect cannot clear another caster's active effect.
struct FCultControlProjection
{
 struct FSource { bool Stop=false,Fear=false,Stun=false; float Speed=1.f,Remaining=.15f; FVector Origin; };
 TMap<int32,FSource> Sources;
 bool Stop=false,Fear=false,Stun=false;
 float Speed=1.f;
 FVector Origin=FVector::ZeroVector;
 void Set(int32 Source,bool InStop,bool InFear,float InSpeed,FVector InOrigin,bool InStun)
 {
  auto& S=Sources.FindOrAdd(Source); S.Stop=InStop; S.Fear=InFear; S.Speed=InSpeed; S.Origin=InOrigin; S.Stun=InStun;
  S.Remaining=FMath::Max(.15f,float(FApp::GetDeltaTime())*2.5f);
 }
 void Tick(float Dt)
 {
  Stop=Fear=Stun=false; Speed=1.f;
  for(auto It=Sources.CreateIterator();It;++It) {
   auto& S=It.Value(); S.Remaining-=Dt; if(S.Remaining<=0) { It.RemoveCurrent(); continue; }
   Stop|=S.Stop; Fear|=S.Fear; Stun|=S.Stun; Speed=FMath::Min(Speed,S.Speed); if(S.Fear) Origin=S.Origin;
  }
 }
};
