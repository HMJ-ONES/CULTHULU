#include "CultUlhuPlayerPawn.h"
#include "CultCombatAudio.h"
#include "CultSkillAttack.h"
#include "CultBlastFX.h"
#include "GameFramework/PlayerController.h"

void ACultUlhuPlayerPawn::RecordCombatHit(AActor* Target,FVector Point,float Damage,bool Killed)
{
 if(Damage>0) MarkCombat();
 if(IsBot()||Damage<=0) return;
 HitConfirmTime_=Killed?.28f:.15f;
 if(Killed) KillConfirmTime_=.45f;
 CombatFocus_=Target; CombatFocusTime_=2.5f;
 const uint32 Id=Target?Target->GetUniqueID():0;
 bool Combined=false;
 for(auto& R:ImpactTexts_) if(R.TargetId==Id&&R.Age<.09f) { R.Amount+=Damage; R.Killed|=Killed; Combined=true; break; }
 if(!Combined) { if(ImpactTexts_.Num()>=8) ImpactTexts_.RemoveAt(0); ImpactTexts_.Add({Point,Damage,0,Killed,Id}); }
 const float Now=GetWorld()->GetTimeSeconds();
 if(Now>=NextImpactSound_) { PlayCultCombatCue(GetWorld(),Killed?ECultCombatCue::Kill:ECultCombatCue::Hit); NextImpactSound_=Now+.065f; }
 AddCameraKick(Killed?.15f:.08f);
 ACultBlastFX::SpawnBlast(GetWorld(),ECultBlastKind::HitFlash,Point,Killed?.6f:.3f);
}
void ACultUlhuPlayerPawn::ConfirmEvade()
{
 if(IsBot()) return;
 if(EvadeConfirmTime_<=0) PlayCultCombatCue(GetWorld(),ECultCombatCue::Evade,.18f);
 EvadeConfirmTime_=.4f;
}
void ACultUlhuPlayerPawn::TickCombatFeel(float Dt)
{
 for(auto& R:ImpactTexts_) R.Age+=Dt;
 ImpactTexts_.RemoveAll([](const FCultImpactText& R){return R.Age>.85f;});
 KillConfirmTime_=FMath::Max(0.f,KillConfirmTime_-Dt); HurtFeedbackTime_=FMath::Max(0.f,HurtFeedbackTime_-Dt);
 EvadeConfirmTime_=FMath::Max(0.f,EvadeConfirmTime_-Dt); CombatFocusTime_=FMath::Max(0.f,CombatFocusTime_-Dt);
 if(Control_.Stun||Control_.Fear||IsRmbHeld()) { MeleeBuffer_=0; return; }
 if(auto* PC=Cast<APlayerController>(GetController())) if(PC->bShowMouseCursor) { MeleeBuffer_=0; return; }
 if(MeleeBuffer_>0) {
  // Consume at readiness before subtracting the frame, so an edge on a slow frame is not lost.
  if(SmashCooldown_<=0&&!IsSwinging()) { MeleeBuffer_=0; Smash(); }
  else MeleeBuffer_=FMath::Max(0.f,MeleeBuffer_-Dt);
 }
}
