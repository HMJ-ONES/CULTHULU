#include "CultMatchDirector.h"
#include "CultCombatCredit.h"
#include "CultMatchState.h"
#include "CultMobaJungle.h"
#include "CultDominationRoute.h"
#include "CultJungleCamp.h"
#include "CultFeralBeast.h"
#include "CultMatchUnit.h"
#include "CultUlhuPlayerPawn.h"
#include "CultSkillAttack.h"
#include "CultPopulation.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "discovery/RelicNames.h"

ACultMatchUnit* ACultMatchDirector::AddUnit(int Team,int Lane,int Kind,int Index,FVector Position,float Hp,float Dps,float Range,float Speed) {
 float Half=Kind==4?1600:Kind>=3?1250:Kind==2?240:150; FTransform T(Position+FVector(0,0,Half+3));
 auto* U=GetWorld()->SpawnActorDeferred<ACultMatchUnit>(ACultMatchUnit::StaticClass(),T,this,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
 U->Team=Team; U->Lane=Lane; U->Kind=Kind; U->Index=Index; U->Health=Hp; U->Dps=Dps; U->Range=Range; U->Speed=Speed; U->FinishSpawning(T); State_->Units.Add(U); return U;
}
void ACultMatchDirector::SpawnWaves() {
 if(!State_->Moba) return; State_->Moba->Wave(++State_->Wave);
 int Live[2]={0,0}; for(auto& Weak:State_->Units) if(Weak.IsValid()&&!Weak->IsStructure()&&Weak->Health>0) ++Live[Weak->Team];
 int N[2][3]={};
 for(const auto& M:State_->Moba->minions()) {
  if(Live[M.team]>=cultulhu::MobaDefense::MINION_CAP_PER_TEAM) continue;
  const int K=M.kind=="siege"?2:M.kind=="ranged"?1:0;
  FVector P=State_->Moba->Waypoint(M.team,M.lane,0)+FVector(0,M.lane*500-500,0);
  P.X+=(M.team==0?1:-1)*(1000+(N[M.team][M.lane]++)*220);
  auto* Unit=AddUnit(M.team,M.lane,K,-1,P,M.hp,M.dps,M.range*200,M.speed*200); Unit->Waypoint=FMath::Min(FCultMobaRoute::Segments,FMath::CeilToInt(FCultMobaRoute::Project(M.team,M.lane,P).Progress*FCultMobaRoute::Segments)); ++Live[M.team];
 }
 if(State_->Wave==1||State_->Wave%3==0) Announce(State_->Wave%3==0?TEXT("Siege wave deployed on every lane. Escort it to enemy towers."):TEXT("First waves deployed. Clear enemy acolytes and escort your own."));
}
bool ACultMatchDirector::UnitProtected(const ACultMatchUnit* U) const {
 if(!State_||!State_->Moba) return false;
 return U->Kind==4?!CoreOpen(U->Team):U->Kind==5?!LaneWardsCleared(U->Team):U->Kind==3?State_->Moba->TowerShielded(U->Index):false;
}
float ACultMatchDirector::HurtUnit(ACultMatchUnit* U,float Damage,AActor* Source) {
 if(!State_||State_->Over||State_->Warmup>0||!U||U->Health<=0) return U?U->Health:0;
 if(U->IsInTransit()) return U->Health;
 int Team=-1; if(auto* H=Cast<ACultUlhuPlayerPawn>(Source)) Team=H->GetFaction(); if(auto* M=Cast<ACultMatchUnit>(Source)) Team=M->Team; if(auto* C=Cast<ACultPopulation>(Source)) Team=C->GetFaction(); if(Team==U->Team) return U->Health;
 if(UnitProtected(U)) { if(auto* P=Cast<ACultUlhuPlayerPawn>(Source)) P->Notify(U->Kind==4?TEXT("God protected: destroy all six lane wards, then the nexus ward."):U->Kind==5?TEXT("Nexus protected: all six lane wards must fall first."):TEXT("Inner tower protected: destroy its outer ward first.")); return U->Health; }
 float Hp=FMath::Max(0.f,U->Health-Damage); const uint64 Attacker=Source?Source->GetUniqueID():0;
 if(U->Kind==3) Hp=State_->Moba->HitTower(U->Index,Damage,Attacker);
 if(U->Kind==4) Hp=State_->Moba->HitCore(U->Team,Damage,Attacker);
 if(Hp<=0&&U->Health>0) {
  const int Killer=Slot(CultCombatCredit::Hero(Source));
  if(U->Kind==3) { Announce(FString::Printf(TEXT("%s %s tower destroyed%s"),U->Team==0?TEXT("Allied"):TEXT("Enemy"),U->Lane==0?TEXT("top"):U->Lane==1?TEXT("mid"):TEXT("bottom"),LaneWardsCleared(U->Team)?TEXT(" | Nexus ward exposed!"):TEXT(""))); for(int I=0;I<State_->Fighters.Num();++I) if(State_->Fighters[I].Team!=U->Team) GrantXP(I,I==Killer?800:200); }
  else if(U->Kind==5) { State_->NexusDestroyed[U->Team]=true; Announce(U->Team==0?TEXT("Our nexus ward has fallen. Protect our god!"):TEXT("Enemy nexus ward destroyed. Their god is exposed!")); for(int I=0;I<State_->Fighters.Num();++I) if(State_->Fighters[I].Team!=U->Team) GrantXP(I,I==Killer?800:200); }
  else if(U->Kind==4) { U->Health=0; Finish(1-U->Team); }
  else for(int I=0;I<State_->Fighters.Num();++I) { auto& F=State_->Fighters[I]; if(F.Team==U->Team||F.Respawn>0||!F.Pawn.IsValid()) continue; if(I==Killer||FVector::Dist2D(F.Pawn->FeetLocation(),U->GetActorLocation())<6000) GrantXP(I,I==Killer?60:30); }
 }
 return Hp;
}
void ACultMatchDirector::UpdateUnits(float Dt) {
 State_->Units.RemoveAll([](const TWeakObjectPtr<ACultMatchUnit>& U){ return !U.IsValid(); });
 for(auto& Weak:State_->Units) {
  if(!Weak.IsValid()||Weak->Health<=0) continue; auto* U=Weak.Get(); if(U->Kind==4||U->IsInTransit()) continue;
  if(U->IsStructure()) { if(auto* Target=SelectTowerTarget(U)) U->FireAt(Target); continue; }
  AActor* Enemy=nullptr; float Best=U->Range;
  // Minions take tower fire first, allowing players to escort a wave into a siege.
  for(auto& Other:State_->Units) if(Other.IsValid()&&Other->Team!=U->Team&&Other->Health>0&&!Other->IsInTransit()&&(!U->IsStructure()||!Other->IsStructure())&&!UnitProtected(Other.Get())) {
   if(!U->IsStructure()&&Other->Kind==3&&Other->Lane!=U->Lane) continue;
   float Dist=FVector::Dist2D(U->GetActorLocation(),Other->GetActorLocation());
   if(auto* Cap=Other->FindComponentByClass<UCapsuleComponent>()) { FVector Closest; float Near=Cap->GetClosestPointOnCollision(U->GetActorLocation(),Closest); if(Near>=0) Dist=Near; }
   if(Dist<Best) { Enemy=Other.Get(); Best=Dist; }
  }
  for(auto& F:State_->Fighters) if(F.Team!=U->Team&&F.Pawn.IsValid()&&F.Respawn<=0&&F.Pawn->GetHealth()>0) {
   FVector Closest; float Dist=F.Pawn->GetCapsuleComponent()->GetClosestPointOnCollision(U->GetActorLocation(),Closest); if(Dist<0) Dist=1.e9f;
   if(Dist<U->Range&&(!Enemy||(!U->IsStructure()&&Dist<Best))) { Enemy=F.Pawn.Get(); Best=Dist; }
  }
  if(Enemy) { U->FireAt(Enemy); continue; }
  if(!U->IsStructure()&&State_->Moba) {
   if(U->RejoiningLane) {
    if(FVector::Dist2D(U->GetActorLocation(),U->RejoinPoint)>180) { U->MoveTo(U->RejoinPoint,Dt); continue; }
    if(U->RejoinPath.Num()>0) U->RejoinPath.RemoveAt(0);
    if(U->RejoinPath.Num()>0) { U->RejoinPoint=U->RejoinPath[0]; continue; }
    U->RejoiningLane=false;
   }
   FVector Target=State_->Moba->Waypoint(U->Team,U->Lane,U->Waypoint);
   if(FVector::Dist2D(U->GetActorLocation(),Target)<200) { U->Waypoint=FMath::Min(FCultMobaRoute::Segments,U->Waypoint+1); Target=State_->Moba->Waypoint(U->Team,U->Lane,U->Waypoint); }
   if(U->Waypoint==FCultMobaRoute::Segments&&LaneWardsCleared(1-U->Team)&&NexusAlive(1-U->Team)) Target=Base(1-U->Team)+FVector(U->Team?4000:-4000,0,0);
   U->MoveTo(Target,Dt);
  }
 }
 // Kit-summoned population uses the same teams and fights real lane targets, then expires.
 for(TActorIterator<ACultPopulation> It(GetWorld());It;++It) if(It->IsSummoned()) {
  if(It->GetLifeSpan()==0) It->SetLifeSpan(30);
  AActor* Target=nullptr; float Best=6000;
  for(auto& U:State_->Units) if(U.IsValid()&&U->Team!=It->GetFaction()&&U->Health>0&&!UnitProtected(U.Get())) { float D=FVector::Dist2D(It->GetActorLocation(),U->GetActorLocation()); if(D<Best) { Best=D; Target=U.Get(); } }
  if(Target) It->Order(TEXT("Attack"),Target->GetActorLocation(),Target,30,true);
 }
}
void ACultMatchDirector::BotThink(int I,float Dt) {
 auto& F=State_->Fighters[I]; auto* P=F.Pawn.Get(); if(!P||!CanAct(P)||F.Invoke>0||F.Recall>0) return;
 if(State_->Moba&&F.Level>=10&&F.Invokes>0&&State_->Time-State_->LastBotInvoke[I]>=120&&P->GetHealth()>P->GetMaxHealth()*.8f&&FVector::Dist2D(P->FeetLocation(),Base(F.Team))<3400) { State_->LastBotInvoke[I]=State_->Time; Interact(P); return; }
 if(P->GetHealth()<P->GetMaxHealth()*.25f) F.Retreating=true;
 if(P->GetHealth()>P->GetMaxHealth()*.85f) F.Retreating=false;
 if(!State_->Moba&&InsideCapture(P->FeetLocation())) F.PointSeconds+=Dt;
 // Each bot retains its lane and physically visits its lane entrance after spawning or retreating.
 const FVector Entry=State_->Moba?FCultMobaRoute::Point(F.Team,F.Lane,.2f):FVector::ZeroVector;
 if(State_->Moba&&F.RouteStep==1&&FVector::Dist2D(P->FeetLocation(),Entry)<1800) { F.RouteStep=2; F.LaneEntered=true; }
 FVector Goal=State_->Moba?FCultMobaRoute::Follow(F.Team,F.Lane,P->FeetLocation(),F.RouteStep==1?.2f:F.RouteStep==2?.8f:1.f):FVector(0,(I%5-2)*900,0);
 if(!State_->Moba&&!F.Retreating) {
  const float Side=I%2?1.f:-1.f;
  if(F.RouteStep==1&&FMath::Abs(P->FeetLocation().X)>11000&&I%5!=0) Goal=FVector(F.Team?10500:-10500,Side*6500,0);
  else F.RouteStep=2;
 }
 AActor* Enemy=nullptr; float Best=5500;
 for(auto& Other:State_->Fighters) if(Other.Team!=F.Team&&Other.Pawn.IsValid()&&Other.Respawn<=0&&Other.Pawn->GetHealth()>0) {
  const FVector At=Other.Pawn->FeetLocation();
  if(State_->Moba&&FCultMobaRoute::Project(F.Team,F.Lane,At).Distance>4200&&FVector::Dist2D(At,Base(F.Team))>6000) continue;
  if(!State_->Moba&&FVector::Dist2D(P->FeetLocation(),Objective())<3200&&FVector::Dist2D(At,Objective())>4800) continue;
  float Dist=FVector::Dist2D(P->FeetLocation(),At);
  if(Dist<Best&&ACultSkillAttack::Clear(GetWorld(),P->GetActorLocation(),Other.Pawn->GetActorLocation(),P,Other.Pawn.Get())) { Best=Dist; Enemy=Other.Pawn.Get(); }
 }
 if(State_->Moba) {
  if(!Enemy) {
   float Threat=5000;
   for(auto& W:State_->Units) if(W.IsValid()&&W->Team!=F.Team&&W->Health>0&&!W->IsInTransit()&&!UnitProtected(W.Get())) {
    if(W->Kind==3&&W->Lane!=F.Lane) continue;
    if(W->Kind==4&&!CoreOpen(W->Team)) continue;
    float D=FVector::Dist2D(P->FeetLocation(),W->GetActorLocation());
    if(D<Threat) { Threat=D; Best=D; Enemy=W.Get(); }
   }
  }
  if(F.RouteStep>=2) {
   ACultMatchUnit* Escort=nullptr; float Progress=-1.e9f;
   for(auto& W:State_->Units) if(W.IsValid()&&W->Health>0&&!W->IsInTransit()&&W->Team==F.Team&&W->Lane==F.Lane&&!W->IsStructure()) {
    float Forward=FCultMobaRoute::Project(F.Team,F.Lane,W->GetActorLocation()).Progress;
    if(Forward>Progress) { Progress=Forward; Escort=W.Get(); }
   }
   if(Escort) Goal=FCultMobaRoute::Follow(F.Team,F.Lane,P->FeetLocation(),FMath::Max(.2f,Progress-.012f));
   else Goal=FCultMobaRoute::Follow(F.Team,F.Lane,P->FeetLocation(),.5f); // Regroup on the assigned lane until the next wave.
   if(LaneWardsCleared(1-F.Team)&&FVector::Dist2D(P->FeetLocation(),FCultMobaRoute::Point(F.Team,F.Lane,.8f))<4500) F.RouteStep=3;
   if(F.RouteStep==3) Goal=FVector::Dist2D(P->FeetLocation(),Base(1-F.Team))<6500?Base(1-F.Team)+(NexusAlive(1-F.Team)?FVector(F.Team?4000:-4000,0,0):FVector::ZeroVector):FCultMobaRoute::Follow(F.Team,F.Lane,P->FeetLocation(),1.f);
  }
  if(auto* Tower=Cast<ACultMatchUnit>(Enemy)) if(Tower->Kind==3||Tower->Kind==5) {
   bool Cover=false; for(auto& W:State_->Units) if(W.IsValid()&&W->Health>0&&!W->IsStructure()&&W->Team==F.Team&&W->Lane==F.Lane&&FVector::Dist2D(W->GetActorLocation(),Tower->GetActorLocation())<Tower->Range) Cover=true;
   if(!Cover) { Enemy=nullptr; Goal=Tower->GetActorLocation()+FVector(F.Team?5000:-5000,0,0); }
  }
 }
 if(State_->Moba&&!Enemy&&!F.Retreating&&F.RouteStep>=2) for(TActorIterator<ACultJungleCamp> It(GetWorld());It;++It) {
  auto* B=It->Monster(); if(!B||B->IsDead()||It->Resetting()||FCultMobaJungle::PlateauAt(P->FeetLocation())!=It->CampIndex) continue;
  if(It->Target()!=P&&(I%5>=2||F.Level<10)) continue;
  const float D=FVector::Dist2D(P->FeetLocation(),B->GetActorLocation());
  if(D<Best&&ACultSkillAttack::Clear(GetWorld(),P->GetActorLocation(),B->GetActorLocation(),P,B)) { Best=D; Enemy=B; }
 }
 if(State_->PingTime>0&&F.Team==0&&I>0&&I<3) { Goal=State_->PingPoint; if(Enemy&&FVector::Dist2D(Enemy->GetActorLocation(),Goal)>6000) Enemy=nullptr; }
 if(F.Retreating) { Enemy=nullptr; Goal=State_->Moba&&FVector::Dist2D(P->FeetLocation(),Base(F.Team))>6500?FCultMobaRoute::Follow(F.Team,F.Lane,P->FeetLocation(),0.f):Base(F.Team)+FVector(F.Team?-2600:2600,((I%5)-2)*400,0); F.RouteStep=1; }
 if(Enemy) {
  if(State_->Moba) Goal=Enemy->GetActorLocation();
  else if(InsideCapture(P->FeetLocation())) Goal=Enemy->GetActorLocation().GetClampedToMaxSize2D(2400); // Fight from the seal instead of abandoning it for a chase.
  P->SetBotAim(Enemy);
 } else P->SetBotAim(nullptr);
 if(State_->Moba&&F.RouteStep==1&&!F.Retreating) Goal=FCultMobaRoute::Follow(F.Team,F.Lane,P->FeetLocation(),.2f); // Nearby combat must not cancel reaching the assigned route.
 if(State_->Moba&&!Enemy) {
  const int Camp=FCultMobaJungle::PlateauAt(P->FeetLocation());
  if(Camp>=0) Goal=FCultMobaJungle::Exit(Camp,F.Lane,P->FeetLocation());
  if(!F.Retreating&&F.RouteStep>=2&&I%5<2) for(int RIndex=0;RIndex<State_->Relics.Num();++RIndex) {
   const auto& R=State_->Relics[RIndex];
   if(R.Claim>=0||State_->Time<R.Spawn||F.Level<(R.Tier==1?10:R.Tier==2?20:30)||FVector::Dist2D(P->FeetLocation(),R.Point)>8500) continue;
   Goal=RIndex<4?FCultMobaJungle::Approach(RIndex,F.Lane,P->FeetLocation()):R.Point; break;
  }
 }
 if(!State_->Moba) {
  const FVector Feet=P->FeetLocation();
  if(Feet.Z< -1500) F.DomTunnelVisited=true;
  // Retreating underground must use an exit; a direct line to spawn hits the roof.
  if(Feet.Z< -500&&(F.DomPath.IsEmpty()||(F.Retreating&&!F.DomEscape))) {
   F.DomPath=FCultDominationRoute::Escape(Feet,F.Team,Base(F.Team)+FVector(F.Team?-2600:2600,0,0)); F.DomPathIndex=0; F.DomEscape=true;
  } else if(F.Retreating&&Feet.Z>=-500&&!F.DomEscape) { F.DomPath.Reset(); F.DomPathIndex=0; }
  // Arrival is a navigation fact even while the bot is fighting. The seal
  // is an area objective; entering it completes the final approach waypoint.
  while(F.DomPath.IsValidIndex(F.DomPathIndex)) {
   const FVector Target=F.DomPath[F.DomPathIndex];
   const bool AtWaypoint=FVector::Dist2D(Feet,Target)<550&&FMath::Abs(Feet.Z-Target.Z)<450;
   const bool AtSeal=!F.DomEscape&&F.DomPathIndex==F.DomPath.Num()-1&&InsideCapture(Feet);
   if(!AtWaypoint&&!AtSeal) break;
   ++F.DomPathIndex;
  }
  if(F.DomPath.IsValidIndex(F.DomPathIndex)) Goal=F.DomPath[F.DomPathIndex];
  else if(!F.DomPath.IsEmpty()) { F.DomFlankCompleted|=!F.DomEscape; F.DomPath.Reset(); F.DomPathIndex=0; F.DomEscape=false; }

 }
 float Stop=Cast<ACultFeralBeast>(Enemy)?1100:Enemy&&State_->Moba&&F.RouteStep>=2?1800:400;
 CultBotThreat::Respond(P,State_->BotThreats[I],State_->Time,I);
 if(CultBotThreat::Avoiding(State_->BotThreats[I],State_->Time)) { Goal=P->FeetLocation()+State_->BotThreats[I].Escape*1600; Stop=0; }
 if(FVector::Dist2D(Goal,P->FeetLocation())>Stop) P->MoveMatchToward(Goal,Dt);
 F.Think-=Dt; if(F.Think>0) return; F.Think=.28f+(I%3)*.04f;
 const int HealSlot=P->HealingAbilitySlot();
 if(HealSlot>=0) {
  ACultUlhuPlayerPawn* Patient=nullptr; float Need=.85f;
  for(auto& Ally:State_->Fighters) if(Ally.Team==F.Team&&Ally.Respawn<=0&&Ally.Pawn.IsValid()) {
   auto* A=Ally.Pawn.Get(); FVector Closest; float Near=A==P?0:A->GetCapsuleComponent()->GetClosestPointOnCollision(P->GetActorLocation(),Closest);
   float Ratio=A->GetHealth()/A->GetMaxHealth();
   if(Ratio<Need&&Near>=0&&Near<P->HealingRange()&&(A==P||ACultSkillAttack::Clear(GetWorld(),P->GetActorLocation(),A->GetActorLocation(),P,A))) { Need=Ratio; Patient=A; }
  }
  if(Patient) { P->SetBotAim(Patient); if(HealSlot==0) P->AbilityQ(); else if(HealSlot==1) P->AbilityF(); else P->AbilityR(); P->SetBotAim(Enemy); }
 }
 if(Enemy&&!F.Retreating) {
  if(Best<3300) { if(HealSlot!=0) P->AbilityQ(); if(Best<2400&&HealSlot!=1) P->AbilityF(); if(Best<3500&&HealSlot!=2) P->AbilityR(); }
  if(Best<3000) P->Smash();
  if(Best<2700&&I%2) { P->HeavyAttack(); P->ReleaseHeavy(); }
 }
}
void ACultMatchDirector::UpdateRelics(float Dt) {
 for(auto& R:State_->Relics) {
  if(R.Name.EndsWith(TEXT("relic"))) R.Name=UTF8_TO_TCHAR(cultulhu::generateRelicName(State_->Rng).c_str());
  if(R.Claim>=0||State_->Time<R.Spawn) continue;
  if(!R.Visual.IsValid()) {
   auto* A=GetWorld()->SpawnActor<AStaticMeshActor>(R.Point+FVector(0,0,300),FRotator::ZeroRotator); auto* C=A->GetStaticMeshComponent(); C->SetMobility(EComponentMobility::Movable); C->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere"))); C->SetWorldScale3D(FVector(R.Tier+1)); C->SetCollisionEnabled(ECollisionEnabled::NoCollision); C->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_Glow_Eye"))); R.Visual=A;
   if(R.Tier==3) Announce(TEXT("THE MYTHIC RELIC HAS MANIFESTED AT MID. Level 30, six-second channel."));
  }
  int Candidate=-1,Teams=0;
  for(int I=0;I<State_->Fighters.Num();++I) {
   const auto& F=State_->Fighters[I]; if(!F.Pawn.IsValid()||F.Respawn>0||FVector::Dist2D(F.Pawn->FeetLocation(),R.Point)>1200||FMath::Abs(F.Pawn->FeetLocation().Z-R.Point.Z)>400) continue;
   Teams|=1<<F.Team;
   if(Candidate<0&&F.Level>=(R.Tier==1?10:R.Tier==2?20:30)&&State_->Time-F.LastHurt>.3f) Candidate=I;
  }
  if(Teams==3||Candidate<0) { R.Channel=0; R.ChannelSlot=-1; continue; }
  if(R.ChannelSlot!=Candidate) { R.ChannelSlot=Candidate; R.Channel=0; }
  R.Channel+=Dt;
  if(R.Channel>=R.Need) {
   R.Claim=Candidate; auto& F=State_->Fighters[Candidate]; F.RelicMult=FMath::Min(3.f,F.RelicMult+R.Amp);
   Announce(F.Name+TEXT(" claimed ")+R.Name); if(R.Visual.IsValid()) R.Visual->Destroy();
  }
 }
}
FString ACultMatchDirector::RelicStatus() const {
 if(!State_||!State_->Moba) return FString();
 for(const auto& R:State_->Relics) if(R.ChannelSlot==0&&R.Claim<0) return FString::Printf(TEXT("CLAIMING %s  %.0f%%"),*R.Name,R.Need>0?100*R.Channel/R.Need:100);
 return State_->Time<600?FString::Printf(TEXT("Mythic relic in %.0fs | Tiers require level 10 / 20 / 30"),600-State_->Time):TEXT("Mythic relic available at mid | Level 30");
}

AActor* ACultMatchDirector::SelectTowerTarget(ACultMatchUnit* Tower) {
 if(!Tower||!Tower->IsStructure()||Tower->Kind==4) return nullptr;
 auto InRange=[&](AActor* A) {
  if(!ACultSkillAttack::Alive(A)||!ACultSkillAttack::Enemy(Tower,A)) return false;
  float Radius=0; if(auto* Cap=A->FindComponentByClass<UCapsuleComponent>()) Radius=Cap->GetScaledCapsuleRadius();
  return FMath::Max(0.f,float(FVector::Dist2D(A->GetActorLocation(),Tower->GetActorLocation()))-Radius)<=Tower->Range;
 };
 AActor* Minion=nullptr; float Best=1.e9f;
 for(auto& W:State_->Units) if(W.IsValid()&&!W->IsStructure()&&!W->IsInTransit()&&InRange(W.Get())) {
  float Dist=FVector::DistSquared2D(W->GetActorLocation(),Tower->GetActorLocation()); if(Dist<Best) { Best=Dist; Minion=W.Get(); }
 }
 // Incoming hostile waves take aggression immediately; otherwise retain the current live hero.
 if(Minion) { Tower->AttackTarget=Minion; return Minion; }
 if(InRange(Tower->AttackTarget.Get())) return Tower->AttackTarget.Get();
 AActor* Hero=nullptr; Best=1.e9f;
 for(auto& F:State_->Fighters) if(F.Respawn<=0&&F.Pawn.IsValid()&&InRange(F.Pawn.Get())) {
  float Dist=FVector::DistSquared2D(F.Pawn->GetActorLocation(),Tower->GetActorLocation()); if(Dist<Best) { Best=Dist; Hero=F.Pawn.Get(); }
 }
 Tower->AttackTarget=Hero; return Hero;
}
