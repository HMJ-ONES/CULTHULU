#include "CultPopulation.h"
#include "ProfilingDebugging/CsvProfiler.h"
CSV_DECLARE_CATEGORY_EXTERN(CultRuntime);
#include "CultCorpse.h"
#include "CultSkillAttack.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "HAL/IConsoleManager.h"
#include "CultFeralBeast.h"
#include "CultUlhuPlayerPawn.h"
#include "CultWorldDirector.h"
#include "CultDestructible.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"

namespace {
TAutoConsoleVariable<float> DecisionInterval(TEXT("cult.Population.DecisionInterval"),.1f,TEXT("NPC decision refresh interval in seconds; zero retains per-frame decisions for profiling. Physics and control states always tick every frame."));
}

ACultPopulation::ACultPopulation()
{
 PrimaryActorTick.bCanEverTick=true;
 GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Pawn,ECR_Overlap);
 GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility,ECR_Block);
 Body_=CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body")); Appearance_=CreateDefaultSubobject<USceneComponent>(TEXT("HumanAppearance")); Appearance_->SetupAttachment(GetCapsuleComponent()); Body_->SetupAttachment(Appearance_); Body_->SetCollisionEnabled(ECollisionEnabled::NoCollision);
 GetCharacterMovement()->bRunPhysicsWithNoController=true; GetCharacterMovement()->bOrientRotationToMovement=true; bUseControllerRotationYaw=false;
}
void ACultPopulation::BeginPlay()
{
 Super::BeginPlay(); Home_=GetActorLocation(); OrderPoint_=Home_;
 Body_->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder"))); Body_->SetRelativeScale3D(FVector(.6,.6,1.7));
 if(auto* M=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_CivilianBody"))) Body_->SetMaterial(0,M);
 CreateHumanAppearance();
 GetCharacterMovement()->MaxWalkSpeed=250; AmbientTime_=FMath::FRandRange(2.f,6.f);
}
void ACultPopulation::Recruit() { RecruitTo(0); }
void ACultPopulation::RecruitTo(int32 Faction)
{
 if(bCultist_) return; bPlayerBattle_=false; bCultist_=true; DecisionWait_=0; bCaptive_=false; Faction_=Faction; Loyalty_=60; Health_=100;
 if(auto* M=LoadObject<UMaterialInterface>(nullptr,Faction==0?TEXT("/Game/CultUlhu/Materials/M_CultistBody"):TEXT("/Game/CultUlhu/Materials/M_PawnVessel"))) Body_->SetMaterial(0,M);
 if(auto* D=ACultWorldDirector::Find(GetWorld())) D->Register(this);
}
void ACultPopulation::ReceiveAttack(float Damage,int32 Attacker,bool Accounted,bool Feral) {
 if(Health_<=0||Damage<=0) return; if(!bCultist_&&!bSummoned_&&Damage<Health_) { Damage=Health_; Accounted=false; }
 const float Before=Health_; Health_-=Damage;
 if(auto* D=ACultWorldDirector::Find(GetWorld())) if(D->PersonHurt(this,Before,Damage,Attacker,Accounted,Feral)) return;
 if(Health_<=0) { ACultCorpse::Leave(this); Destroy(); }
 else if(Damage>0&&!bCaptive_&&Activity_!=TEXT("Attack")&&Activity_!=TEXT("Raid")) Order(TEXT("Patrol"),GetActorLocation(),nullptr,1);
}
void ACultPopulation::PlayerOrder(const FString& Action,FVector Point,AActor* Target,bool Battle)
{
 for(TActorIterator<ACultSkillAttack> It(GetWorld());It;++It) if(It->GetOwner()==this) It->Destroy();
 bPlayerBattle_=false; Order(Action,Point,Target,Battle?1.e9f:600.f,true); bPlayerBattle_=Battle;
}
void ACultPopulation::Order(const FString& Action,FVector Point,AActor* Target,float Duration,bool bDirective)
{
 if(bPlayerBattle_) return; // Only an explicit player reassignment releases a battle group.
 DecisionWait_=0;
 if(Activity_!=Action) { WorkProgress_=0; bWorking_=false; }
 bDirective_=bDirective; Activity_=Action; OrderPoint_=Point; OrderTarget_=Target; OrderTime_=Duration; bAttack_=Action==TEXT("Attack")||Action==TEXT("Raid")||Action==TEXT("WildsHunt");
}
void ACultPopulation::Tick(float Dt)
{
 CSV_SCOPED_TIMING_STAT(CultRuntime, PopulationTick);
 Super::Tick(Dt);
 UpdateHumanAppearance(Dt);
 Control_.Tick(Dt); AttackTime_=FMath::Max(0.f,AttackTime_-Dt); OrderTime_=FMath::Max(0.f,OrderTime_-Dt);
 GetCharacterMovement()->MaxWalkSpeed=250.f*Control_.Speed;
 if(bWasStopped_!=Control_.Stop) DecisionWait_=0; bWasStopped_=Control_.Stop;
 if(Control_.Stop) GetCharacterMovement()->StopMovementImmediately();
 if(Control_.Fear) for(TActorIterator<ACultSkillAttack> It(GetWorld());It;++It) if(It->GetOwner()==this) It->Destroy();
 if(Control_.Stun||bCaptive_) {
  DecisionWait_=0; DecisionElapsed_=0; DecisionMove_=FVector::ZeroVector;
  for(TActorIterator<ACultSkillAttack> It(GetWorld());It;++It) if(It->GetOwner()==this) It->Destroy();
  const bool CanFollow=!Control_.Stun&&!Control_.Stop&&bCaptive_&&Captor_.IsValid()&&FVector::Dist2D(GetActorLocation(),Captor_->GetActorLocation())>250;
  if(CanFollow) AddMovementInput((Captor_->GetActorLocation()-GetActorLocation()).GetSafeNormal2D(),1,true);
  else GetCharacterMovement()->StopMovementImmediately();
  return;
 }
 DecisionElapsed_+=Dt; DecisionWait_-=Dt;
 if(DecisionWait_<=0||Control_.Fear) {
  const float Elapsed=DecisionElapsed_; DecisionElapsed_=0;
  DecisionWait_=FMath::Max(0.f,DecisionInterval.GetValueOnGameThread());
  DecisionMove_=FVector::ZeroVector; UpdateDecision(Elapsed);
 }
 // Feed intent every frame; native collision, acceleration and animation stay smooth.
 if(!Control_.Stop&&!DecisionMove_.IsNearlyZero()) AddMovementInput(DecisionMove_,1,true);
}
void ACultPopulation::UpdateDecision(float Dt)
{
 CSV_SCOPED_TIMING_STAT(CultRuntime, PopulationDecision);
 CSV_CUSTOM_STAT(CultRuntime, PopulationDecisionCalls,1,ECsvCustomStatOp::Accumulate);
 FVector Target=OrderPoint_; float Stop=150;
 auto* Player=Cast<ACultUlhuPlayerPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(),0));
 auto* Director=ACultWorldDirector::Find(GetWorld());
 if(Control_.Fear) Target=GetActorLocation()+(GetActorLocation()-Control_.Origin).GetSafeNormal2D()*1000;
 else if(!bCultist_ && !bSummoned_)
 {
  bool Flee=false; for(TActorIterator<ACultUlhuPlayerPawn> It(GetWorld());It;++It) if(FVector::Dist2D(GetActorLocation(),It->GetActorLocation())<1200) { Target=GetActorLocation()+(GetActorLocation()-It->GetActorLocation()).GetSafeNormal2D()*1000; Flee=true; break; }
  if(!Flee) { AmbientTime_-=Dt; if(AmbientTime_<=0) { OrderPoint_=Home_+FVector(FMath::FRandRange(-2000.f,2000.f),FMath::FRandRange(-2000.f,2000.f),0); AmbientTime_=FMath::FRandRange(2.f,6.f); } Target=OrderPoint_; }
 }
 else
 {
  if(OrderTime_<=0) { Activity_=TEXT("Patrol"); bAttack_=false; AmbientTime_-=Dt; if(AmbientTime_<=0) { OrderPoint_=Home_+FVector(FMath::FRandRange(-2000.f,2000.f),FMath::FRandRange(-2000.f,2000.f),0); AmbientTime_=FMath::FRandRange(8.f,15.f); } Target=OrderPoint_; }
  else if((Activity_==TEXT("Follow")||Activity_==TEXT("Retreat")) && Player && Faction_==0) { Target=Player->GetActorLocation(); Stop=1800; }
  else if(OrderTarget_.IsValid()) Target=OrderTarget_->GetActorLocation();
  if(Activity_==TEXT("Torture")&&OrderTarget_.IsValid()&&FVector::Dist2D(GetActorLocation(),OrderTarget_->GetActorLocation())<=400) return;
  if(Activity_==TEXT("Rest")||Activity_==TEXT("SacrificePerformer")||Activity_==TEXT("SacrificeVictim")||Activity_==TEXT("Necromancy")) return;
  if(FVector::Dist2D(Target,GetActorLocation())<500 && (Activity_==TEXT("Pray")||Activity_==TEXT("OmenReading")||Activity_==TEXT("ChantingCircle")||Activity_==TEXT("DreamSharing")||Activity_==TEXT("MendEffigy"))) return;
  AActor* Enemy=nullptr; float Best=bAttack_?10000.f:1200.f;
  const FVector BattleCenter=OrderTarget_.IsValid()?OrderTarget_->GetActorLocation():OrderPoint_;
  if(OrderTarget_.IsValid() && (!Cast<ACultPopulation>(OrderTarget_.Get())||Cast<ACultPopulation>(OrderTarget_.Get())->GetFaction()!=Faction_) && (Activity_==TEXT("Capture") || Activity_==TEXT("Attack"))) { Enemy=OrderTarget_.Get(); Best=FVector::Dist2D(GetActorLocation(),Enemy->GetActorLocation()); }
  if(Activity_!=TEXT("Capture")&&Activity_!=TEXT("Retreat")) for(TActorIterator<ACultPopulation> It(GetWorld());It;++It) if(It->IsCultist()&&It->GetFaction()!=Faction_&&Director&&Director->AtWar(Faction_,It->GetFaction())) { float D=FVector::Dist2D(GetActorLocation(),It->GetActorLocation()); if(D<Best&&(!bPlayerBattle_||FVector::Dist2D(It->GetActorLocation(),BattleCenter)<6000)) { Best=D; Enemy=*It; } }
  if(Activity_!=TEXT("Capture")&&Activity_!=TEXT("Retreat")) for(TActorIterator<ACultFeralBeast> It(GetWorld());It;++It) if(!It->IsDead()&&!It->IsCaptured()) { float D=FVector::Dist2D(GetActorLocation(),It->GetActorLocation()); if(D<Best&&(!bPlayerBattle_||FVector::Dist2D(It->GetActorLocation(),BattleCenter)<6000)) { Best=D; Enemy=*It; } }
  if(Enemy) { Target=Enemy->GetActorLocation(); Stop=180; if(auto* Avatar=Cast<ACultUlhuPlayerPawn>(Enemy)) Stop=Avatar->GetCapsuleComponent()->GetScaledCapsuleRadius()+150; if(Best<Stop+120&&AttackTime_<=0) { const float Strike=Activity_==TEXT("Capture")?0.f:Director?Director->CultDamage(this,10):10; if(Activity_==TEXT("Capture")) { if(auto* P=Cast<ACultPopulation>(Enemy)) P->Capture(this,Faction_); else if(auto* Beast=Cast<ACultFeralBeast>(Enemy)) Beast->Capture(this,Faction_);
    for(TActorIterator<ACultDestructible> It(GetWorld());It;++It) if(It->ActorHasTag(TEXT("FaithAltar"))&&It->GetFaction()==Faction_&&!It->IsDestroyed()) { Order(TEXT("Escort"),It->GetActorLocation()+FVector(700,0,0),nullptr,600,true); break; } } else {
     const FVector Origin=GetActorLocation()+FVector(0,0,10); FTransform T(Origin); auto* A=GetWorld()->SpawnActorDeferred<ACultSkillAttack>(ACultSkillAttack::StaticClass(),T,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
     if(A) { A->Shape=ACultSkillAttack::EShape::Sweep; A->Direction=(Enemy->GetActorLocation()-Origin).GetSafeNormal(); A->Reach=300; A->Radius=30; A->Windup=.28f; A->Duration=.18f; TWeakObjectPtr<ACultPopulation> Weak(this); A->Hit=[Weak,Strike](AActor* Other,FVector P) { if(Weak.IsValid()&&Weak->GetHealth()>0) ACultSkillAttack::Hurt(Other,Cast<ACultUlhuPlayerPawn>(Other)?Strike*.2f:Strike,Weak.Get(),P); }; A->FinishSpawning(T); }
    } AttackTime_=1; } }
  if(Activity_==TEXT("Convert")&&!Enemy) {
   ACultPopulation* Candidate=nullptr; float Nearest=10000.f;
   for(TActorIterator<ACultPopulation> It(GetWorld());It;++It) if(!It->IsCultist()&&!It->IsSummoned()) { const float D=FVector::Dist2D(GetActorLocation(),It->GetActorLocation()); if(D<Nearest) { Nearest=D; Candidate=*It; } }
   if(Candidate) { Target=Candidate->GetActorLocation(); Stop=300; if(Nearest<600&&AttackTime_<=0) { if(Director) Director->TryConvert(Candidate,Faction_,this); AttackTime_=45; } }
  }

  if(Activity_==TEXT("Raid")&&!Enemy) {
   ACultDestructible* Quarry=nullptr; float Distance=1e9; FVector Surface;
   for(TActorIterator<ACultDestructible> It(GetWorld());It;++It) if(!It->IsDestroyed()&&It->GetFaction()!=Faction_&&It->ActorHasTag(TEXT("LivingCity"))&&FVector::Dist2D(It->GetActorLocation(),OrderPoint_)<10000) {
    const FVector Nearest=It->GetComponentsBoundingBox().GetClosestPointTo(GetActorLocation()); const float D=FVector::Dist2D(Nearest,GetActorLocation()); if(D<Distance) { Distance=D; Quarry=*It; Surface=Nearest; }
   }
   if(Quarry) { Target=Surface+(GetActorLocation()-Surface).GetSafeNormal2D()*100; Stop=65;
    if(Distance<280&&AttackTime_<=0) { Quarry->ApplyDamage(Director?Director->CultDamage(this,10):10,Surface,Faction_,this); AttackTime_=1; }
   }
  }
 }
 FVector Direction=Target-GetActorLocation(); Direction.Z=0;
 if(!Control_.Stop && Direction.Size2D()>Stop) { // Reject movement toward holes and cliffs.
  Direction=Direction.GetSafeNormal();
  FCollisionQueryParams Avoid(SCENE_QUERY_STAT(CultNpcAvoid),false,this); FHitResult Obstacle;
  for(float Turn:{0.f,45.f,-45.f,90.f,-90.f,135.f,-135.f}) {
   const FVector Trial=Direction.RotateAngleAxis(Turn,FVector::UpVector);
   if(!GetWorld()->SweepSingleByChannel(Obstacle,GetActorLocation(),GetActorLocation()+Trial*220,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeSphere(45),Avoid)) { Direction=Trial; break; }
  }
  const float Lookahead=FMath::Max(200.f,GetCharacterMovement()->MaxWalkSpeed*(FMath::Max(0.f,DecisionInterval.GetValueOnGameThread())+.15f));
  FVector Probe=GetActorLocation()+Direction.GetSafeNormal()*Lookahead; FHitResult H; FCollisionQueryParams Q(SCENE_QUERY_STAT(CultNpcGround),false,this);
  if(GetWorld()->LineTraceSingleByObjectType(H,Probe+FVector(0,0,250),Probe-FVector(0,0,400),FCollisionObjectQueryParams(ECC_WorldStatic),Q)&&H.ImpactNormal.Z>.65) DecisionMove_=Direction.GetSafeNormal();
 }
}

void ACultPopulation::DieWithCorpse() { ACultCorpse::Leave(this); Destroy(); }
