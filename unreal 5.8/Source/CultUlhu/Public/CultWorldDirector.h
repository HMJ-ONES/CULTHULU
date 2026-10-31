#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CultWorldDirector.generated.h"
class ACultPopulation;
namespace cultulhu { class EventBus; class BeliefSystem; class RNG; struct GameEvent; }
UCLASS()
class CULTULHU_API ACultWorldDirector : public AActor
{
 GENERATED_BODY()
public:
 ACultWorldDirector();
 virtual void BeginPlay() override;
 virtual void Tick(float Dt) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 static ACultWorldDirector* Find(UWorld* World);
 void Register(ACultPopulation* Person);
 bool TryConvert(ACultPopulation* Person,int32 Faction,ACultPopulation* Agent=nullptr);
 float CombatMultiplier(int32 Faction) const;
 // Native player orders deliberately override core whole-cult obedience for combat only.
 enum EPlayerCommand { Preach=100, PrepareBattle, DisengageBattle, GlobalRetreat, ClearSelection,
  RestFollowers=200, MassConvert, PlaceTrap, TortureCaptive, SacrificeCaptive, PerformNecromancy, BreedCreatures, ImprisonCultist, PunishCultist };
 void SelectFollowers(const TArray<ACultPopulation*>& Units);
 bool IsSelected(const ACultPopulation* Unit) const;
 int32 SelectedCount() const;
 bool BattlePrepared() const;
 int32 PreparedCount() const;
 FString CommandSummary() const;
 FString PlayerOrder(int32 Action,AActor* Target,FVector Point);
 FString FaithOrder(int32 Action,AActor* Target,FVector Point);
 FString PreachAtEntity();
 FString Execute(int32 Action, AActor* Target, FVector Point);
 FString Status() const;
 FString ActivitySummary(int32 Faction=0) const;
 FString RecentContribution(int32 Faction=0) const;
 static FString CultName(int32 Faction);
 bool AtWar(int32 A,int32 B) const;
 void DeclareWar(int32 A,int32 B);
 void AdvancePopulation();
 int32 CityCount() const;
 cultulhu::EventBus& Bus(int32 Faction=0);
 cultulhu::BeliefSystem& Beliefs(int32 Faction=0);
 cultulhu::RNG& Rng(int32 Faction=0);
 bool RequestBelief(int32 In,int32 Out=-1,int32 Faction=0);
 FString BeliefReport(int32 Belief,int32 Faction=0) const;
 FString FaithSummary(int32 Faction=0) const;
 TArray<FString> FaithJournal(int32 Faction=0) const;
 float BeliefExertion(int32 Belief,int32 Faction=0) const;
 float FaithPower(int32 Faction=0) const;
 int32 FaithEventCount(int32 Event,int32 Faction=0) const;
 FString FaithAction(const FString& Action,AActor* Target,FVector Point,int32 Faction=0,ACultPopulation* Agent=nullptr);
 bool PersonHurt(ACultPopulation* Person,float Before,float Damage,int32 Attacker=-1,bool Accounted=false,bool Feral=false);
 void BuildingHurt(class ACultDestructible* Building,int32 Attacker,bool Destroyed,AActor* Agent=nullptr);
 void CombatActivity(int32 Faction,bool Melee=false);
 float CultDamage(ACultPopulation* Attacker,float Base);
 bool BeginAmbient(ACultPopulation* Person,int32 Action);
private:
 friend struct FCultSaveAccess;
 friend struct FCultArmyTestAccess;
 friend struct FCultFaithTestAccess;
 struct FState;
 FState* State_ = nullptr;
 float PopulationTime_=20;
 float CityTime_=120;
 float BotTime_=0;
 bool Ground(FVector P,FVector& Out) const;
 void NewCity();
 void SpawnRivals();
 void BuildTemples();
 void FactionOrder(int32 Faction,const FString& Verb,FVector Point,AActor* Target);
 void InitializeFaith();
 void TickFaith(float Dt);
 void OnFaithEvent(int32 Faction,const cultulhu::GameEvent& Event);
 void AdvanceAmbient(float Dt);
 void PopulateSociety();
 void TickSociety(float Dt);
 void OnFaithReward(int32 Faction,const cultulhu::GameEvent& Event,float Delta);
};
