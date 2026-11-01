#include "CultKitRuntime.h"
#include "CultMatchDirector.h"
#include "CultMatchUnit.h"
#include "CultUlhuPlayerPawn.h"
#include "CultSkillAttack.h"
#include "CultBlastFX.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"

FCultKitRuntime::~FCultKitRuntime() { CancelMinionThrow(); }
bool FCultKitRuntime::UsesMinionThrow() const {
 auto* M=ACultMatchDirector::Find(Owner->GetWorld()); return Def.id=="cthulhu_avatar"&&M&&M->Mode()==ECultPlayMode::Dotgoo;
}
FString FCultKitRuntime::MinionThrowHint() const {
 if(!UsesMinionThrow()) return FString();
 if(HeldMinion.IsValid()&&HeldMinion->IsHeldBy(Owner)) return TEXT("MINION HELD | Release RMB / LMB: throw");
 return HeavyCooldown>0?FString::Printf(TEXT("RMB MINION SLING  %.0fs"),FMath::CeilToFloat(HeavyCooldown)):TEXT("RMB: lift minion | Release: throw");
}
void FCultKitRuntime::MinionThrowInput(bool Press) {
 if(!Press) { if(HeldMinion.IsValid()&&HeldMinion->IsHeldBy(Owner)) { ThrowOnReady=true; if(HoldTime>=.3f) LaunchMinion(); } return; }
 if(HeldMinion.IsValid()&&HeldMinion->IsHeldBy(Owner)) return;
 if(HeavyCooldown>0) { Owner->Notify(FString::Printf(TEXT("Minion sling ready in %.0fs"),FMath::CeilToFloat(HeavyCooldown))); return; }
 auto* M=ACultMatchDirector::Find(Owner->GetWorld()); if(!M||!M->CanAct(Owner)||Owner->IsIncapacitated()) return;
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
 if(!Pick||!Pick->TryLift(Owner)) { Owner->Notify(TEXT("Aim at an enemy lane minion within 45 metres.")); return; }
 HeldMinion=Pick; HoldTime=0; ThrowOnReady=false; HeavyCooldown=20; Owner->MarkCombat(); M->OnAction(Owner);
 Owner->Notify(TEXT("Minion lifted. Release RMB or click LMB to launch. 20s cooldown."));
 ACultBlastFX::SpawnBlast(Owner->GetWorld(),ECultBlastKind::HitFlash,Pick->GetActorLocation(),1);
}
void FCultKitRuntime::TickMinionThrow(float Dt) {
 if(!HeldMinion.IsValid()) return;
 if(!HeldMinion->IsHeldBy(Owner)) { HeldMinion.Reset(); return; }
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
 Bolt->TargetFilter=[](AActor* A){ auto* U=::Cast<ACultMatchUnit>(A); return ::Cast<ACultUlhuPlayerPawn>(A)||(U&&U->IsStructure()); };
 TWeakObjectPtr<ACultMatchUnit> WeakUnit(Unit); TWeakObjectPtr<ACultUlhuPlayerPawn> WeakOwner(Owner); const float Scale=M->DamageMultiplier(Owner);
 Bolt->Hit=[WeakUnit,WeakOwner,Scale](AActor* Target,FVector Point) {
  if(!WeakUnit.IsValid()||!WeakOwner.IsValid()) return;
  WeakUnit->ConsumeThrown(WeakOwner.Get());
  ACultSkillAttack::Hurt(Target,(::Cast<ACultUlhuPlayerPawn>(Target)?20.f:40.f)*Scale,WeakOwner.Get(),Point);
 };
 Bolt->Finished=[WeakUnit](FVector End){ if(WeakUnit.IsValid()&&WeakUnit->IsInTransit()) WeakUnit->LandMiss(End); };
 Bolt->FinishSpawning(T); Bolt->SetActorHiddenInGame(true); Unit->StartFlight(Bolt);
 HeldMinion.Reset(); HoldTime=0; ThrowOnReady=false; Owner->MarkCombat(); M->OnAction(Owner);
}
void FCultKitRuntime::CancelMinionThrow() {
 if(HeldMinion.IsValid()&&HeldMinion->IsHeldBy(Owner)) HeldMinion->LandMiss(HeldMinion->GetActorLocation());
 HeldMinion.Reset(); HoldTime=0; ThrowOnReady=false;
}
