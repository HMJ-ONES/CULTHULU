#pragma once
#include "CultWorldDirector.h"
#include "CultLivingLandscape.h"
#include "Engine/StaticMesh.h"
#include "CultPopulation.h"
#include "CultUlhuPlayerPawn.h"
#include "CultUlhuGameInstance.h"
#include "CultDestructible.h"
#include "CultFeralBeast.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "core/EventBus.h"
#include "core/GameClock.h"
#include "core/RNG.h"
#include "beliefs/BeliefSystem.h"
#include "cult/CultManager.h"
#include "commands/CommandSystem.h"
#include "commands/DirectiveExecutor.h"
#include "ai/AmbientBehavior.h"
#include "exertion/ExertionSystem.h"
#include "war/WarSystem.h"
#include "chaos/LunaticSystem.h"
#include "entities/Structures.h"
#include "ui/CommandMenu.h"
#include <memory>

#include "dreams/DreamSystem.h"
#include "breeding/BreedingSystem.h"
#include "traps/TrapSystem.h"
#include "rituals/Ritual.h"
#include "ai/BuilderAI.h"
#include "ai/RitualCaster.h"

struct ACultWorldDirector::FState
{
 struct Faith {
  cultulhu::EventBus Bus; cultulhu::GameClock Clock; cultulhu::RNG Rng;
  cultulhu::BeliefSystem Beliefs; cultulhu::CultManager Cult; cultulhu::PowerSystem Power;
  cultulhu::ExertionSystem Exertion; cultulhu::CommandSystem Commands; cultulhu::AmbientDirector Ambient; cultulhu::WarSystem Wars; cultulhu::LunaticSystem Lunatics;
  cultulhu::DreamSystem Dreams; cultulhu::BreedingSystem Breeding; cultulhu::TrapSystem Traps; cultulhu::BuilderAI Builder; cultulhu::RitualCaster RitualCaster;
  TArray<FString> Journal; TMap<int32,int32> EventCounts;
  struct Soul { TWeakObjectPtr<ACultPopulation> Actor; std::unique_ptr<cultulhu::Entity> Core; }; std::vector<Soul> Souls;
  struct Rite { std::unique_ptr<cultulhu::Ritual> Core; TWeakObjectPtr<ACultPopulation> Performer,Victim; TWeakObjectPtr<ACultDestructible> Altar; bool Sacrifice=false; float PerformerHp=0,VictimHp=0; }; std::vector<Rite> Rites;
  struct Snare { uint64 Id; FVector Point; TWeakObjectPtr<ACultDestructible> Marker; }; std::vector<Snare> Snares;
  TWeakObjectPtr<ACultPopulation> ConversionTarget;
  TSet<uint32> Tortured;
  FString LastContribution; float ContributionUntil=0;
  TWeakObjectPtr<ACultDestructible> WorkAltar; float SocietyTimer=1;
  bool Projecting=false; float WorkTimer=20,CombatRemaining=0; bool InCombat=false;
  Faith(uint64 Seed,int F):Rng(Seed),Beliefs(Bus,Clock),Cult(Bus,Clock,Rng),Exertion(Bus,Beliefs,Power,Cult,Rng),Commands(Bus,Rng,Beliefs,Cult),Ambient(Bus,Rng,Beliefs,Cult),Wars(Bus),Lunatics(Bus,Rng,Beliefs,Cult),Dreams(Bus,Rng,Beliefs,Cult),Breeding(Bus,Rng),Traps(Bus),Builder(Bus,Rng,Beliefs,Exertion,Cult),RitualCaster(Bus,Rng,Beliefs,Exertion)
  {
   using B=cultulhu::Belief;
   Beliefs.restoreActive(F==0?std::vector<B>{B::Dreams,B::Conversion}:F==1?std::vector<B>{B::War,B::Onslaught}:std::vector<B>{B::Chaos,B::Magic});
   Commands.setExternalExecution(true);
   Ambient.setExertion(&Exertion); Dreams.setExertion(&Exertion);
   Ambient.setTargetFilter([](const cultulhu::Cultist& A,const cultulhu::Cultist& B) { return A.position().distance(B.position())<=6.f; });
  }
 };
 struct Person { TWeakObjectPtr<ACultPopulation> Actor; int Faction; size_t Index; uint64 Id; float Timer=0;
  int Action=-1; float ActionTime=0; TWeakObjectPtr<AActor> Target; std::unique_ptr<cultulhu::Sorcerer> Caster;
  float RestRemaining=0,RestDue=90; bool Lost=false; };
 struct City { FVector Center; TArray<TWeakObjectPtr<ACultDestructible>> Buildings; };
 struct Build { std::unique_ptr<cultulhu::ConstructionSite> Site; FVector Point; int Faction; TArray<TWeakObjectPtr<ACultPopulation>> Workers; int LastDecile=-1; TWeakObjectPtr<AActor> Preview; };
 std::vector<std::unique_ptr<Faith>> Faiths;
 std::vector<Person> People;
 struct Labor { TWeakObjectPtr<ACultPopulation> Worker,Victim; TWeakObjectPtr<ACultDestructible> Altar; FString Kind; float Remaining=20,Elapsed=0,WorkerHp=0,VictimHp=0; }; std::vector<Labor> Labors;
 bool SocietyEnabled=true;
 std::vector<City> Cities;
 std::vector<Build> Builds;
 TArray<TWeakObjectPtr<ACultUlhuPlayerPawn>> Bots;
 TArray<FVector> Temples={FVector(-10000,-10000,0),FVector(75000,65000,0),FVector(-85000,70000,0)};
 TMap<int32,float> DecisionTimers;
 struct Repair { TWeakObjectPtr<ACultDestructible> Actor; std::unique_ptr<cultulhu::Building> Core; }; std::vector<Repair> Repairs;
 TMap<int32,FString> Goals;
 struct Convert { TWeakObjectPtr<ACultPopulation> Actor; int Faction; }; TArray<Convert> ConversionQueue;
 TArray<TWeakObjectPtr<ACultPopulation>> Selected,Prepared;
 bool PreparedActive=false;
 float NextPreach=0;
 TMap<TWeakObjectPtr<ACultPopulation>,float> SermonAudience;
 FRandomStream Random;
 FState():Random(FMath::Rand()) { for(int I=0;I<3;++I) Faiths.push_back(std::make_unique<Faith>(Random.GetUnsignedInt(),I)); }
};
