#include "CultJungleCamp.h"
#include "Net/UnrealNetwork.h"
#include "CultCombatCredit.h"
#include "CultActorLabels.h"
#include "CultFeralBeast.h"
#include "CultMobaJungle.h"
#include "CultMatchDirector.h"
#include "CultMatchUnit.h"
#include "CultPopulation.h"
#include "CultUlhuPlayerPawn.h"
#include "CultSkillAttack.h"
#include "Components/CapsuleComponent.h"
#include "EngineUtils.h"

namespace {
ACultUlhuPlayerPawn* CreditHero(AActor* Source) {
 return CultCombatCredit::Hero(Source);
}
}
ACultJungleCamp::ACultJungleCamp() { bReplicates=true; bAlwaysRelevant=true; PrimaryActorTick.bCanEverTick=true; RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("CampOrigin")); }
ACultFeralBeast* ACultJungleCamp::Monster() const { return Monster_.Get(); }
ACultUlhuPlayerPawn* ACultJungleCamp::Target() const { return Target_.Get(); }
void ACultJungleCamp::BeginPlay() {
 Super::BeginPlay(); CampIndex=FMath::Clamp(CampIndex,0,3); SetActorLocation(FCultMobaJungle::Ground(CampIndex)); if(HasAuthority()) SpawnMonster();
}
void ACultJungleCamp::SpawnMonster() {
 const FVector Ground=FCultMobaJungle::Ground(CampIndex); FTransform Transform(Ground+FVector(0,0,1000));
 auto* B=GetWorld()->SpawnActorDeferred<ACultFeralBeast>(ACultFeralBeast::StaticClass(),Transform,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 B->SetSpecies(CampIndex>=2?TEXT("ossified_brute"):TEXT("skittering_ghoul")); B->FinishSpawning(Transform);
 B->JungleCamp_=this; B->BodyScale_*=4; B->SetActorScale3D(FVector(B->BodyScale_));
 B->MaxHealth_=B->Health_=CampIndex>=2?300.f:180.f; B->AttackDamage_=CampIndex>=2?28.f:20.f;
 B->SetActorLocation(Ground+FVector(0,0,B->Collision_->GetScaledCapsuleHalfHeight()+3)); B->Home_=B->GetActorLocation(); B->MoveTarget_=B->Home_;
 B->Tags.Add(TEXT("CultJungleMonster")); CultActorLabels::Set(B,Title());
 Monster_=B; Target_.Reset(); LastSource_.Reset(); RewardPaid_=Resetting_=false; Respawn_=ResetTime_=0;
}
void ACultJungleCamp::Tick(float Dt) {
 Super::Tick(Dt); if(!HasAuthority()) return; auto* M=ACultMatchDirector::Find(GetWorld()); if(!M||M->IsOver()||M->Warmup()>0) return;
 if(auto* B=Monster_.Get()) if(!B->IsDead()) return;
 if(Respawn_>0) { Respawn_=FMath::Max(0.f,Respawn_-Dt); if(Respawn_>0) return; }
 SpawnMonster();
}
bool ACultJungleCamp::DamageAllowed(AActor* Source) const {
 auto* M=ACultMatchDirector::Find(GetWorld()); if(!M||M->IsOver()||M->Warmup()>0||Resetting_) return false;
 if(auto* Hero=CreditHero(Source)) return M->Slot(Hero)>=0;
 if(auto* Unit=Cast<ACultMatchUnit>(Source)) return Unit->Health>0;
 return false;
}
bool ACultJungleCamp::Eligible(const ACultUlhuPlayerPawn* Hero,float Radius) const {
 auto* M=ACultMatchDirector::Find(GetWorld()); if(!Hero||!M||Hero->GetHealth()<=0||M->RespawnFor(Hero)>0||M->Slot(Hero)<0) return false;
 const FVector Feet=Hero->FeetLocation();
 return FVector::Dist2D(Feet,GetActorLocation())<Radius&&FMath::Abs(Feet.Z-GetActorLocation().Z)<800;
}
void ACultJungleCamp::Damaged(AActor* Source) {
 if(auto* Hero=CreditHero(Source)) { if(Eligible(Hero,3500)) Target_=Hero; else StartReset(); }
 // The hit which starts a leash can still be lethal (e.g. a projectile whose
 // owner has moved away). Keep its credit until death or reset completion.
 LastSource_=Source;
}
void ACultJungleCamp::CancelAttacks() {
 auto* B=Monster_.Get(); if(!B) return;
 for(TActorIterator<ACultSkillAttack> It(GetWorld());It;++It) if(It->GetOwner()==B) It->Destroy();
 B->AttackAnimTime_=0;
}
void ACultJungleCamp::StartReset() {
 if(Resetting_) return; Resetting_=true; ResetTime_=0; Target_.Reset(); LastSource_.Reset(); CancelAttacks();
 if(auto* B=Monster_.Get()) { B->Control_=FCultControlProjection(); B->StaggerTime_=0; B->AttackCooldown_=1; }
}
void ACultJungleCamp::MonsterDied(ACultFeralBeast* B) {
 if(Monster_.Get()!=B||RewardPaid_) return; RewardPaid_=true; Resetting_=false; Target_.Reset(); Respawn_=CampIndex>=2?90:60;
 auto* M=ACultMatchDirector::Find(GetWorld()); if(!M||M->IsOver()) return;
 auto* Hero=CreditHero(LastSource_.Get()); int Killer=M->Slot(Hero),Team=Hero?Hero->GetFaction():-1;
 if(auto* Unit=Cast<ACultMatchUnit>(LastSource_.Get())) Team=Unit->Team;
 if(Killer>=0) { M->GrantXP(Killer,RewardXP()); Hero->Notify(FString::Printf(TEXT("%s slain  +%d XP"),*Title(),RewardXP())); }
 for(int I=0;I<M->Fighters().Num();++I) {
  const auto& F=M->Fighters()[I]; if(I==Killer||F.Team!=Team||F.Respawn>0||!F.Pawn.IsValid()) continue;
  if(FVector::Dist2D(F.Pawn->FeetLocation(),GetActorLocation())<3500&&FMath::Abs(F.Pawn->FeetLocation().Z-GetActorLocation().Z)<800) M->GrantXP(I,RewardXP()/2);
 }
 LastSource_.Reset();
}
void ACultJungleCamp::TickMonster(ACultFeralBeast* B,float Dt) {
 auto* M=ACultMatchDirector::Find(GetWorld()); if(!M||M->IsOver()||M->Warmup()>0) { CancelAttacks(); B->AnimateCreature(Dt,0); return; }
 const FVector Before=B->GetActorLocation();
 B->AttackCooldown_=FMath::Max(0.f,B->AttackCooldown_-Dt); B->StaggerCooldown_=FMath::Max(0.f,B->StaggerCooldown_-Dt);
 B->PoiseReset_=FMath::Max(0.f,B->PoiseReset_-Dt); if(B->PoiseReset_<=0) B->PoiseDamage_=0;
 if(Resetting_) {
  ResetTime_+=Dt; B->Health_=FMath::Min(B->MaxHealth_,B->Health_+B->MaxHealth_*.25f*Dt);
  B->MoveToward(B->Home_,B->Speed_*1.5f,Dt);
  if(ResetTime_>6) B->SetActorLocation(B->Home_,false,nullptr,ETeleportType::TeleportPhysics);
  if(FVector::Dist2D(B->GetActorLocation(),B->Home_)<120&&FMath::Abs(B->GetActorLocation().Z-B->Home_.Z)<100) { B->Health_=B->MaxHealth_; Resetting_=false; LastSource_.Reset(); }
  B->AnimateCreature(Dt,FVector::Dist2D(Before,B->GetActorLocation())); return;
 }
 if(Target_.IsValid()&&(!Eligible(Target_.Get(),3500)||FVector::Dist2D(B->GetActorLocation(),B->Home_)>3200)) { StartReset(); return; }
 if(!Target_.IsValid()&&B->Health_<B->MaxHealth_) { StartReset(); return; }
 if(!Target_.IsValid()) {
  float Best=2600;
  for(const auto& F:M->Fighters()) if(Eligible(F.Pawn.Get(),2600)) {
   const float D=FVector::Dist2D(B->GetActorLocation(),F.Pawn->FeetLocation()); if(D<Best) { Best=D; Target_=F.Pawn; }
  }
 }
 if(B->StaggerTime_>0) { B->StaggerTime_=FMath::Max(0.f,B->StaggerTime_-Dt); B->AnimateCreature(Dt,0); return; }
 if(B->Control_.Stop||B->Control_.Stun||B->Control_.Fear) {
  CancelAttacks();
  if(B->Control_.Fear) B->MoveToward(Before+(Before-B->Control_.Origin).GetSafeNormal2D()*1000,B->Speed_,Dt);
  if(FVector::Dist2D(B->GetActorLocation(),B->Home_)>3200) StartReset();
  B->AnimateCreature(Dt,FVector::Dist2D(Before,B->GetActorLocation())); return;
 }
 if(B->AttackAnimTime_>0) { B->AnimateCreature(Dt,0); return; }
 if(B->RecoveryTime_>0) { B->RecoveryTime_=FMath::Max(0.f,B->RecoveryTime_-Dt); B->AnimateCreature(Dt,0); return; }
 if(auto* Hero=Target_.Get()) {
  const FVector Origin=B->GetActorLocation()-FVector(0,0,B->Collision_->GetScaledCapsuleHalfHeight())+FVector(0,0,100*B->BodyScale_);
  FVector Closest; const float Distance=Hero->GetCapsuleComponent()->GetClosestPointOnCollision(Origin,Closest);
  if(Distance>=0&&Distance<220*B->BodyScale_*.85f) {
   B->FaceToward(Hero->GetActorLocation(),Dt,12);
   if(B->AttackCooldown_<=0&&ACultSkillAttack::Clear(GetWorld(),Origin,Closest,B,Hero)) B->BeginSwipe(Hero->FeetLocation()+FVector(0,0,100*B->BodyScale_));
  } else B->MoveToward(Hero->FeetLocation(),B->Speed_,Dt);
 } else {
  const float Angle=GetWorld()->GetTimeSeconds()*.3f+CampIndex;
  B->MoveToward(B->Home_+FVector(FMath::Cos(Angle),FMath::Sin(Angle),0)*350,B->Speed_*.25f,Dt);
 }
 B->AnimateCreature(Dt,FVector::Dist2D(Before,B->GetActorLocation()));
}

void ACultJungleCamp::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const { Super::GetLifetimeReplicatedProps(OutLifetimeProps); DOREPLIFETIME(ACultJungleCamp,CampIndex); DOREPLIFETIME(ACultJungleCamp,Monster_); DOREPLIFETIME(ACultJungleCamp,Respawn_); DOREPLIFETIME(ACultJungleCamp,Resetting_); }
