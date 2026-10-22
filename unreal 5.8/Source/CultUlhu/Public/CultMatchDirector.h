#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CultUlhuGameInstance.h"
#include "CultMatchDirector.generated.h"
class ACultUlhuPlayerPawn;
class ACultMatchUnit;
struct FCultMatchElimination {
 FString Attacker,Victim;
 int AttackerTeam=-1,VictimTeam=-1;
 float Remaining=6.f;
 bool Local=false;
};
struct FCultMatchFighter {
 TWeakObjectPtr<ACultUlhuPlayerPawn> Pawn;
 FString Kit,Name; int Team=0,Lane=1,XP=0,Level=1,Kills=0,Deaths=0,Assists=0,Invokes=0,FirstPick=-1,RouteStep=1;
 float Respawn=0,Shield=0,Recall=0,Invoke=0,Think=0,LastHurt=-100,LastAction=-100;
 FVector ChannelOrigin=FVector::ZeroVector; float RelicMult=1,DamageDone=0,DamageTaken=0; bool Retreating=false,LaneEntered=false; float PointSeconds=0;
 TArray<FVector> DomPath; int DomPathIndex=0; bool DomEscape=false,DomTunnelVisited=false,DomFlankCompleted=false;
 TMap<int,float> RecentAttackers;
};
UCLASS()
class CULTULHU_API ACultMatchDirector:public AActor {
 GENERATED_BODY()
public:
 ACultMatchDirector();
 virtual void BeginPlay() override;
 virtual void Tick(float Dt) override;
 virtual void EndPlay(const EEndPlayReason::Type Reason) override;
 static bool IsMatch(UWorld* World);
 static ACultMatchDirector* Find(UWorld* World);
 static void BuildArena(UWorld* World,ECultPlayMode Mode);
 ECultPlayMode Mode() const;
 bool IsOver() const;
 bool IsPaused() const;
 void TogglePause();
 int Winner() const;
 float Elapsed() const;
 float Warmup() const;
 float Score(int Team) const;
 float CoreHealth(int Team) const;
 int TowersAlive(int Team) const;
 bool CoreOpen(int Team) const;
 bool LaneWardsCleared(int Team) const;
 bool NexusAlive(int Team) const;
 FVector LaneWaypoint(int Team,int Lane,int Step) const;
 bool Contested() const;
 bool Overtime() const;
 int Holder() const;
 FVector Base(int Team) const;
 FVector Objective() const { return FVector::ZeroVector; }
 static FVector2D CaptureHalfExtent() { return FVector2D(5200,3700); }
 static bool InsideCapture(FVector Feet) {
  const auto E=CaptureHalfExtent(); return FMath::Abs(Feet.X)<=E.X&&FMath::Abs(Feet.Y)<=E.Y&&FMath::Abs(Feet.Z)<150;
 }
 const TArray<FCultMatchFighter>& Fighters() const;
 const FCultMatchFighter* Fighter(const ACultUlhuPlayerPawn* P) const;
 int Slot(const AActor* P) const;
 float RespawnFor(const ACultUlhuPlayerPawn* P) const;
 bool CanAct(const ACultUlhuPlayerPawn* P) const;
 bool AbilityAvailable(ACultUlhuPlayerPawn* P,int Slot,bool Choose=false);
 void OnAction(ACultUlhuPlayerPawn* P);
 bool DamageAllowed(ACultUlhuPlayerPawn* Victim,AActor* Source);
 void NoteDamage(ACultUlhuPlayerPawn* Victim,AActor* Source,float Actual);
 void HeroDied(ACultUlhuPlayerPawn* Victim,AActor* Source);
 float DamageMultiplier(const ACultUlhuPlayerPawn* P) const;
 void StartRecall(ACultUlhuPlayerPawn* P);
 void Interact(ACultUlhuPlayerPawn* P);
 void Ping(ACultUlhuPlayerPawn* P);
 void Rematch();
 void ClickResult();
 float HurtUnit(ACultMatchUnit* Unit,float Damage,AActor* Source);
 bool UnitProtected(const ACultMatchUnit* Unit) const;
 void GrantXP(int Slot,int Amount);
 FString Announcement() const;
 const TArray<FCultMatchElimination>& Eliminations() const;
 FString Hint(const ACultUlhuPlayerPawn* P) const;
 FString RelicStatus() const;
 void Draw(class ACultPlayHUD* HUD,class UCanvas* Canvas,ACultUlhuPlayerPawn* Player);
private:
 friend struct FCultHUDCoordinateTestAccess;
 friend struct FCultMatchTestAccess;
 friend struct FCultMobaMechanicsTestAccess;
 friend struct FCultJungleTestAccess;
 struct FState; FState* State_=nullptr;
 void SpawnFighter(int Index);
 void UpdateFighters(float Dt);
 void UpdateUnits(float Dt);
 AActor* SelectTowerTarget(ACultMatchUnit* Tower);
 void UpdateRelics(float Dt);
 void SpawnWaves();
 void Finish(int Team);
 void Announce(const FString& Text);
 void BotThink(int Index,float Dt);
 ACultMatchUnit* AddUnit(int Team,int Lane,int Kind,int Index,FVector Position,float Hp,float Dps=0,float Range=0,float Speed=0);
};
