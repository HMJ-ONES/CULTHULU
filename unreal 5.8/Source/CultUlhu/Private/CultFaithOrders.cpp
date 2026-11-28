#include "CultWorldState.h"
#include "CultMatchDirector.h"

FString ACultWorldDirector::FaithOrder(int32 Action,AActor* Target,FVector Point)
{
 if(!HasAuthority()||!State_||ACultMatchDirector::IsMatch(GetWorld())) return TEXT("Cult orders are managed by the Free Roam host.");
 auto& F=*State_->Faiths[0];
 // Entity powers keep their original rules. They do not pretend to be worker jobs.
 if(Action==MassConvert) return FaithAction(TEXT("MassConvert"),Target,Point);
 if(Action==PlaceTrap) return FaithAction(TEXT("Trap"),Target,Point);
 if(Action==BreedCreatures) return FaithAction(TEXT("Breed"),Target,Point);
 if(Action==ImprisonCultist) return FaithAction(TEXT("Imprison"),Target,Point);
 if(Action==PunishCultist) return FaithAction(TEXT("Punish"),Target,Point);
 const FString Kind=Action==RestFollowers?TEXT("Rest"):Action==TortureCaptive?TEXT("Torture"):Action==SacrificeCaptive?TEXT("Sacrifice"):TEXT("Necromancy");
 auto* Victim=Cast<ACultPopulation>(Target); ACultDestructible* Altar=nullptr;
 if(Action==TortureCaptive) {
  // Captured beasts retain the original direct handling rule; human captives
  // use the society worker job below. Neither path invents a new reward.
  if(Cast<ACultFeralBeast>(Target)) return FaithAction(TEXT("Torture"),Target,Point);
  if(!Victim||!Victim->IsCaptive()||Victim->GetCaptiveFaction()!=0||Victim->GetHealth()<=0) return TEXT("ALT + click a living captive belonging to your cult, then choose Torture.");
  if(F.Tortured.Contains(Victim->GetUniqueID())) return TEXT("This captive has already yielded their one-time reward.");
  for(auto& Job:State_->Labors) if(Job.Worker.IsValid()&&Job.Victim==Victim) return TEXT("A follower is already working with this captive.");
  float Best=1200;
  for(TActorIterator<ACultDestructible> It(GetWorld());It;++It) if(It->ActorHasTag(TEXT("FaithAltar"))&&It->GetFaction()==0&&!It->IsDestroyed()) {
   const float Distance=FVector::Dist2D(It->GetActorLocation(),Victim->GetActorLocation()); if(Distance<Best) { Best=Distance; Altar=*It; }
  }
  if(!Altar) return TEXT("Bring this captive within 12m of your temple first.");
 }
 TArray<ACultPopulation*> Units;
 auto Available=[&](ACultPopulation* P) { return IsValid(P)&&(Action==RestFollowers||P!=Victim)&&P->IsCultist()&&P->GetFaction()==0&&!P->IsCaptive()&&P->GetHealth()>0; };
 if(SelectedCount()) { for(auto& P:State_->Selected) if(Available(P.Get())) Units.Add(P.Get()); }
 else for(auto& Row:State_->People) if(Available(Row.Actor.Get())&&!Row.Actor->HasPlayerBattle()) Units.Add(Row.Actor.Get());
 Units.Sort([&](const ACultPopulation& A,const ACultPopulation& B){return FVector::DistSquared(A.GetActorLocation(),Point)<FVector::DistSquared(B.GetActorLocation(),Point);});
 if(Units.IsEmpty()) return TEXT("Select available followers with ALT + drag first.");
 const auto Directive=Action==RestFollowers?cultulhu::DirectiveType::OneiricHarvest:cultulhu::DirectiveType::MassSacrifice;
 const cultulhu::Vec3 Pos{float(Point.X/100),float(Point.Z/100),float(Point.Y/100)};
 int Assigned=0,Refused=0; FString Result;
 for(auto* Worker:Units) {
  bool Loyal=false; for(auto& Row:State_->People) if(Row.Actor==Worker) Loyal=F.Cult.at(Row.Index).state()==cultulhu::CultistState::Loyal;
  if(Worker->IsLunatic()||Worker->GetLoyalty()<cultulhu::BuilderAI::LOYALTY_THRESHOLD||!Loyal||!F.Rng.chance(F.Commands.previewObedience(Directive,Pos).chance)) { ++Refused; continue; }
  if(Action==TortureCaptive) {
   // Same interruptible job and saved state as autonomous society. No reward on click.
   for(auto& Job:State_->Labors) if(Job.Worker==Worker) Job.Worker.Reset();
   Worker->PlayerOrder(TEXT("Torture"),Victim->GetActorLocation(),Victim);
   Worker->SetWorkProgress(0,false);
   State_->Labors.push_back({Worker,Victim,Altar,TEXT("Torture"),20,0,Worker->GetHealth(),Victim->GetHealth()});
   Result=TEXT("Torture ordered: a follower approaches, then works for 20s. Damage or reassignment interrupts."); ++Assigned; break;
  }
  if(Action==RestFollowers) { Worker->PlayerOrder(TEXT("Rest"),Worker->GetActorLocation()); Result=FaithAction(Kind,Worker,Worker->GetActorLocation()); ++Assigned; continue; }
  // Do not release a battle assignment unless the requested rite can actually start.
  if(Worker->HasPlayerBattle()) { Result=TEXT("Disengage this follower from battle before assigning a temple rite."); continue; }
  Result=FaithAction(Kind,Target,Point,0,Worker);
  if(Result.Contains(TEXT("began:"))) { ++Assigned; break; }
 }
 if(Result.IsEmpty()) Result=TEXT("No followers accepted this order.");
 if(Assigned&&Action==RestFollowers) Result=FString::Printf(TEXT("%d followers ordered to rest for 30s."),Assigned);
 if(Refused) Result+=FString::Printf(TEXT(" %d cultists are disobeying (loyalty / beliefs)."),Refused);
 return Result;
}
