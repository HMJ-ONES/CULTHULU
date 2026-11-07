#include "CultKitRuntime.h"

#include "CultMatchDirector.h"
#include "CultMatchUnit.h"
#include "CultSkillAttack.h"
#include "CultUlhuPlayerPawn.h"
#include "CultWorldDirector.h"
#include "Components/CapsuleComponent.h"

bool FCultKitRuntime::LaunchPrimary() {
 using namespace CultPrimary;
 if(!Owner->HasAuthority() || !Primary.Ranged()) return false;

 const auto* Capsule = Owner->GetCapsuleComponent();
 const auto Aim = Owner->CameraAim();
 FVector Forward = Owner->AimRotation().Vector();
 if(Owner->IsBot()) Forward = (Aim.TraceEnd - Aim.TraceStart).GetSafeNormal();
 const FVector Origin = Owner->FeetLocation()
  + FVector(0,0,Capsule->GetScaledCapsuleHalfHeight()*1.43f)
  + Forward.GetSafeNormal2D()*Capsule->GetScaledCapsuleRadius()*.74f;
 FVector Direction = (Aim.ImpactPoint - Origin).GetSafeNormal();
 if(Direction.IsNearlyZero()) Direction = Forward;
 const FTransform Transform(Direction.Rotation(),Origin);
 auto* Shot = Owner->GetWorld()->SpawnActorDeferred<ACultSkillAttack>(
  ACultSkillAttack::StaticClass(),Transform,Owner,Owner,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 if(!Shot) return false;

 Shot->Direction = Direction;
 Shot->Windup = Primary.Windup;
 Shot->Reach = Primary.Reach;
 Shot->Speed = Primary.Speed;
 Shot->Radius = Primary.Radius;
 Shot->Duration = Primary.Reach/Primary.Speed + .35f;
 Shot->SignalStyle = uint8(Primary.Signal);
 Shot->bTrackMuzzle = true;
 Shot->PrimaryAim = Aim.ImpactPoint;
 const bool Corrosive = Primary.Delivery == EDelivery::CorrosiveArc;
 if(Corrosive) Shot->Gravity = 2750.f;

 const TWeakObjectPtr<ACultUlhuPlayerPawn> Source(Owner);
 const float Damage = Primary.Damage;
 Shot->Hit = [Source,Damage,Corrosive](AActor* Target,FVector Point) {
  auto* Pawn = Source.Get();
  if(!Pawn) return;
  const auto* Match = ACultMatchDirector::Find(Pawn->GetWorld());
  float Amount = Damage;
  if(auto* Hero = ::Cast<ACultUlhuPlayerPawn>(Target)) {
   if(!Match) Amount *= Hero->GetMaxHealth()/150.f;
  } else if(auto* Unit = ::Cast<ACultMatchUnit>(Target)) {
   Amount *= Unit->IsStructure() ? 1.3f : 2.0f;
  } else {
   Amount *= 5.f;
  }
  if(Match) Amount *= Match->DamageMultiplier(Pawn);
  const float Before = ACultSkillAttack::HealthOf(Target);
  ACultSkillAttack::Hurt(Target,Amount,Pawn,Point);
  if(Corrosive && ACultSkillAttack::HealthOf(Target)<Before) Pawn->ApplyPrimaryCorrosion(Target);
 };
 Shot->FinishSpawning(Transform);
 Owner->MarkCombat();
 if(auto* Director = ACultWorldDirector::Find(Owner->GetWorld())) Director->CombatActivity(Owner->GetFaction());
 Owner->MulticastMotionCue(3,0,false,Primary.Recovery*.85f);
 return true;
}

void FCultKitRuntime::ApplyCorrosion(AActor* Target) {
 if(!ACultSkillAttack::Enemy(Owner,Target)) return;
 if(auto* Unit = ::Cast<ACultMatchUnit>(Target); Unit && Unit->IsStructure()) return;
 auto& Effect = Corrosion.FindOrAdd(Target);
 Effect.Ticks = 3;
}

void FCultKitRuntime::TickCorrosion(float Delta) {
 for(auto It = Corrosion.CreateIterator(); It; ++It) {
  AActor* Target = It.Key().Get();
  if(!ACultSkillAttack::Enemy(Owner,Target)) {
   It.RemoveCurrent();
   continue;
  }
  auto& Effect = It.Value();
  Effect.UntilTick -= Delta;
  if(Effect.UntilTick > 0) continue;

  Effect.UntilTick += .75f;
  float Damage = 1.15f;
  if(auto* Hero = ::Cast<ACultUlhuPlayerPawn>(Target)) {
   if(!ACultMatchDirector::IsMatch(Owner->GetWorld())) Damage *= Hero->GetMaxHealth()/150.f;
  }
  ACultSkillAttack::Hurt(Target,Damage,Owner,Target->GetActorLocation());
  if(--Effect.Ticks == 0) It.RemoveCurrent();
 }
}
