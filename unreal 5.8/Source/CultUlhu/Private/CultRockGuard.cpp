#include "CultKitRuntime.h"
#include "CultAbilityFX.h"

#include "CultMatchDirector.h"
#include "CultSkillAttack.h"
#include "CultUlhuPlayerPawn.h"
#include "Components/BoxComponent.h"
#include "Components/CapsuleComponent.h"

namespace {
constexpr float RockCooldown=10.f;
void DropHeldRock(ACultSkillAttack* Rock) {
 Rock->Hit.Reset();
 Rock->Windup=0;
 Rock->Duration=1.45f;
 Rock->Reach=9000;
 Rock->Speed=450;
 Rock->Gravity=2300;
 Rock->PrimaryAim=Rock->GetActorLocation();
 Rock->ReleaseGuard(Rock->GetActorLocation()-FVector(0,0,1000));
}
}

bool FCultKitRuntime::RockInput(bool Press) {
 if(Def.id!="cthulhu_avatar") return false;
 if(Owner->HoldingRock()) {
  if(!Press) {
   DropHeldRock(Owner->ActiveRock_);
   Owner->ActiveRock_=nullptr;
   Owner->PlayAttackMotion(CultAttackMotion::EMove::Lower,.12f,.46f);
  }
  return true;
 }
 if(!Press) return false;
 const auto Aim=Owner->CameraAim();
 if(ACultSkillAttack::Alive(Aim.GetActor())) return false;
 auto* Match=ACultMatchDirector::Find(Owner->GetWorld());
 const bool Ground=Aim.bBlockingHit&&Aim.ImpactNormal.Z>.35f&&!ACultSkillAttack::Alive(Aim.GetActor());
 if(!Match&&!Ground) return false;
 if(HeavyCooldown>0||Owner->IsIncapacitated()) return true;

 const auto* Capsule=Owner->GetCapsuleComponent();
 const float HalfHeight=Capsule->GetScaledCapsuleHalfHeight();
 const FTransform Transform(Owner->GetActorLocation());
 auto* Rock=Owner->GetWorld()->SpawnActorDeferred<ACultSkillAttack>(
  ACultSkillAttack::StaticClass(),Transform,Owner,Owner,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 if(!Rock) return true;
 Rock->bRockGuard=true;
 Rock->RockExtents=FVector(HalfHeight*.22f,Capsule->GetScaledCapsuleRadius()*1.04f,HalfHeight*.55f);
 Rock->ImpactPower=16.f;
 Rock->SignalStyle=uint8(CultPrimary::ESignal::Stone);
 Rock->AbilityLook=uint8(CultAbilityFX::ELook::Stone);
 Rock->Windup=.10f;
 Rock->Radius=FMath::Min(float(Rock->RockExtents.Y),HalfHeight*.40f);
 Rock->Speed=10530;
 Rock->Reach=11000;
 Rock->Duration=Rock->Reach/Rock->Speed+.06f;
 const TWeakObjectPtr<ACultUlhuPlayerPawn> Source(Owner);
 Rock->Hit=[Source](AActor* Target,FVector Point) {
  if(auto* Pawn=Source.Get()) {
   float Damage=16.f;
   if(const auto* Match=ACultMatchDirector::Find(Pawn->GetWorld())) Damage*=Match->DamageMultiplier(Pawn);
   else if(auto* Hero=::Cast<ACultUlhuPlayerPawn>(Target)) Damage*=Hero->GetMaxHealth()/150.f;
   else Damage*=4.f;
   ACultSkillAttack::Hurt(Target,Damage,Pawn,Point);
  }
 };
 Rock->FinishSpawning(Transform);
 Owner->ActiveRock_=Rock;
 Owner->MarkCombat();
 Owner->PlayAttackMotion(CultAttackMotion::EMove::RockLift,.30f,.64f);
 return true;
}

void FCultKitRuntime::ReleaseRock() {
 auto* Rock=Owner->ActiveRock_.Get();
 if(Owner->IsIncapacitated()&&Owner->GetHealth()>0) return;
 Owner->ActiveRock_=nullptr;
 if(!IsValid(Rock)) return;
 if(Owner->GetHealth()<=0) {
  Rock->Destroy();
  return;
 }
 Rock->ReleaseGuard(Owner->CameraAim().ImpactPoint);
 HeavyCooldown=RockCooldown;
 if(!Owner->IsBot()&&!Owner->IsLocallyControlled()) Owner->ClientAbilityCommitted(3,HeavyCooldown);
 Owner->MarkCombat();
 Owner->PlayAttackMotion(CultAttackMotion::EMove::RockThrow,Rock->Windup,.64f);
}

void FCultKitRuntime::TickRock() {
 if(Owner->HoldingRock()||!Owner->HasAttackHold(CultAttackMotion::EMove::RockLift)) return;
 Owner->ActiveRock_=nullptr;
 HeavyCooldown=RockCooldown;
 if(!Owner->IsBot()&&!Owner->IsLocallyControlled()) Owner->ClientAbilityCommitted(3,HeavyCooldown);
 Owner->PlayAttackMotion(CultAttackMotion::EMove::Lower,.12f,.46f);
}

void ACultSkillAttack::DamageGuard(float Amount) {
 if(!HasAuthority()||!bRockGuard||IsActorBeingDestroyed()||Amount<=0) return;
 GuardIntegrity-=Amount;
 if(GuardIntegrity>0) return;
 EmitAbilityFX(uint8(CultAbilityFX::EEvent::Contact),GetActorLocation(),FVector::UpVector,RockExtents.GetMax());
 Destroy();
}

void ACultSkillAttack::OnRep_RockGuard() {
 if(bRockGuard&&RockExtents.X>0&&!GuardCollision) {
  GuardCollision=NewObject<UBoxComponent>(this);
  GuardCollision->SetupAttachment(GetRootComponent());
  GuardCollision->SetBoxExtent(RockExtents);
  GuardCollision->SetCollisionObjectType(ECC_WorldDynamic);
  GuardCollision->SetCollisionResponseToAllChannels(ECR_Ignore);
  GuardCollision->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
  GuardCollision->SetGenerateOverlapEvents(false);
  GuardCollision->SetCanEverAffectNavigation(false);
  GuardCollision->RegisterComponent();
 }
 if(GuardCollision) GuardCollision->SetCollisionEnabled(bRockGuard?ECollisionEnabled::QueryOnly:ECollisionEnabled::NoCollision);
 if(bRockGuard&&HasAuthority()) PositionGuard();
}

void ACultSkillAttack::PositionGuard() {
 const auto* Hero=Cast<ACultUlhuPlayerPawn>(GetOwner());
 if(!Hero) return;
 const auto* Capsule=Hero->GetCapsuleComponent();
 const FRotator Yaw(0,Hero->AimRotation().Yaw,0);
 const float Lift=FMath::SmoothStep(0.f,.30f,Age);
 const FVector Chest=Hero->GetActorLocation()+FVector(0,0,Capsule->GetScaledCapsuleHalfHeight()*FMath::Lerp(-.38f,.37f,Lift));
 const FVector Desired=Chest+Yaw.Vector()*(Capsule->GetScaledCapsuleRadius()+RockExtents.X+45.f);
 FCollisionQueryParams Query(SCENE_QUERY_STAT(CultRockLift),false,Hero);
 Query.AddIgnoredActor(this);
 FHitResult Obstruction;
 const bool Blocked=GetWorld()->SweepSingleByObjectType(Obstruction,Chest,Desired,Yaw.Quaternion(),
  FCollisionObjectQueryParams(ECC_WorldStatic),FCollisionShape::MakeBox(RockExtents),Query);
 SetActorLocationAndRotation(Blocked?Obstruction.Location:Desired,Yaw);
}

void ACultSkillAttack::ReleaseGuard(FVector Aim) {
 if(!HasAuthority()||!bRockGuard) return;
 bRockGuard=false;
 OnRep_RockGuard();
 Age=0;
 BornAt=GetWorld()->GetTimeSeconds();
 FirstTick=true;
 SetLifeSpan(Duration+Windup+.25f);
 Origin=Previous=GetActorLocation();
 Direction=(Aim-Origin).GetSafeNormal();
 if(Direction.IsNearlyZero()) Direction=GetActorForwardVector();
 if(Hit) SetActorRotation(Direction.Rotation());
 ForceNetUpdate();
}
