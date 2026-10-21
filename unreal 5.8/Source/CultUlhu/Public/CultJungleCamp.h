#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CultJungleCamp.generated.h"
class ACultFeralBeast;
class ACultUlhuPlayerPawn;

UCLASS()
class CULTULHU_API ACultJungleCamp:public AActor {
 GENERATED_BODY()
public:
 ACultJungleCamp();
 virtual void BeginPlay() override;
 virtual void Tick(float Dt) override;
 int CampIndex=0;
 ACultFeralBeast* Monster() const;
 ACultUlhuPlayerPawn* Target() const;
 bool Resetting() const { return Resetting_; }
 float RespawnRemaining() const { return Respawn_; }
 int RewardXP() const { return CampIndex>=2?300:180; }
 FString Title() const { return CampIndex>=2?TEXT("OSSIFIED WARDEN"):TEXT("CHARNEL HUNTER"); }
 bool DamageAllowed(AActor* Source) const;
 void Damaged(AActor* Source);
 void MonsterDied(ACultFeralBeast* Beast);
 void TickMonster(ACultFeralBeast* Beast,float Dt);
private:
 friend struct FCultJungleTestAccess;
 TWeakObjectPtr<ACultFeralBeast> Monster_;
 TWeakObjectPtr<ACultUlhuPlayerPawn> Target_;
 TWeakObjectPtr<AActor> LastSource_;
 float Respawn_=0,ResetTime_=0;
 bool Resetting_=false,RewardPaid_=false;
 void SpawnMonster();
 void StartReset();
 void CancelAttacks();
 bool Eligible(const ACultUlhuPlayerPawn* Hero,float Radius) const;
};
