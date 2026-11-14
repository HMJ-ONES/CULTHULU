#pragma once
#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "CultReplicatedBody.generated.h"

// Server owns health, AI and combat; clients receive presentation state.
UCLASS()
class CULTULHU_API UCultReplicatedBody : public UActorComponent {
 GENERATED_BODY()
public:
 UCultReplicatedBody();
 virtual void TickComponent(float Dt,ELevelTick Type,FActorComponentTickFunction* Function) override;
 virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;
 UFUNCTION(NetMulticast,Reliable) void LeaveCorpse();
private:
 UPROPERTY(ReplicatedUsing=OnRep_State) TArray<uint8> State_;
 UFUNCTION() void OnRep_State();
 bool Pending_=false;
};
