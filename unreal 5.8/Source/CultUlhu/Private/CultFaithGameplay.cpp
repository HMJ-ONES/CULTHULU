#include "CultWorldState.h"
#include "CultMatchDirector.h"
#include "CultBeliefCatalog.h"
#include "beliefs/InteractionMatrix.h"
#include "exertion/ActionExertionTable.h"
#include "CultBlastFX.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "combat/Combat.h"

namespace {
cultulhu::Vec3 SoulPos(FVector P) { return {float(P.X/100),float(P.Z/100),float(P.Y/100)}; }
uint64 NativeSoul(AActor* A) { return A ? (1ull<<63)|A->GetUniqueID() : 0; }
float Planar(AActor* A,AActor* B) { return A&&B?FVector::Dist2D(A->GetActorLocation(),B->GetActorLocation()):1e9f; }
}
void ACultWorldDirector::InitializeFaith()
{
 if(ACultMatchDirector::IsMatch(GetWorld())) { for(auto& F:State_->Faiths) F->Beliefs.restoreActive({}); return; }
 if(auto* GI=Cast<UCultUlhuGameInstance>(GetGameInstance())) if(GI->InitialBeliefs().Num()==3) {
  std::vector<cultulhu::Belief> Choices; for(int Id:GI->InitialBeliefs()) Choices.push_back(static_cast<cultulhu::Belief>(Id));
  State_->Faiths[0]->Beliefs.restoreActive(Choices);
 }
 for(int F=0;F<3;++F) {
  auto& Faith=*State_->Faiths[F];
  Faith.Exertion.setPowerObserver([this,F](const cultulhu::GameEvent& E,float Delta){ OnFaithReward(F,E,Delta); });
  for(int I=0;I<int(cultulhu::EventType::Count);++I) Faith.Bus.subscribe(static_cast<cultulhu::EventType>(I),[this,F](const cultulhu::GameEvent& E){ OnFaithEvent(F,E); });
  Faith.RitualCaster.setTargetVulnerability([this](const cultulhu::Entity& Target) { return Target.type()==cultulhu::EntityType::Cultist && Target.faction()>=0&&Target.faction()<3 ? Beliefs(Target.faction()).conversionVulnerability():1.f; });
  Faith.Ambient.setCivilianPicker([this,F]() -> uint64 {
   auto& S=*State_->Faiths[F]; for(auto& Soul:S.Souls) if(Soul.Actor.IsValid()&&!Soul.Actor->IsCultist()&&!Soul.Actor->IsSummoned()&&Soul.Core->alive()&&(!S.ConversionTarget.IsValid()||S.ConversionTarget->IsCultist()||Soul.Actor==S.ConversionTarget)) return Soul.Core->id(); return 0;
  });
  Faith.Dreams.setCivilianPicker([this,F]() -> uint64 {
   auto& S=*State_->Faiths[F];
   for(auto& Soul:S.Souls) if(Soul.Actor.IsValid()&&!Soul.Actor->IsCultist()&&!Soul.Actor->IsSummoned()&&Soul.Core->alive()) return Soul.Core->id();
   return 0;
  });
 }
}
void ACultWorldDirector::OnFaithEvent(int32 Faction,const cultulhu::GameEvent& E)
{
 if(!State_) return; using T=cultulhu::EventType; auto& F=*State_->Faiths[Faction];
 F.EventCounts.FindOrAdd(int(E.type))++;
 if(E.type!=T::BuildProgress && E.type!=T::DirectiveProgress) {
  F.Journal.Insert(FString::Printf(TEXT("%05.0fs  %s  %s  [power %.1f]"),F.Clock.now(),*CultFaith::EventName(int(E.type)),UTF8_TO_TCHAR(E.tag.c_str()),F.Power.value()),0);
  if(F.Journal.Num()>48) F.Journal.SetNum(48);
 }
 if(E.type==T::CombatStarted) F.CombatRemaining=8;
 if(E.type==T::CultistLost) for(auto& Row:State_->People) if(Row.Faction==Faction&&(Row.Id==E.sourceId||Row.Id==E.targetId)) Row.Lost=true;
 if(E.type==T::ConversionPerformed && E.tag!="mass_convert") {
  ACultPopulation* Candidate=nullptr;
  for(auto& Soul:F.Souls) if(Soul.Actor.IsValid()&&(Soul.Core->id()==E.targetId||NativeSoul(Soul.Actor.Get())==E.targetId)) { Candidate=Soul.Actor.Get(); break; }
  if(!Candidate && F.ConversionTarget.IsValid()) Candidate=F.ConversionTarget.Get();
  if(!Candidate && E.targetId==0) for(auto& Soul:F.Souls) if(Soul.Actor.IsValid()&&Soul.Core->alive()&&!Soul.Actor->IsCultist()&&!Soul.Actor->IsSummoned()) { Candidate=Soul.Actor.Get(); break; }
  if(Candidate && Candidate->GetFaction()!=Faction) {
   bool Reserved=false; for(auto& C:State_->ConversionQueue) if(C.Actor==Candidate) Reserved=true;
   if(!Reserved) State_->ConversionQueue.Add({Candidate,Faction});
   for(auto& Soul:F.Souls) if(Soul.Actor==Candidate) Soul.Core->takeDamage(100000);
  }
 }
 if(E.type==T::LunaticActed) {
  if(E.tag=="stoleDevotion") for(auto& Row:State_->People) if(Row.Id==E.targetId&&Row.Actor.IsValid()) State_->ConversionQueue.Add({Row.Actor,(Faction+1)%3});
  if(E.tag=="freedPrisoners") for(auto& Row:State_->People) if(Row.Id==E.targetId&&Row.Actor.IsValid()) Row.Actor->ReleaseCaptive();
  if(E.tag=="attackedOwnShrine") for(TActorIterator<ACultDestructible> It(GetWorld());It;++It) if(It->GetFaction()==Faction&&!It->IsDestroyed()&&It->ActorHasTag(TEXT("CultTemple"))) { F.Projecting=true; It->ApplyDamage(10,It->GetActorLocation(),Faction); F.Projecting=false; break; }
 }
 if(E.type==T::SacrificeInterrupted&&E.tag=="lunatic") for(auto& Rite:F.Rites) if(Rite.Sacrifice&&Rite.Core->active()) {
  Rite.Core->interrupt(false); if(Rite.Performer.IsValid()) Rite.Performer->Order(TEXT("Patrol"),Rite.Performer->GetActorLocation(),nullptr,1); if(Rite.Victim.IsValid()) Rite.Victim->Order(TEXT("Patrol"),Rite.Victim->GetActorLocation(),nullptr,1); break;
 }
 if(E.type==T::LunaticActed&&E.tag=="freedCaptives") for(TActorIterator<ACultFeralBeast> It(GetWorld());It;++It) if(It->IsCaptured()&&It->GetCaptiveFaction()==Faction) { It->ReleaseCaptive(); break; }
 if(E.type==T::DeathDenied) for(auto& Row:State_->People) if(Row.Faction==Faction&&Row.Id==E.sourceId&&Row.Actor.IsValid()) Row.Actor->ApplyFaithState(0,Row.Actor->GetLoyalty(),false);
 if(E.type==T::Nightmare) for(auto& Row:State_->People) if(Row.Id==E.sourceId) { Row.RestRemaining=0; if(Row.Actor.IsValid()) Row.Actor->Order(TEXT("Nightmare"),Row.Actor->GetActorLocation(),nullptr,10); }
}
bool ACultWorldDirector::RequestBelief(int32 In,int32 Out,int32 Faction)
{
 if(ACultMatchDirector::IsMatch(GetWorld())) return false;
 if(!State_||In<0||In>=int(cultulhu::Belief::Count)||Faction<0||Faction>2) return false;
 auto& F=*State_->Faiths[Faction]; auto Incoming=static_cast<cultulhu::Belief>(In);
 const bool Ok=F.Beliefs.requestChange(Incoming,Out>=0&&Out<12?static_cast<cultulhu::Belief>(Out):cultulhu::Belief::Count);
 if(Ok) F.Journal.Insert(FString::Printf(TEXT("Adoption requested: %s; followers need 120 game seconds (Chaos accelerates)."),UTF8_TO_TCHAR(cultulhu::beliefName(Incoming))),0);
 return Ok;
}
float ACultWorldDirector::BeliefExertion(int32 B,int32 F) const { return State_&&B>=0&&B<12?State_->Faiths[FMath::Clamp(F,0,2)]->Exertion.exertion(static_cast<cultulhu::Belief>(B)):0; }
float ACultWorldDirector::FaithPower(int32 F) const { return State_?State_->Faiths[FMath::Clamp(F,0,2)]->Power.value():0; }
int32 ACultWorldDirector::FaithEventCount(int32 E,int32 F) const { return State_?State_->Faiths[FMath::Clamp(F,0,2)]->EventCounts.FindRef(E):0; }
TArray<FString> ACultWorldDirector::FaithJournal(int32 F) const { return State_?State_->Faiths[FMath::Clamp(F,0,2)]->Journal:TArray<FString>(); }
FString ACultWorldDirector::FaithSummary(int32 Faction) const
{
 if(!State_) return TEXT("Cult unavailable"); auto& F=*State_->Faiths[FMath::Clamp(Faction,0,2)]; const auto& S=F.Exertion.stats();
 return FString::Printf(TEXT("Power %.1f | Fear %.1f | Risk %.1f%% | Combat x%.2f | Conversion %.1f%% | Loyalty/s %+.4f\nResting %d | Sorcerer rituals %llu/%llu | Income x%.2f | %s"),F.Power.value(),F.Beliefs.fearLevel(),F.Cult.insurrectionRisk(),S.combatPowerMult,S.conversionChance*100,S.loyaltyDriftPerSec,int(F.Dreams.restingCount()),F.RitualCaster.ritualsSucceeded(),F.RitualCaster.ritualsAttempted(),(F.Beliefs.active().size()>=3?.5f:F.Beliefs.active().size()==2?.75f:1.f),F.InCombat?TEXT("in combat"):TEXT("out of combat"));
}
FString ACultWorldDirector::BeliefReport(int32 Id,int32 Faction) const
{
 if(!State_||Id<0||Id>=12) return TEXT(""); auto& F=*State_->Faiths[FMath::Clamp(Faction,0,2)]; const auto B=static_cast<cultulhu::Belief>(Id);
 FString R=CultFaith::Description(Id); R+=TEXT("\n\nCanonical exertion feeds (base; inactive beliefs receive half):\n");
 for(auto& Feed:cultulhu::ACTION_EXERTION_TABLE) if(Feed.belief==B) R+=FString::Printf(TEXT("%s %+.1f; "),*CultFaith::EventName(int(Feed.type)),Feed.amount);
 R+=TEXT("\n\nInteractions (both fervors above 25):\n");
 for(int Kind=0;Kind<2;++Kind) { const auto* P=Kind?cultulhu::conflictPairs():cultulhu::synergyPairs(); int Count=Kind?cultulhu::conflictPairCount():cultulhu::synergyPairCount();
  for(int I=0;I<Count;++I) if(P[I].a==B||P[I].b==B) { bool Hot=Kind?cultulhu::conflictActive(P[I].a,P[I].b,F.Exertion.levels()):cultulhu::synergyActive(P[I].a,P[I].b,F.Exertion.levels()); R+=FString::Printf(TEXT("%s: %s + %s (%s). "),Kind?TEXT("Conflict"):TEXT("Synergy"),UTF8_TO_TCHAR(P[I].name),UTF8_TO_TCHAR(cultulhu::beliefName(P[I].a==B?P[I].b:P[I].a)),Hot?TEXT("active"):TEXT("cold")); }
 }
 R+=TEXT("\nConflict tension: both above 35 raises risk by 1.5, once per 60s. Fervor decays toward 10 at 0.05/s.\nSource: game-core BeliefSystem / ActionExertionTable / InteractionMatrix.");
 return R;
}
void ACultWorldDirector::CombatActivity(int32 Faction,bool Melee)
{
 if(!State_||ACultMatchDirector::IsMatch(GetWorld())||Faction<0||Faction>2) return; auto& F=*State_->Faiths[Faction];
 F.CombatRemaining=8;
 if(Melee) { cultulhu::GameEvent E(cultulhu::EventType::MeleeAttack); E.amount=1; F.Bus.publish(E); }
}
float ACultWorldDirector::CultDamage(ACultPopulation* Attacker,float Base)
{
 if(!State_||ACultMatchDirector::IsMatch(GetWorld())||!Attacker||Attacker->GetFaction()<0) return Base;
 auto& F=*State_->Faiths[Attacker->GetFaction()]; CombatActivity(Attacker->GetFaction()); cultulhu::GameEvent Swing(cultulhu::EventType::MeleeAttack); Swing.sourceId=NativeSoul(Attacker); F.Bus.publish(Swing);
 cultulhu::AttackInfo Info; Info.baseDamage=Base; Info.melee=true; Info.attackerType=Attacker->IsSorcerer()?cultulhu::EntityType::Sorcerer:cultulhu::EntityType::Cultist;
 return cultulhu::combat::calcDamage(Info,F.Beliefs)*F.Exertion.stats().combatPowerMult;
}
bool ACultWorldDirector::PersonHurt(ACultPopulation* P,float Before,float Damage,int32 Attacker,bool Accounted,bool Feral)
{
 if(!State_||!P||Before<=0||Damage<=0) return false; const int Victim=P->GetFaction();
 if(Victim>=0) CombatActivity(Victim); if(Attacker>=0) { CombatActivity(Attacker); DeclareWar(Attacker,Victim); }
 if(P->GetHealth()>0) return false;
 if(Victim>=0&&P->IsCultist()) {
  auto& F=*State_->Faiths[Victim];
  for(auto& Row:State_->People) if(Row.Actor==P) {
   auto& C=F.Cult.at(Row.Index); C.takeDamage(100000);
   if(F.Beliefs.isActive(cultulhu::Belief::Sacrifice)&&F.Cult.denyDeath(C)) { P->ApplyFaithState(C.hp(),C.devotion(),C.state()==cultulhu::CultistState::Lunatic); return true; }
   if(!Row.Lost) { Row.Lost=true; cultulhu::GameEvent Lost(cultulhu::EventType::CultistLost); Lost.sourceId=Row.Id; F.Bus.publish(Lost); }
   if(Before>=100&&Damage>=Before) { cultulhu::GameEvent E(cultulhu::EventType::CultistOneHitKilled); F.Bus.publish(E); }
   if(Feral) { cultulhu::GameEvent E(cultulhu::EventType::CultistKilledByFeral); F.Bus.publish(E); }
   break;
  }
 }
 if(!Accounted&&Attacker>=0&&Attacker<3&&!P->IsSummoned()) {
  auto& F=*State_->Faiths[Attacker]; F.Wars.syncBeliefs(F.Beliefs);
  if(P->IsCultist()) { if(Attacker!=Victim&&AtWar(Attacker,Victim)) { cultulhu::GameEvent E(cultulhu::EventType::EnemyCultistSlain); E.faction=Victim; E.targetId=NativeSoul(P); F.Bus.publish(E); } }
  else { cultulhu::GameEvent E(cultulhu::EventType::CivilianSlain); E.amount=1; E.targetId=NativeSoul(P); F.Bus.publish(E); }
 }
 return false;
}
void ACultWorldDirector::BuildingHurt(ACultDestructible* B,int32 Attacker,bool Destroyed,AActor* Agent)
{
 if(!State_||!B) return; using T=cultulhu::EventType; int OwningFaction=B->GetFaction();
 if(OwningFaction>=0) { auto& F=*State_->Faiths[OwningFaction]; if(!F.Projecting) { cultulhu::GameEvent E(Destroyed?T::BaseBuildingDestroyed:T::BaseBuildingDamaged); E.targetId=NativeSoul(B); F.Bus.publish(E); } }
 if(Attacker<0||Attacker>2) return; CombatActivity(Attacker); DeclareWar(Attacker,OwningFaction);
 if(Destroyed) { auto& F=*State_->Faiths[Attacker];
  if(B->ActorHasTag(TEXT("LivingCity"))) { cultulhu::GameEvent E(T::CityBuildingDestroyed); E.sourceId=NativeSoul(Agent); E.targetId=NativeSoul(B); E.amount=1; F.Bus.publish(E);
   for(auto& City:State_->Cities) { bool Found=false; int Total=0,Ruins=0; for(auto& Building:City.Buildings) { ++Total; if(Building==B) Found=true; if(!Building.IsValid()||Building==B||Building->IsDestroyed()) ++Ruins; }
    if(Found&&Total>0) { cultulhu::GameEvent Raid(T::RaidPerformed); Raid.amount=1.f/Total; Raid.sourceId=NativeSoul(Agent); F.Bus.publish(Raid); if(Ruins==Total) { cultulhu::GameEvent Raze(T::DistrictRazed); Raze.sourceId=NativeSoul(Agent); F.Bus.publish(Raze); } break; }
   }
  }
  cultulhu::GameEvent Explosion(T::Explosion); F.Bus.publish(Explosion);
 }
}

bool ACultWorldDirector::BeginAmbient(ACultPopulation* P,int32 Action)
{
 if(!State_||!P||Action<0||Action>=int(cultulhu::AmbientAction::Count)) return false;
 using A=cultulhu::AmbientAction; const A Act=static_cast<A>(Action);
 for(auto& Row:State_->People) if(Row.Actor==P) {
  auto& F=*State_->Faiths[Row.Faction]; if(P->HasDirective()||P->IsCaptive()||P->IsLunatic()) return false;
  AActor* Target=nullptr; float Best=20000;
  const bool Audience=Act==A::Preach||Act==A::WhisperCampaign;
  const bool Pair=Act==A::Brawl||Act==A::Sparring||Act==A::TendWounded||Act==A::DreamSharing;
  if(Audience||Pair) for(TActorIterator<ACultPopulation> It(GetWorld());It;++It) if(*It!=P&&!It->IsSummoned()) {
   bool Eligible=Audience?!It->IsCultist():It->GetFaction()==Row.Faction&&!It->IsCaptive()&&!It->IsLunatic();
   if(Act==A::TendWounded&&It->GetHealth()>=100) Eligible=false;
   for(auto& Q:State_->ConversionQueue) if(Q.Actor==*It) Eligible=false;
   const float D=Planar(P,*It); if(Eligible&&D<Best) { Best=D; Target=*It; }
  }
  if(Act==A::WildsHunt) for(TActorIterator<ACultFeralBeast> It(GetWorld());It;++It) if(!It->IsDead()&&!It->IsCaptured()&&Planar(P,*It)<Best) { Best=Planar(P,*It); Target=*It; }
  if(Audience||Pair||Act==A::WildsHunt) if(!Target) return false;
  Row.Action=Action; Row.Target=Target; Row.ActionTime=30; Row.Timer=30;
  P->Order(UTF8_TO_TCHAR(cultulhu::ambientActionName(Act)),Target?Target->GetActorLocation():P->GetActorLocation(),Target,180);
  return true;
 }
 return false;
}
void ACultWorldDirector::AdvanceAmbient(float Dt)
{
 using A=cultulhu::AmbientAction; using S=cultulhu::CultistState;
 for(auto& Row:State_->People) if(Row.Actor.IsValid()) {
  auto* P=Row.Actor.Get(); auto& F=*State_->Faiths[Row.Faction]; if(Row.Index>=F.Cult.size()) continue; auto& C=F.Cult.at(Row.Index);
  if(!C.alive()||C.state()==S::Converted||C.state()==S::Imprisoned||C.state()==S::Lunatic||P->IsCaptive()) { F.Dreams.endRest(Row.Id); Row.RestRemaining=0; Row.Action=-1; continue; }
  if(P->HasDirective()) { F.Dreams.endRest(Row.Id); Row.RestRemaining=0; Row.Action=-1; continue; }
  if(Row.RestRemaining>0) { Row.RestRemaining-=Dt; if(Row.RestRemaining<=0) { F.Dreams.endRest(Row.Id); P->Order(TEXT("Patrol"),P->GetActorLocation(),nullptr,1); } continue; }
  Row.RestDue-=Dt;
  if(Row.Action<0 && Row.RestDue<=0 && F.Beliefs.isActive(cultulhu::Belief::Dreams)) { Row.RestRemaining=30; Row.RestDue=90; F.Dreams.startRest(Row.Id); P->Order(TEXT("Rest"),P->GetActorLocation(),nullptr,30); continue; }
  if(Row.Action<0) { Row.Timer-=Dt; if(Row.Timer<=0) { Row.Timer=30; auto Chosen=cultulhu::chooseAmbientAction(C,F.Beliefs,C.devotion()/100.f,FMath::Fmod(22.+F.Clock.now()*24./600.,24.),F.Rng); BeginAmbient(P,int(Chosen)); } continue; }
  const auto Action=static_cast<A>(Row.Action);
  if(P->GetActivity()!=UTF8_TO_TCHAR(cultulhu::ambientActionName(Action))) { Row.Action=-1; continue; }
  if(Row.Target.IsValid()&&Planar(P,Row.Target.Get())>550) continue;
  if(Action==A::WildsHunt) { auto* Beast=Cast<ACultFeralBeast>(Row.Target.Get()); if(Beast&&!Beast->IsDead()) continue; }
  else { if(P->IsStunned()) continue; Row.ActionTime-=Dt; P->SetWorkProgress(1.f-Row.ActionTime/30.f,true); if(Row.ActionTime>0) continue; }
  if((Action==A::Preach||Action==A::WhisperCampaign)&&(!Row.Target.IsValid()||Cast<ACultPopulation>(Row.Target.Get())->IsCultist())) { Row.Action=-1; continue; }
  F.ConversionTarget=Cast<ACultPopulation>(Row.Target.Get());
  F.Ambient.forceAction(Row.Index,Action); F.ConversionTarget.Reset();
  // Acting against a currently professed doctrine creates a punishable infringer.
  for(auto& Feed:cultulhu::ACTION_EXERTION_TABLE) { bool Against=false;
   if(Action==A::Brawl&&Feed.type==cultulhu::EventType::BrawlBrokeOut) Against=true;
   if(Action==A::Desecrate&&Feed.type==cultulhu::EventType::DesecrationDone) Against=true;
   if(Against) for(auto B:F.Beliefs.active()) if(cultulhu::beliefRelation(B,Feed.belief)==cultulhu::BeliefRelation::Conflict) C.setState(S::Infringer);
  }
  Row.Action=-1; Row.Timer=0;
 }
}

void ACultWorldDirector::TickFaith(float Dt)
{
 if(!State_) return;
 if(Dt>1.f) { float Remaining=Dt; while(Remaining>0.f) { const float Step=FMath::Min(Remaining,1.f); TickFaith(Step); Remaining-=Step; } return; }
 using S=cultulhu::CultistState;
 // Snapshot every person before any core system runs; never overwrite a partner's new injury/heal.
 for(auto& Row:State_->People) {
  auto& F=*State_->Faiths[Row.Faction]; size_t Found=F.Cult.size(); for(size_t I=0;I<F.Cult.size();++I) if(F.Cult.at(I).id()==Row.Id) { Found=I; break; }
  if(Found==F.Cult.size()) { if(Row.Actor.IsValid()) Row.Actor->Desert(); Row.Actor.Reset(); continue; } Row.Index=Found;
  auto& C=F.Cult.at(Found); if(!Row.Actor.IsValid()) { C.takeDamage(100000); F.Dreams.endRest(Row.Id); if(!Row.Lost) { Row.Lost=true; cultulhu::GameEvent E(cultulhu::EventType::CultistLost); E.sourceId=Row.Id; F.Bus.publish(E); } continue; }
  if(!Row.Actor->IsCultist()||Row.Actor->GetFaction()!=Row.Faction) { C.takeDamage(100000); F.Dreams.endRest(Row.Id); Row.Actor.Reset(); if(!Row.Lost) { Row.Lost=true; cultulhu::GameEvent E(cultulhu::EventType::CultistLost); E.sourceId=Row.Id; F.Bus.publish(E); } continue; }
  C.revive(Row.Actor->GetHealth()); C.setPosition(SoulPos(Row.Actor->GetActorLocation()));
  if(Row.Actor->IsCaptive()) C.setState(S::Imprisoned); else if(C.state()==S::Imprisoned) C.setState(S::Loyal);
  if(Row.Actor->HasDirective()||Row.Actor->IsCaptive()||C.state()==S::Lunatic) { F.Dreams.endRest(Row.Id); Row.RestRemaining=0; }
 }
 for(int Faction=0;Faction<3;++Faction) {
  auto& F=*State_->Faiths[Faction];
  // Stable mirrors make source ritual/dream choices point to real people.
  for(TActorIterator<ACultPopulation> It(GetWorld());It;++It) if(!It->IsSummoned()) {
   bool Found=false; for(auto& Soul:F.Souls) if(Soul.Actor==*It) { Found=true; break; }
   if(!Found) { FState::Faith::Soul Soul; Soul.Actor=*It; if(It->IsCultist()) Soul.Core=std::make_unique<cultulhu::Cultist>(It->GetFaction(),SoulPos(It->GetActorLocation()),100); else Soul.Core=std::make_unique<cultulhu::Civilian>(SoulPos(It->GetActorLocation())); F.Souls.push_back(std::move(Soul)); }
  }
  std::vector<cultulhu::Civilian*> Civilians; std::vector<cultulhu::Cultist*> Enemies; std::vector<cultulhu::Sorcerer*> Casters;
  for(auto& Soul:F.Souls) {
   if(!Soul.Actor.IsValid()) { Soul.Core->takeDamage(100000); continue; }
   auto* P=Soul.Actor.Get(); if(P->IsCultist()&&(Soul.Core->type()!=cultulhu::EntityType::Cultist||Soul.Core->faction()!=P->GetFaction())) Soul.Core=std::make_unique<cultulhu::Cultist>(P->GetFaction(),SoulPos(P->GetActorLocation()));
   if(!P->IsCultist()&&Soul.Core->type()!=cultulhu::EntityType::Civilian) Soul.Core=std::make_unique<cultulhu::Civilian>(SoulPos(P->GetActorLocation()));
   Soul.Core->revive(P->GetHealth()); Soul.Core->setPosition(SoulPos(P->GetActorLocation()));
   if(P->IsCultist()) { if(P->GetFaction()!=Faction) Enemies.push_back(static_cast<cultulhu::Cultist*>(Soul.Core.get())); }
   else Civilians.push_back(static_cast<cultulhu::Civilian*>(Soul.Core.get()));
   for(auto& Q:State_->ConversionQueue) if(Q.Actor==P) Soul.Core->takeDamage(100000);
  }
  bool HasCaster=false; for(auto& Row:State_->People) if(Row.Faction==Faction&&Row.Actor.IsValid()&&Row.Caster) HasCaster=true;
  if(!HasCaster) for(auto& Row:State_->People) if(Row.Faction==Faction&&Row.Actor.IsValid()) { Row.Caster=std::make_unique<cultulhu::Sorcerer>(Faction,SoulPos(Row.Actor->GetActorLocation())); Row.Actor->SetSorcerer(true); break; }
  for(auto& Row:State_->People) if(Row.Faction==Faction&&Row.Actor.IsValid()&&Row.Caster) { Row.Caster->setPosition(SoulPos(Row.Actor->GetActorLocation())); Row.Caster->revive(Row.Actor->GetHealth()); if(!Row.Actor->IsCaptive()&&!Row.Actor->IsLunatic()&&!Row.Actor->HasDirective()) Casters.push_back(Row.Caster.get()); }
  F.RitualCaster.setSorcerers(Casters); F.RitualCaster.setCivilians(Civilians); F.RitualCaster.setCultists(Enemies);
  F.Clock.advance(Dt); const double Hour=FMath::Fmod(22.+F.Clock.now()*24./600.,24.); const bool Night=Hour>=22||Hour<6;
  F.CombatRemaining=FMath::Max(0.f,F.CombatRemaining-Dt); const bool Combat=F.CombatRemaining>0;
  if(Combat!=F.InCombat) { cultulhu::GameEvent E(Combat?cultulhu::EventType::CombatStarted:cultulhu::EventType::CombatEnded); F.Bus.publish(E); F.InCombat=Combat; }
  int ArmySources=0; for(TActorIterator<ACultFeralBeast> It(GetWorld());It;++It) if(!It->IsDead()&&It->IsCaptured()&&It->GetCaptiveFaction()==Faction) ++ArmySources;
  for(TActorIterator<ACultPopulation> It(GetWorld());It;++It) if(It->ActorHasTag(TEXT("BredHybrid"))&&It->GetHealth()>0&&It->GetFaction()==Faction) ++ArmySources;
  F.Beliefs.setArmyFearSources(ArmySources);
  F.Beliefs.setInCombat(Combat); F.Beliefs.setNight(Night); F.Dreams.setNight(Night); F.Wars.syncBeliefs(F.Beliefs);
  F.Beliefs.update(Dt); F.Power.add(F.Beliefs.tick(Dt)); F.Power.add(F.Dreams.update(Dt)); F.Cult.update(Dt);
  // A revolt can erase roster entries before construction or other systems run.
  for(auto& Row:State_->People) if(Row.Faction==Faction) {
   size_t Found=F.Cult.size(); for(size_t I=0;I<F.Cult.size();++I) if(F.Cult.at(I).id()==Row.Id) { Found=I; break; }
   Row.Index=Found; if(Found==F.Cult.size()) { if(Row.Actor.IsValid()) Row.Actor->Desert(); Row.Actor.Reset(); F.Dreams.endRest(Row.Id); }
  }
  F.Lunatics.update(Dt); F.Exertion.update(Dt);
  F.RitualCaster.update(Dt); // Original 45s / 25 mana ritual; finite mana is preserved.
  F.Breeding.setBreedingActive(F.Beliefs.isActive(cultulhu::Belief::Breeding)); F.Breeding.setBreedingSkill(F.Exertion.exertion(cultulhu::Belief::Breeding)/100.f);
  for(auto& Rite:F.Rites) if(Rite.Core->active()) {
   if(Rite.Performer.IsValid()) Rite.Performer->SetWorkProgress(Rite.Core->progress(),true);
   const bool PerformerReady=Rite.Performer.IsValid()&&!Rite.Performer->IsCaptive()&&!Rite.Performer->IsLunatic()&&!Rite.Performer->IsStunned()&&Rite.Performer->GetHealth()>=Rite.PerformerHp&&Rite.Performer->GetActivity()==(Rite.Sacrifice?TEXT("SacrificePerformer"):TEXT("Necromancy"));
   const bool VictimReady=!Rite.Sacrifice||(Rite.Victim.IsValid()&&Rite.Victim->GetHealth()>=Rite.VictimHp&&Rite.Victim->GetActivity()==TEXT("SacrificeVictim")&&Planar(Rite.Victim.Get(),Rite.Altar.Get())<=1200);
   if(!PerformerReady||!VictimReady||!Rite.Altar.IsValid()||Rite.Altar->IsDestroyed()||Planar(Rite.Performer.Get(),Rite.Altar.Get())>1200) {
    Rite.Core->interrupt(); if(Rite.Performer.IsValid()) Rite.Performer->Order(TEXT("Patrol"),Rite.Performer->GetActorLocation(),nullptr,1); if(Rite.Victim.IsValid()) Rite.Victim->Order(TEXT("Patrol"),Rite.Victim->GetActorLocation(),nullptr,1); continue;
   }
   if(Rite.Core->update(Dt)) { if(Rite.Victim.IsValid()) Rite.Victim->ReceiveAttack(100000,Faction,true); Rite.Performer->Order(TEXT("Patrol"),Rite.Performer->GetActorLocation(),nullptr,1); }
  }
  for(auto& Snare:F.Snares) if(F.Traps.get(Snare.Id)->armed&&Snare.Marker.IsValid()&&!Snare.Marker->IsDestroyed()) for(auto* C:Civilians) if(C->alive()) {
   const FVector P(C->position().x*100,C->position().z*100,C->position().y*100);
   if(FVector::Dist2D(P,Snare.Point)>200) continue;
   if(F.Traps.springTrap(Snare.Id,*C)) for(auto& Soul:F.Souls) if(Soul.Core.get()==C&&Soul.Actor.IsValid()) Soul.Actor->Capture(nullptr,Faction);
   break;
  }
  F.WorkTimer-=Dt; if(F.WorkTimer<=0) { F.WorkTimer=F.Rng.uniform(15.f,25.f); if(F.Cult.insurrectionRisk()<70) {
   int Idle=0; for(auto& Row:State_->People) if(Row.Faction==Faction&&Row.Actor.IsValid()&&!Row.Actor->HasDirective()&&Row.Actor->GetLoyalty()>=40&&Row.Action<0&&Row.RestRemaining<=0&&!Row.Actor->IsCaptive()&&!Row.Actor->IsLunatic()) ++Idle;
   FVector Site; if(Idle>=4&&Ground(State_->Temples[Faction]+FVector(F.Rng.uniform(-10000,10000),F.Rng.uniform(-12000,-8000),0),Site)) FactionOrder(Faction,TEXT("Build"),Site,nullptr);
  } }
 }
 // Core revolts can erase cultist slots. Resolve stable ids again before touching indices.
 for(auto& Row:State_->People) {
  auto& F=*State_->Faiths[Row.Faction]; size_t Found=F.Cult.size(); for(size_t I=0;I<F.Cult.size();++I) if(F.Cult.at(I).id()==Row.Id) { Found=I; break; }
  Row.Index=Found; if(Found==F.Cult.size()) { if(Row.Actor.IsValid()) Row.Actor->Desert(); Row.Actor.Reset(); F.Dreams.endRest(Row.Id); }
 }
 AdvanceAmbient(Dt);
 // Reconstruction uses the source Building implementation and its exact 60s timer.
 for(TActorIterator<ACultDestructible> It(GetWorld());It;++It) if(It->GetFaction()>=0&&!It->IsDestroyed()) {
  bool Found=false; for(auto& R:State_->Repairs) if(R.Actor==*It) Found=true;
  if(!Found) { FState::Repair R; R.Actor=*It; R.Core=std::make_unique<cultulhu::Building>(It->GetFaction(),SoulPos(It->GetActorLocation()),It->GetMaxHealth()); State_->Repairs.push_back(std::move(R)); }
 }
 for(auto& R:State_->Repairs) if(R.Actor.IsValid()&&!R.Actor->IsDestroyed()) {
  auto& F=*State_->Faiths[R.Actor->GetFaction()]; const float Current=R.Actor->GetHealth();
  if(Current<R.Core->hp()) R.Core->takeDamage(R.Core->hp()-Current); else if(Current>R.Core->hp()) R.Core->heal(Current-R.Core->hp());
  R.Core->setAutoRebuild(F.Beliefs.isActive(cultulhu::Belief::Reconstruction)); R.Core->update(Dt);
  if(R.Core->hp()>Current) { R.Actor->RepairHealth(R.Core->hp()); cultulhu::GameEvent E(cultulhu::EventType::BuildingRebuilt); E.targetId=NativeSoul(R.Actor.Get()); F.Bus.publish(E); }
 }
 for(auto& Row:State_->People) if(Row.Actor.IsValid()&&Row.Index<State_->Faiths[Row.Faction]->Cult.size()) {
  auto& F=*State_->Faiths[Row.Faction]; auto& C=F.Cult.at(Row.Index);
  if(!C.alive()&&!Row.Lost) { Row.Lost=true; cultulhu::GameEvent E(cultulhu::EventType::CultistLost); E.sourceId=Row.Id; F.Bus.publish(E); }
  Row.Actor->ApplyFaithState(C.hp(),C.devotion(),C.state()==S::Lunatic);
 }
 auto Queue=MoveTemp(State_->ConversionQueue); State_->ConversionQueue.Reset();
 for(auto& Q:Queue) if(Q.Actor.IsValid()&&Q.Actor->GetFaction()!=Q.Faction) Q.Actor->TransferTo(Q.Faction);
}
