#include "CultWorldState.h"
#include "CultActorLabels.h"
#include "CultMatchDirector.h"
#include "CultBeliefCatalog.h"
#include "CultBlastFX.h"
namespace {
uint64 NativeSoul(AActor* A) { return A ? (1ull<<63)|A->GetUniqueID() : 0; }
cultulhu::Vec3 RitePos(FVector P) { return {float(P.X/100),float(P.Z/100),float(P.Y/100)}; }
}
FString ACultWorldDirector::FaithAction(const FString& Action,AActor* Target,FVector Point,int32 Faction,ACultPopulation* Agent)
{
 if(ACultMatchDirector::IsMatch(GetWorld())) return TEXT("Beliefs, civilian rites, and cult management are available only in Free Roam.");
 if(!State_||Faction<0||Faction>2) return TEXT("Cult unavailable"); using B=cultulhu::Belief; using E=cultulhu::EventType;
 auto& F=*State_->Faiths[Faction]; auto* Person=Cast<ACultPopulation>(Target);
 auto* Altar=Cast<ACultDestructible>(Target);
 if(!Altar||!Altar->ActorHasTag(TEXT("FaithAltar"))||Altar->GetFaction()!=Faction||Altar->IsDestroyed()) {
  Altar=nullptr; float Best=20000;
  for(TActorIterator<ACultDestructible> It(GetWorld());It;++It) if(It->ActorHasTag(TEXT("FaithAltar"))&&It->GetFaction()==Faction&&!It->IsDestroyed()) { const float D=FVector::Dist2D(It->GetActorLocation(),Point); if(D<Best) { Altar=*It; Best=D; } }
 }
 auto Emit=[&](E Type,float Amount=1.f) { cultulhu::GameEvent Event(Type); Event.amount=Amount; Event.pos=RitePos(Point); Event.sourceId=NativeSoul(Agent); Event.targetId=NativeSoul(Target); F.Bus.publish(Event); };
 if(Action==TEXT("Rest")) {
  for(auto& Row:State_->People) if(Row.Faction==Faction&&Row.Actor.IsValid()&&(!Person||Row.Actor==Person)&&!Row.Actor->IsCaptive()&&!Row.Actor->IsLunatic()) { Row.Action=-1; Row.RestRemaining=30; Row.RestDue=90; Row.Actor->Order(TEXT("Rest"),Row.Actor->GetActorLocation(),nullptr,30); F.Dreams.startRest(Row.Id); return TEXT("A follower rests for 30s; Dreams rewards actual resting time."); }
  return TEXT("Select an available follower.");
 }
 if(Action==TEXT("MassConvert")) {
  if(!F.Beliefs.canMassConvert()) return TEXT("Conversion must be adopted and its 200s cooldown ready.");
  TArray<ACultPopulation*> People;
  for(TActorIterator<ACultPopulation> It(GetWorld());It;++It) if(!It->IsCultist()&&!It->IsSummoned()) { People.Add(*It); if(People.Num()==cultulhu::BeliefSystem::MASS_CONVERT_COUNT) break; }
  if(People.Num()!=cultulhu::BeliefSystem::MASS_CONVERT_COUNT) return TEXT("Three living civilians are required; no phantom converts are awarded.");
  for(auto* P:People) P->TransferTo(Faction); F.Beliefs.useMassConvert(); return TEXT("Three real civilians converted. Mass conversion recharges in 200s.");
 }
 if(Action==TEXT("Torture")) {
  if(auto* Beast=Cast<ACultFeralBeast>(Target)) {
   if(Beast->IsDead()||!Beast->IsCaptured()||Beast->GetCaptiveFaction()!=Faction) return TEXT("Capture this living creature for your cult first.");
   if(F.Tortured.Contains(Beast->GetUniqueID())) return TEXT("This creature has already yielded its per-victim torture reward.");
   F.Tortured.Add(Beast->GetUniqueID()); Emit(E::TorturePerformed); return TEXT("Captured creature tortured; the canonical per-victim reward applied once.");
  }
  if(!Person||!Person->IsCaptive()) return TEXT("Capture a living civilian, creature, or enemy cultist, then aim at them.");
  if(Person->GetCaptiveFaction()>=0&&Person->GetCaptiveFaction()!=Faction) return TEXT("This captive belongs to another cult.");
  if(F.Tortured.Contains(Person->GetUniqueID())) return TEXT("This captive has already yielded their per-victim torture reward.");
  F.Tortured.Add(Person->GetUniqueID());
  if(Person->IsCultist()&&Person->GetFaction()==Faction) Emit(E::OwnCultistTortured);
  else Emit(Person->IsCultist()?E::CapturedTortured:E::TorturePerformed);
  return TEXT("Torture performed on the selected captive. The original power and fervor rules applied.");
 }
 if(Action==TEXT("Imprison")||Action==TEXT("Punish")) {
  if(!Person||Person->GetFaction()!=Faction||!Person->IsCultist()) return TEXT("Aim at one of your cultists.");
  for(auto& Row:State_->People) if(Row.Actor==Person) {
   auto& C=F.Cult.at(Row.Index);
   if(Action==TEXT("Imprison")) { Person->Capture(nullptr,Faction); C.setState(cultulhu::CultistState::Imprisoned); F.Dreams.endRest(Row.Id); Emit(E::CultistImprisoned); return TEXT("Cultist imprisoned. Chaos loses power; their work stops."); }
   if(C.state()==cultulhu::CultistState::Lunatic) { F.Lunatics.alignLunatic(Row.Index); Person->ApplyFaithState(C.hp(),C.devotion(),true); return TEXT("Lunatic struck down; the original alignment risk applies."); }
   if(C.state()!=cultulhu::CultistState::Infringer) return TEXT("This cultist is not an infringer or lunatic. No punishment was applied.");
   F.Beliefs.punishInfringer(); C.setState(cultulhu::CultistState::Loyal); return TEXT("Remaining adoption times halved; risk increased by 10 (another 10 under Chaos).");
  }
 }
 if(Action==TEXT("Trap")) {
  FVector GroundPoint; if(!Ground(Point,GroundPoint)) return TEXT("Aim at clear walkable ground.");
  auto* Marker=GetWorld()->SpawnActor<ACultDestructible>(GroundPoint,FRotator::ZeroRotator); if(!Marker) return TEXT("No room for a trap.");
  Marker->SetMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Game/CultUlhu/World/rune-spike/StaticMeshes/rune-spike"))); Marker->Tags.Add(FName(*FString::Printf(TEXT("Faction%d"),Faction))); Marker->Tags.Add(TEXT("FaithTrap"));
  const uint64 Id=F.Traps.placeTrap(RitePos(GroundPoint)); F.Snares.push_back({Id,GroundPoint,Marker}); return TEXT("Single-use trap placed; a real civilian must enter it before rewards apply.");
 }
 if(Action==TEXT("Breed")) {
  if(!F.Beliefs.isActive(B::Breeding)) return TEXT("Adopt Breeding first.");
  if(!Altar) return TEXT("A living owned temple is required nearby.");
  TArray<ACultFeralBeast*> Beasts;
  for(TActorIterator<ACultFeralBeast> It(GetWorld());It;++It) if(It->IsCaptured()&&It->GetCaptiveFaction()==Faction&&!It->IsDead()&&FVector::Dist2D(It->GetActorLocation(),Altar->GetActorLocation())<10000) Beasts.Add(*It);
  if(Beasts.Num()<2) return TEXT("Bring two captured beasts near your temple (ALT capture orders).");
  FVector Spawn; if(!Ground(Altar->GetActorLocation()+FVector(6000,-2000,0),Spawn)) return TEXT("No safe ground for the birth.");
  F.Breeding.setBreedingActive(true); F.Breeding.setBreedingSkill(F.Exertion.exertion(B::Breeding)/100.f);
  auto Child=F.Breeding.breed(cultulhu::Species::Beast,cultulhu::Species::Beast,Faction,RitePos(Spawn)); if(!Child) return TEXT("These bloodlines cannot be bred.");
  if(Child->feral()) { FTransform T(Spawn+FVector(0,0,300)); auto* Beast=GetWorld()->SpawnActorDeferred<ACultFeralBeast>(ACultFeralBeast::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn); Beast->SetSpecies(TEXT("ossified_brute")); Beast->FinishSpawning(T); Beast->SetBredHealth(Child->maxHp()); Beast->Tags.Add(TEXT("BredHybrid")); CultActorLabels::Set(Beast,TEXT("Feral bred hybrid")); }
  else { FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn; auto* ChildActor=GetWorld()->SpawnActor<ACultPopulation>(Spawn+FVector(0,0,100),FRotator::ZeroRotator,Params); if(ChildActor) { ChildActor->ConfigureBred(Faction,Child->maxHp()); ChildActor->Tags.Add(TEXT("BredHybrid")); CultActorLabels::Set(ChildActor,TEXT("Bred beast hybrid")); } }
  return FString::Printf(TEXT("A %s hybrid was born; canonical feral chance %.1f%%."),Child->feral()?TEXT("feral"):TEXT("loyal"),F.Breeding.effectiveFeralChance()*100);
 }
 if(Action==TEXT("Sacrifice")||Action==TEXT("Necromancy")) {
  if(!Altar) return TEXT("Aim near an intact temple belonging to your faction.");
  ACultPopulation* Performer=nullptr; float Best=1200;
  for(auto& Row:State_->People) if(Row.Faction==Faction&&Row.Actor.IsValid()&&Row.Actor!=Person&&(!Agent||Row.Actor==Agent)&&!Row.Actor->IsCaptive()&&!Row.Actor->IsLunatic()) {
   float D=FVector::Dist2D(Row.Actor->GetActorLocation(),Altar->GetActorLocation()); if(D<Best) { Performer=Row.Actor.Get(); Best=D; }
  }
  if(!Performer) return TEXT("A free cultist must stand within 12m of the temple. Move a follower there first.");
  for(auto& R:F.Rites) if(R.Core->active()&&R.Performer==Performer) return TEXT("This performer is already conducting a ritual.");
  FState::Faith::Rite Rite; Rite.Performer=Performer; Rite.Altar=Altar; Rite.PerformerHp=Performer->GetHealth();
  if(Action==TEXT("Sacrifice")) {
   if(!Person) for(TActorIterator<ACultPopulation> It(GetWorld());It;++It) if(It->IsCaptive()&&FVector::Dist2D(It->GetActorLocation(),Altar->GetActorLocation())<1200) { Person=*It; break; }
   if(!Person||Person==Performer||(!Person->IsCaptive()&&Person->GetFaction()!=Faction)||FVector::Dist2D(Person->GetActorLocation(),Altar->GetActorLocation())>1200) return TEXT("A captive or offered cultist must be at the temple, within 12m.");
   auto Core=std::make_unique<cultulhu::SacrificeRitual>(F.Bus); Core->setVictim(NativeSoul(Person)); Core->setPerformer(NativeSoul(Performer)); Rite.Core=std::move(Core); Rite.Victim=Person; Rite.Sacrifice=true; Rite.VictimHp=Person->GetHealth();
   Person->Order(TEXT("SacrificeVictim"),Person->GetActorLocation(),nullptr,35,true); Performer->Order(TEXT("SacrificePerformer"),Performer->GetActorLocation(),nullptr,35,true);
  } else { Rite.Core=std::make_unique<cultulhu::NecromancyRitual>(F.Bus); Performer->Order(TEXT("Necromancy"),Performer->GetActorLocation(),nullptr,50,true); }
  Rite.Core->start(); F.Rites.push_back(std::move(Rite)); return Action==TEXT("Sacrifice")?TEXT("Sacrifice began: 30s. Disrupting the participants or altar interrupts it."):TEXT("Necromancy began: 45s. This original rite grants Magic power; it does not invent a resurrection ability.");
 }
 return TEXT("No action performed.");
}
