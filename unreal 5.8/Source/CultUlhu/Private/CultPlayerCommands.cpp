#include "CultWorldState.h"
#include "CultBlastFX.h"

namespace {
bool Commandable(const ACultPopulation* P) { return IsValid(P)&&P->GetHealth()>0&&P->IsCultist()&&P->GetFaction()==0&&!P->IsCaptive(); }
cultulhu::Vec3 CommandPos(FVector P) { return {float(P.X/100),float(P.Z/100),float(P.Y/100)}; }
}
void ACultWorldDirector::SelectFollowers(const TArray<ACultPopulation*>& Units)
{
 if(!State_) return;
 TArray<TWeakObjectPtr<ACultPopulation>> Next; for(auto* P:Units) if(Commandable(P)) Next.AddUnique(P);
 bool Same=Next.Num()==State_->Selected.Num(); for(auto& P:Next) Same=Same&&State_->Selected.Contains(P);
 if(!Same) { State_->Prepared.Reset(); State_->PreparedActive=false; }
 State_->Selected.Reset(); for(auto* P:Units) if(Commandable(P)) State_->Selected.AddUnique(P);
}
bool ACultWorldDirector::IsSelected(const ACultPopulation* P) const { return State_&&Commandable(P)&&State_->Selected.Contains(P); }
int32 ACultWorldDirector::SelectedCount() const { int N=0; if(State_) for(auto& P:State_->Selected) N+=Commandable(P.Get()); return N; }
bool ACultWorldDirector::BattlePrepared() const { return State_&&State_->PreparedActive; }
int32 ACultWorldDirector::PreparedCount() const { if(!HasAuthority()) return State_?State_->RemotePrepared:0; int N=0; if(State_) for(auto& P:State_->Prepared) N+=Commandable(P.Get()); return N; }
FString ACultWorldDirector::CommandSummary() const
{
 if(!State_) return TEXT("");
 int Fighting=0; for(auto& Row:State_->People) if(Commandable(Row.Actor.Get())&&Row.Actor->HasPlayerBattle()) ++Fighting;
 return FString::Printf(TEXT("Selected %d | %s | %d assigned to battle | Conversion %s"),SelectedCount(),
  BattlePrepared()?*FString::Printf(TEXT("prepared group %d"),PreparedCount()):TEXT("all unassigned followers ready"),Fighting,
  State_->Faiths[0]->Beliefs.canMassConvert()?TEXT("READY: 3 recruits"):TEXT("recharging / not adopted"));
}
FString ACultWorldDirector::PlayerOrder(int32 Action,AActor* Target,FVector Point)
{
 if(!State_) return TEXT("Cult unavailable.");
 using A=cultulhu::MenuAction;
 auto& F=*State_->Faiths[0]; const auto Kind=static_cast<A>(Action);
 if(Action==Preach) return PreachAtEntity();
 if(Action==ClearSelection) { State_->Selected.Reset(); return TEXT("Selection cleared. Battle assignments continue."); }
 if(Action==PrepareBattle) {
  if(State_->PreparedActive) { State_->PreparedActive=false; State_->Prepared.Reset(); return TEXT("Battle preparation disengaged. Existing fronts continue; all unassigned followers are available."); }
  if(!SelectedCount()) return TEXT("Select your followers with ALT + drag first.");
  State_->Prepared=State_->Selected; State_->PreparedActive=true;
  return FString::Printf(TEXT("%d prepared. Press M to select a distant region, or ALT + click a local target."),PreparedCount());
 }
 TArray<ACultPopulation*> Units;
 if(Action==GlobalRetreat) {
  for(auto& Row:State_->People) if(Commandable(Row.Actor.Get())) Units.Add(Row.Actor.Get());
  State_->PreparedActive=false; State_->Prepared.Reset();
 } else if(Action==DisengageBattle) {
  for(auto& P:State_->Selected) if(Commandable(P.Get())) Units.Add(P.Get());
  if(Units.IsEmpty()) return TEXT("Select the group you want to disengage.");
  for(auto* P:Units) State_->Prepared.Remove(P);
  if(State_->Prepared.IsEmpty()) State_->PreparedActive=false;
 } else {
  const bool Combat=Kind==A::AttackTarget||Kind==A::RaidAt;
  if(Combat&&State_->PreparedActive) {
   for(auto& P:State_->Prepared) if(Commandable(P.Get())) Units.Add(P.Get());
  } else if(!Combat&&!State_->Selected.IsEmpty()) {
   for(auto& P:State_->Selected) if(Commandable(P.Get())) Units.Add(P.Get());
  } else {
   // Previously committed groups keep their fronts until explicitly reselected.
   for(auto& Row:State_->People) if(Commandable(Row.Actor.Get())&&!Row.Actor->HasPlayerBattle()) Units.Add(Row.Actor.Get());
  }
 }
 if(Units.IsEmpty()) return TEXT("No available followers in this group. Select another group or use Global retreat.");
 const bool Combat=Kind==A::AttackTarget||Kind==A::RaidAt;
 const bool Build=Kind==A::BuildAt||Kind==A::BuildAltarAt;
 if(Combat) {
  int Enemy=-1; if(auto* P=Cast<ACultPopulation>(Target)) Enemy=P->GetFaction();
  if(auto* P=Cast<ACultUlhuPlayerPawn>(Target)) Enemy=P->GetFaction();
  if(Enemy==0) return TEXT("Select an enemy or a city to attack.");
  if(Enemy>0) DeclareWar(0,Enemy);
 }
 if(Build) { FVector Floor; if(!Ground(Point,Floor)) return TEXT("Choose solid ground for construction."); Point=Floor; }
 FString Verb=Action==GlobalRetreat||Action==DisengageBattle?TEXT("Retreat"):Kind==A::Follow?TEXT("Follow"):
  Kind==A::AttackTarget?TEXT("Attack"):Kind==A::RaidAt?TEXT("Raid"):Build?TEXT("Build"):
  Kind==A::CaptureOrder?TEXT("Capture"):Kind==A::ConvertOrder?TEXT("Convert"):TEXT("Move");
 int Refused=0,Assigned=0;
 FState::Build Site;
 if(Build) { Site.Point=Point; Site.Faction=0; Site.Site=std::make_unique<cultulhu::ConstructionSite>(0,CommandPos(Point),Kind==A::BuildAltarAt?cultulhu::BuildingType::Altar:cultulhu::BuildingType::Barracks); }
 for(auto* P:Units) {
  if(Build) {
   bool Loyal=false; for(auto& Row:State_->People) if(Row.Actor==P) Loyal=F.Cult.at(Row.Index).state()==cultulhu::CultistState::Loyal;
   const float Chance=F.Commands.previewObedience(cultulhu::DirectiveType::RebuildSanctum,CommandPos(Point)).chance;
   if(P->GetLoyalty()<cultulhu::BuilderAI::LOYALTY_THRESHOLD||P->IsLunatic()||!Loyal||!F.Rng.chance(Chance)) { ++Refused; continue; }
  }
  P->PlayerOrder(Verb,Point,(Action==GlobalRetreat||Action==DisengageBattle||Build)?nullptr:Target,Combat);
  if(Build) Site.Workers.Add(P);
  ++Assigned;
 }
 if(Build&&Assigned) {
  State_->Builds.push_back(std::move(Site)); cultulhu::GameEvent E(cultulhu::EventType::BuildStarted); E.tag=Kind==A::BuildAltarAt?"altar":"barracks"; E.pos=CommandPos(Point); F.Bus.publish(E);
 }
 return FString::Printf(TEXT("%s: %d followers assigned.%s"),*Verb,Assigned,Refused?*FString::Printf(TEXT(" %d cultists are disobeying (loyalty / beliefs)."),Refused):TEXT(""));
}
FString ACultWorldDirector::PreachAtEntity()
{
 if(!State_) return TEXT("Cult unavailable.");
 auto* Player=Cast<ACultUlhuPlayerPawn>(UGameplayStatics::GetPlayerPawn(GetWorld(),0));
 if(!Player||Player->GetHealth()<=0) return TEXT("Your entity must be present to preach.");
 const float Now=GetWorld()->GetTimeSeconds();
 if(Now<State_->NextPreach) return FString::Printf(TEXT("Preaching ready in %.0fs."),FMath::CeilToFloat(State_->NextPreach-Now));
 auto& F=*State_->Faiths[0]; TArray<ACultPopulation*> Audience;
 const FVector Origin=Player->FeetLocation()+FVector(0,0,400);
 for(TActorIterator<ACultPopulation> It(GetWorld());It;++It) if(!It->IsCultist()&&!It->IsSummoned()&&!It->IsCaptive()&&It->GetHealth()>0&&FVector::Dist(Origin,It->GetActorLocation())<4500) {
  FHitResult Hit; FCollisionQueryParams Q(SCENE_QUERY_STAT(CultPreach),false,Player);
  if(!GetWorld()->LineTraceSingleByChannel(Hit,Origin,It->GetActorLocation(),ECC_Visibility,Q)||Hit.GetActor()==*It) Audience.Add(*It);
 }
 if(Audience.IsEmpty()) return TEXT("Preach near living civilians in a city or village (45m; line of sight).");
 State_->NextPreach=Now+20;
 ACultBlastFX::SpawnBlast(GetWorld(),ECultBlastKind::ShockRing,Player->FeetLocation(),45.f);
 // Shuffle actual listeners so repeated sermons do not always choose the first spawned city.
 for(int I=Audience.Num()-1;I>0;--I) Audience.Swap(I,State_->Random.RandRange(0,I));
 cultulhu::GameEvent Sermon(cultulhu::EventType::SermonPreached); Sermon.sourceId=(1ull<<63)|Player->GetUniqueID(); Sermon.amount=1; Sermon.pos=CommandPos(Player->FeetLocation()); F.Bus.publish(Sermon);
 int Converts=0;
 if(F.Beliefs.canMassConvert()&&Audience.Num()>=cultulhu::BeliefSystem::MASS_CONVERT_COUNT) {
  for(int I=0;I<cultulhu::BeliefSystem::MASS_CONVERT_COUNT;++I) { Audience[I]->TransferTo(0); Audience[I]->PlayerOrder(TEXT("Follow"),Player->FeetLocation()); ++Converts; }
  F.Beliefs.useMassConvert(); // Exactly one canonical three-soul receipt, never double-rewarded.
 } else {
  for(auto* P:Audience) {
   auto& ReadyAt=State_->SermonAudience.FindOrAdd(P); if(Now<ReadyAt) continue; ReadyAt=Now+60;
   // Original ambient sermon chance, with modest raid terror. At most one ordinary recruit per sermon.
   const float Chance=FMath::Min(.75f,.25f*F.Beliefs.conversionEfficiency()*(F.InCombat?1.5f:1.f));
   if(!F.Rng.chance(Chance)) break;
   P->TransferTo(0); P->PlayerOrder(TEXT("Follow"),Player->FeetLocation()); ++Converts;
   cultulhu::GameEvent E(cultulhu::EventType::ConversionPerformed); E.sourceId=Sermon.sourceId; E.targetId=(1ull<<63)|P->GetUniqueID(); E.amount=1; E.tag="entity_sermon"; E.pos=CommandPos(P->GetActorLocation()); F.Bus.publish(E); break;
  }
 }
 for(auto It=State_->SermonAudience.CreateIterator();It;++It) if(!It.Key().IsValid()) It.RemoveCurrent();
 return FString::Printf(TEXT("Preached to %d civilians: %d joined and will follow you. %s"),Audience.Num(),Converts,
  Converts==3?TEXT("Conversion recharges in 200s."):TEXT("Most resist. Conversion ready recruits 3; preaching recharges in 20s."));
}
