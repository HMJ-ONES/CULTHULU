#include "CultHUDPresentation.h"
#include "CultUlhuPlayerPawn.h"
#include "CultMatchDirector.h"
#include "CultDominationArt.h"
#include "CultJungleCamp.h"
#include "CultFeralBeast.h"
#include "EngineUtils.h"

FCultHUDPresentation CultHUDPresentation::Observe(ACultUlhuPlayerPawn* P,ACultMatchDirector* M,FCultHUDPresentationMemory& S,float Now) {
 FCultHUDPresentation R;
 if(S.Pawn.Get()!=P) { S=FCultHUDPresentationMemory(); S.Pawn=P; }
 if(!M) return R;
 if(M->Warmup()<=0&&S.ActiveAt<0) S.ActiveAt=Now;
 R.Expanded=P->StatsVisible()||M->IsPaused()||M->IsOver();
 R.Guidance=R.Expanded||M->Warmup()>0||S.ActiveAt<0||Now-S.ActiveAt<6;
 R.ShowWorldGoal=!R.Expanded&&(M->Mode()!=ECultPlayMode::Domination||!M->InsideCapture(P->FeetLocation()));
 auto* F=M->Fighter(P); if(!F) return R;
 bool Persistent=false; FString Key;
 if(F->Respawn>0) return R; // The dedicated reforming panel already shows this.
 if(M->Warmup()>0||F->Recall>0||F->Invoke>0) { R.Hint=M->Hint(P); Persistent=true; Key=TEXT("action"); }
 else if(M->Mode()==ECultPlayMode::Domination) { R.Hint=CultDominationArt::RouteHint(P->FeetLocation()); Key=R.Hint; }
 else for(TActorIterator<ACultJungleCamp> It(P->GetWorld());It;++It) {
  if(FVector::Dist2D(P->FeetLocation(),It->GetActorLocation())>=3500||FMath::Abs(P->FeetLocation().Z-It->GetActorLocation().Z)>=800) continue;
  Key=FString::Printf(TEXT("camp%d"),It->CampIndex); R.Hint=M->Hint(P); Persistent=It->Resetting()||It->Target()==P; break;
 }
 if(Key!=S.PendingKey) { S.PendingKey=Key; S.PendingAt=Now; }
 if(Key.IsEmpty()) { S.ShownKey.Reset(); return R; }
 if(!Persistent&&Now-S.PendingAt<.3f) return R; // Do not flash at route boundaries.
 if(S.ShownKey!=Key) { S.ShownKey=Key; S.ShownAt=Now; }
 const float Age=Now-S.ShownAt;
 R.HintOpacity=Persistent||R.Expanded?1.f:FMath::Min(FMath::Clamp(Age/.2f,0.f,1.f),FMath::Clamp((5.5f-Age)/1.f,0.f,1.f));
 R.ShowHint=!R.Hint.IsEmpty()&&R.HintOpacity>.02f&&!R.Expanded;
 return R;
}
