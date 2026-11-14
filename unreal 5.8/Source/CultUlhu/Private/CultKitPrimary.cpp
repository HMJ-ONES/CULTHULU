#include "CultKitRuntime.h"

#include "CultAbilityFX.h"
#include "CultMatchDirector.h"
#include "CultMatchUnit.h"
#include "CultSkillAttack.h"
#include "CultUlhuPlayerPawn.h"
#include "CultWorldDirector.h"
#include "Components/CapsuleComponent.h"
#include "Engine/OverlapResult.h"

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
 Shot->Speed = Primary.Speed;
 Shot->Radius = Primary.Radius;
 Shot->ImpactPower = Primary.Damage;
 Shot->ConfigureProjectileFlight(Primary.Reach,Primary.Delivery==EDelivery::Cannon);
 Shot->SignalStyle = uint8(Primary.Signal);
 Shot->bTrackMuzzle = true;
 Shot->PrimaryAim = Aim.ImpactPoint;
 const bool Corrosive = Primary.Delivery == EDelivery::CorrosiveArc;
 if(Corrosive) Shot->Gravity = 2750.f;

 const TWeakObjectPtr<ACultUlhuPlayerPawn> Source(Owner);
 const TWeakObjectPtr<ACultSkillAttack> Projectile(Shot);
 const float Damage = Primary.Damage;
 const auto HitTarget = [Source,Damage,Corrosive](AActor* Target,FVector Point,float Scale) {
  auto* Pawn = Source.Get();
  if(!Pawn) return;
  const auto* Match = ACultMatchDirector::Find(Pawn->GetWorld());
  float Amount = Damage*Scale;
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
 Shot->Hit = [Projectile,HitTarget](AActor* Target,FVector Point) {
  HitTarget(Target,Point,Projectile.IsValid()?Projectile->ProjectileDamageScale(Point):1.f);
 };
 if(Primary.BlastRadius>0) {
  const float Radius=Primary.BlastRadius;
  Shot->Impact=[Source,Projectile,HitTarget,Radius](const FHitResult& Hit) {
   auto* Pawn=Source.Get();
   if(!Pawn) return;
   auto* World=Pawn->GetWorld();
   const FVector Point=Hit.bStartPenetrating&&Projectile.IsValid()?Projectile->ProjectilePosition():FVector(Hit.ImpactPoint);
   const bool Body=::Cast<ACultUlhuPlayerPawn>(Hit.GetActor())!=nullptr;
   const FVector Origin=Point+Hit.ImpactNormal*(Body?110.f:12.f);
   const float ShotScale=Projectile.IsValid()?Projectile->ProjectileDamageScale(Point):1.f;
   Pawn->EmitAbilityBurst(uint8(CultAbilityFX::ELook::Spore),uint8(CultAbilityFX::EEvent::Release),Origin,Hit.ImpactNormal,Radius);
   TArray<FOverlapResult> Overlaps;
   FCollisionQueryParams Query(SCENE_QUERY_STAT(SporeBlast),false,Pawn);
   World->OverlapMultiByObjectType(Overlaps,Point,FQuat::Identity,
    FCollisionObjectQueryParams(FCollisionObjectQueryParams::AllObjects),FCollisionShape::MakeSphere(Radius),Query);
   TSet<AActor*> Checked;
   for(const auto& Overlap:Overlaps) {
    auto* Target=Overlap.GetActor();
    if(Target==Hit.GetActor()||Checked.Contains(Target)||!ACultSkillAttack::Enemy(Pawn,Target)) continue;
    Checked.Add(Target);
    FVector Contact=Target->GetComponentsBoundingBox(true).GetClosestPointTo(Point);
    if(auto* Collision=::Cast<UPrimitiveComponent>(Target->GetRootComponent())) {
     FVector Closest;
     if(Collision->GetClosestPointOnCollision(Point,Closest)>0) Contact=Closest;
    }
    const float Distance=FVector::Dist(Point,Contact);
    if(Distance>Radius||!ACultSkillAttack::Clear(World,Origin,Contact,Pawn,Target)) continue;
    HitTarget(Target,Contact,ShotScale*FMath::Lerp(.65f,.25f,Distance/Radius));
   }
  };
 }
 Shot->FinishSpawning(Transform);
 Owner->MarkCombat();
 if(auto* Director = ACultWorldDirector::Find(Owner->GetWorld())) Director->CombatActivity(Owner->GetFaction());
 Owner->PlayAttackMotion(CultAttackMotion::EMove::Primary,Shot->Windup,Primary.Recovery*.92f);
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
