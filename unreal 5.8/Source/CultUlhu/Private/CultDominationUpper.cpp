#include "CultDominationUpper.h"
#include "CultDominationRoute.h"
#include "CultMatchDirector.h"
#include "CultUlhuPlayerPawn.h"
#include "CultCanonicalKit.h"
#include "CultSkillAttack.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace {
float WeaponRange(const FString& Id) {
 cultulhu::CharacterDef D; if(!CultCanonicalKit::Load(Id,D)) return 0;
 float R=0; for(const auto* S:{&D.qAbility,&D.fAbility}) if(S->effectKind=="projectile") R=FMath::Max(R,S->range*100.f);
 if(D.rightClick.kind==cultulhu::HeavyAttackKind::AcidSpit) R=FMath::Max(R,D.rightClick.range*100.f);
 return R;
}
AActor* Reachable(ACultMatchDirector* M,ACultUlhuPlayerPawn* P,FVector Feet,float Range) {
 AActor* Best=nullptr; float Distance=Range;
 const float H=P->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()*2,Body=P->GetSimpleCollisionRadius();
 for(const auto& F:M->Fighters()) if(F.Team!=P->GetFaction()&&F.Respawn<=0&&F.Pawn.IsValid()&&F.Pawn->GetHealth()>0) {
  auto* Other=F.Pawn.Get(); const FVector Eye=Feet+FVector(0,0,H*.52f),D=(Other->GetActorLocation()-Eye).GetSafeNormal(),Origin=Eye+D*Body*.8f;
  FVector Point; const float Near=Other->GetCapsuleComponent()->GetClosestPointOnCollision(Origin,Point);
  if(Near>=0&&Near<=Distance&&ACultSkillAttack::Clear(P->GetWorld(),Origin,Point,P,Other)) { Best=Other; Distance=Near; }
 }
 return Best;
}
}
bool CultDominationUpper::Update(ACultMatchDirector* M,FCultMatchFighter& F,FCultDominationUpperMemory* All,int Slot,bool Rally,AActor*& Enemy,FVector& Goal,float& Stop) {
 using Phase=FCultDominationUpperMemory::EPhase;
 auto* P=F.Pawn.Get(); auto& S=All[Slot]; const float Now=M->Elapsed(); const FVector Feet=P->FeetLocation();
 if(S.Pawn.Get()!=P||S.Kit!=F.Kit||S.Deaths!=F.Deaths) { S=FCultDominationUpperMemory(); S.Pawn=P; S.Kit=F.Kit; S.Deaths=F.Deaths; S.Range=WeaponRange(F.Kit); }
 if(Feet.Z<-500) { S.Phase=Phase::Idle; S.NextTry=Now+10; return false; }
 int Capturers=0; bool Reserved=false;
 for(int I=0;I<M->Fighters().Num();++I) { const auto& A=M->Fighters()[I]; if(A.Team!=F.Team||A.Respawn>0||!A.Pawn.IsValid()||A.Pawn->GetHealth()<=0||I==Slot) continue;
  Capturers+=M->InsideCapture(A.Pawn->FeetLocation()); Reserved|=A.Pawn->IsBot()&&All[I].Phase!=Phase::Idle;
 }
 auto Site=[&]() { return FCultDominationRoute::Perch(S.High?-S.Home:S.Home,S.Side); };
 auto SetPath=[&](Phase Which,TArray<FVector> Path) { S.Phase=Which; S.Path=MoveTemp(Path); S.Index=0; S.Started=S.LastProgress=Now; S.BestDistance=BIG_NUMBER; };
 auto Exit=[&]() {
  TArray<FVector> Path;
  if(Feet.Z>4500) {
   if(F.Retreating) {
    if(FMath::Abs(Feet.Y)<10000) Path.Add(FVector(0,S.Side*12500,6000));
    Path.Add(FVector(S.Home*10500,S.Side*12500,6000)); Path.Add(FVector(S.Home*10500,S.Side*26500,0));
   }
   else Path={FVector(0,S.Side*12500,6000),FVector(0,S.Side*6800,6000),FVector(0,S.Side*5500,0)};
  } else if(Feet.Z>150&&FMath::Abs(Feet.Y)>16500) Path={FVector(S.Home*10500,S.Side*26500,0)};
  else if(Feet.Z>150) {
   if(S.High) Path={FVector(Site().X,S.Side*3300,0)}; // Leave the enemy-side perch toward the seal, not deeper into their spawn.
   else Path={FCultDominationRoute::PerchRamp(S.Home,S.Side)};
  }
  if(!Path.Num()) { S.Phase=Phase::Idle; S.NextTry=Now+12; }
  else SetPath(Phase::Exit,MoveTemp(Path));
 };
 if(S.Phase==Phase::Idle) {
  // A knocked-up or handed-over bot must descend instead of firing forever
  // from a platform that its authored weapons cannot reach the seal from.
  if(Feet.Z>4800&&Feet.Z<6500&&P->GetCharacterMovement()->IsMovingOnGround()&&FMath::Abs(Feet.Y)<17000&&FMath::Abs(Feet.X)<16000) {
   S.Side=Feet.Y>=0?1:-1; S.Home=F.Team?1:-1; Exit();
  } else {
   if(Now<S.NextTry||Rally||Reserved||Capturers==0||F.Retreating||F.DomPath.Num()||Slot%5==4||S.Range<1200||P->GetHealth()<P->GetMaxHealth()*.65f) return false;
   S.NextTry=Now+1;
   int Chosen=0; float Best=BIG_NUMBER; S.Home=F.Team?1:-1; S.High=false;
   for(int Side:{-1,1}) if(Reachable(M,P,FCultDominationRoute::Perch(S.Home,Side),S.Range)) { const float D=FVector::Dist2D(Feet,FCultDominationRoute::PerchRamp(S.Home,Side)); if(D<Best) { Best=D; Chosen=Side; } }
   // Never spend the long crossing on a target the direct approach already
   // reaches. Cross above the fight only for an otherwise unavailable angle.
   if(!Chosen&&Capturers>=2&&M->Score(F.Team)>=M->Score(1-F.Team)+25&&FVector::Dist2D(Feet,M->Base(F.Team))<6000) {
    for(int Side:{-1,1}) if(Reachable(M,P,FCultDominationRoute::Perch(-S.Home,Side),S.Range)) { const float D=FVector::Dist2D(Feet,FVector(S.Home*18500,Side*20500,0)); if(D<Best) { Best=D; Chosen=Side; S.High=true; } }
   }
   if(!Chosen) return false;
   S.Side=Chosen; S.LastUseful=Now; S.NextObserve=0;
   if(S.High) SetPath(Phase::Approach,{FVector(S.Home*18500,S.Side*20500,0),FVector(S.Home*10500,S.Side*26500,0),FVector(S.Home*10500,S.Side*12500,6000),FVector(0,S.Side*12500,6000),FVector(0,S.Side*6800,6000),Site()});
   else SetPath(Phase::Approach,{FCultDominationRoute::PerchRamp(S.Home,S.Side),FCultDominationRoute::Perch(S.Home,S.Side)});
  }
 }
 if(S.Phase==Phase::Idle) return false;
 if(S.Phase!=Phase::Exit&&Now>=S.NextObserve) {
  S.NextObserve=Now+.2f; S.Target=Reachable(M,P,S.Phase==Phase::Pressure?Feet:Site(),S.Range);
  if(S.Target.IsValid()) S.LastUseful=Now;
 }
 if(S.Phase!=Phase::Exit&&(F.Retreating||Rally||Capturers==0||Now-S.LastUseful>3||(S.Phase==Phase::Pressure&&Now-S.Started>8))) Exit();
 if(S.Phase==Phase::Idle) return false;
 if(S.Phase==Phase::Pressure) {
  Enemy=S.Target.Get(); Goal=Site()+FVector(FMath::Sin(Now*1.3f+Slot)*150,S.Side*FMath::Cos(Now)*90,0); Stop=100; return true;
 }
 while(S.Path.IsValidIndex(S.Index)&&FVector::Dist2D(Feet,S.Path[S.Index])<300&&FMath::Abs(Feet.Z-S.Path[S.Index].Z)<200) { ++S.Index; S.BestDistance=BIG_NUMBER; S.LastProgress=Now; }
 if(!S.Path.IsValidIndex(S.Index)) {
  if(S.Phase==Phase::Approach) { S.Phase=Phase::Pressure; S.Started=Now; S.NextObserve=0; Goal=Site(); Stop=100; return true; }
  S.Phase=Phase::Idle; S.NextTry=Now+12; return false;
 }
 const float D=FVector::Dist2D(Feet,S.Path[S.Index])+FMath::Abs(Feet.Z-S.Path[S.Index].Z)*.5f;
 if(D<S.BestDistance-100) { S.BestDistance=D; S.LastProgress=Now; }
 if(Now-S.LastProgress>6||Now-S.Started>(S.High?65:25)) {
  if(S.Phase==Phase::Exit) { S.Phase=Phase::Idle; S.NextTry=Now+15; return false; }
  Exit(); if(S.Phase==Phase::Idle) return false;
 }
 Goal=S.Path[S.Index]; Stop=120; if(S.Phase==Phase::Exit) Enemy=nullptr; return true;
}
