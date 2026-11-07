#include "CultKitRuntime.h"
#include "CultAbilityFX.h"
#include "CultMatchDirector.h"
#include "CultMatchUnit.h"
#include "CultUlhuPlayerPawn.h"
#include "CultSkillAttack.h"
#include "CultBlastFX.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"

FCultKitRuntime::~FCultKitRuntime() {
 CancelMinionThrow();
 if(Owner->HasAuthority()&&Owner->HoldingRock()) Owner->ActiveRock_->Destroy();
}
bool FCultKitRuntime::UsesMinionThrow() const {
 auto* M=ACultMatchDirector::Find(Owner->GetWorld()); return Def.id=="cthulhu_avatar"&&M&&M->Mode()==ECultPlayMode::Dotgoo;
}
FString FCultKitRuntime::MinionThrowHint() const {
 if(Def.id!="cthulhu_avatar") return FString();
 if(Owner->HoldingRock()) return TEXT("ROCK GUARD  /  LMB throw  /  Release RMB drop");
 if(HeldMinion.IsValid()&&HeldMinion->IsHeldBy(Owner)) return TEXT("MINION HELD | Release RMB / LMB: throw");
 if(HeavyCooldown>0) return FString::Printf(TEXT("RMB  %.0fs"),FMath::CeilToFloat(HeavyCooldown));
 return UsesMinionThrow()?TEXT("RMB: lift minion or raise rock"):TEXT("RMB toward ground: raise rock");
}
bool FCultKitRuntime::MinionThrowInput(bool Press) {
 if(HeldMinion.IsValid()&&HeldMinion->IsHeldBy(Owner)) {
  if(!Press) { ThrowOnReady=true; if(HoldTime>=.3f) LaunchMinion(); }
  return true;
 }
 if(!Press||HeavyCooldown>0||Owner->HoldingRock()) return false;
 auto* M=ACultMatchDirector::Find(Owner->GetWorld()); if(!M||!M->CanAct(Owner)||Owner->IsIncapacitated()) return false;
 const FHitResult Aim=Owner->CameraAim(); const FVector Ray=(Aim.TraceEnd-Aim.TraceStart).GetSafeNormal();
 ACultMatchUnit* Pick=nullptr; float Best=200.f;
 for(TActorIterator<ACultMatchUnit> It(Owner->GetWorld());It;++It) {
  if(It->IsStructure()||It->Health<=0||It->IsInTransit()||It->Team==Owner->GetFaction()||FVector::Dist2D(It->GetActorLocation(),Owner->FeetLocation())>4500+Owner->GetSimpleCollisionRadius()) continue;
  const float Along=FVector::DotProduct(It->GetActorLocation()-Aim.TraceStart,Ray);
  if(Along<0||(Aim.bBlockingHit&&Along>FVector::Dist(Aim.TraceStart,Aim.ImpactPoint)+400)) continue;
  const FVector Near=Aim.TraceStart+Ray*Along; FVector Closest;
  auto* Cap=It->FindComponentByClass<UCapsuleComponent>(); if(!Cap||Cap->GetClosestPointOnCollision(Near,Closest)<0) continue;
  const float Off=FVector::Dist(Near,Closest);
  if((Off<Best||Aim.GetActor()==*It)&&ACultSkillAttack::Clear(Owner->GetWorld(),Owner->GetActorLocation(),It->GetActorLocation(),Owner,*It)) { Best=Off; Pick=*It; }
 }
 if(!Pick||!Pick->TryLift(Owner)) return false;
 HeldMinion=Pick; HoldTime=0; ThrowOnReady=false; HeavyCooldown=20; Owner->MarkCombat(); M->OnAction(Owner);
 Owner->PlayAttackMotion(CultAttackMotion::EMove::MinionLift,.30f,.65f);
 Owner->Notify(TEXT("Minion lifted. Release RMB or click LMB to launch. 20s cooldown."));
 ACultBlastFX::SpawnBlast(Owner->GetWorld(),ECultBlastKind::HitFlash,Pick->GetActorLocation(),1);
 return true;
}
void FCultKitRuntime::TickMinionThrow(float Dt) {
 if(!HeldMinion.IsValid()||!HeldMinion->IsHeldBy(Owner)) {
  if(Owner->HasAttackHold(CultAttackMotion::EMove::MinionLift)) Owner->PlayAttackMotion(CultAttackMotion::EMove::Lower,.11f,.43f);
  HeldMinion.Reset();
  return;
 }
 if(Owner->IsIncapacitated()||Owner->GetHealth()<=0) { CancelMinionThrow(); return; }
 HoldTime+=Dt; if((ThrowOnReady&&HoldTime>=.3f)||HoldTime>=4) LaunchMinion();
}
void FCultKitRuntime::LaunchMinion() {
 auto* Unit=HeldMinion.Get(); auto* M=ACultMatchDirector::Find(Owner->GetWorld());
 if(!Unit||!Unit->IsHeldBy(Owner)||Owner->IsIncapacitated()||!M||!M->CanAct(Owner)) { CancelMinionThrow(); return; }
 const FVector Origin=Unit->GetActorLocation(); const FVector Aim=Owner->CameraAim().ImpactPoint;
 const FTransform T(Origin); auto* Bolt=Owner->GetWorld()->SpawnActorDeferred<ACultSkillAttack>(ACultSkillAttack::StaticClass(),T,Owner,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 if(!Bolt) { CancelMinionThrow(); return; }
 Bolt->Shape=ACultSkillAttack::EShape::Bolt; Bolt->Direction=(Aim-Origin).GetSafeNormal(); if(Bolt->Direction.IsNearlyZero()) Bolt->Direction=Owner->AimRotation().Vector();
 Bolt->Speed=6000; Bolt->Reach=8000; Bolt->Radius=100; Bolt->Windup=.12f; Bolt->Duration=1.5f;
 Bolt->AbilityLook=uint8(CultAbilityFX::ELook::Tendrils);
 Bolt->TargetFilter=[](AActor* A){ auto* U=::Cast<ACultMatchUnit>(A); return ::Cast<ACultUlhuPlayerPawn>(A)||(U&&U->IsStructure()); };
 TWeakObjectPtr<ACultMatchUnit> WeakUnit(Unit); TWeakObjectPtr<ACultUlhuPlayerPawn> WeakOwner(Owner); const float Scale=M->DamageMultiplier(Owner);
 Bolt->Hit=[WeakUnit,WeakOwner,Scale](AActor* Target,FVector Point) {
  if(!WeakUnit.IsValid()||!WeakOwner.IsValid()) return;
  WeakUnit->ConsumeThrown(WeakOwner.Get());
  ACultSkillAttack::Hurt(Target,(::Cast<ACultUlhuPlayerPawn>(Target)?13.f:28.f)*Scale,WeakOwner.Get(),Point);
 };
 Bolt->Finished=[WeakUnit](FVector End){ if(WeakUnit.IsValid()&&WeakUnit->IsInTransit()) WeakUnit->LandMiss(End); };
 Bolt->FinishSpawning(T); Bolt->SetActorHiddenInGame(true); Unit->StartFlight(Bolt);
 Owner->PlayAttackMotion(CultAttackMotion::EMove::MinionThrow,Bolt->Windup,.59f);
 HeldMinion.Reset(); HoldTime=0; ThrowOnReady=false; Owner->MarkCombat(); M->OnAction(Owner);
}
void FCultKitRuntime::CancelMinionThrow() {
 if(HeldMinion.IsValid()&&HeldMinion->IsHeldBy(Owner)) HeldMinion->LandMiss(HeldMinion->GetActorLocation());
 HeldMinion.Reset(); HoldTime=0; ThrowOnReady=false;
}
