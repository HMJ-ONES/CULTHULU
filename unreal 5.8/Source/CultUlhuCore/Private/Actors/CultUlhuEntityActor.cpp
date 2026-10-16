// CULT-ULHU entity actor implementation.
#include "Actors/CultUlhuEntityActor.h"
#include "Net/UnrealNetwork.h"

ACultUlhuEntityActor::ACultUlhuEntityActor()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
	// VERIFY IN EDITOR: replication settings suit your net update budget;
	// consider SetReplicateMovement(true) for simple movers.
}

void ACultUlhuEntityActor::PossessCoreEntity(std::unique_ptr<cultulhu::Entity> CoreEntity)
{
	CoreEntity_ = std::move(CoreEntity);
	if (CoreEntity_)
	{
		SetActorLocation(ToUnreal(CoreEntity_->position()));
		ReplicatedHp = CoreEntity_->hp();
	}
}

void ACultUlhuEntityActor::BeginPlay()
{
	Super::BeginPlay();
}

void ACultUlhuEntityActor::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!CoreEntity_) return;

	if (HasAuthority())
	{
		// Mirror core -> actor. (The core itself is ticked by the GameMode
		// in dependency order; see ACultUlhuGameMode.)
		SetActorLocation(ToUnreal(CoreEntity_->position()));
		const float Hp = CoreEntity_->hp();
		if (!FMath::IsNearlyEqual(Hp, ReplicatedHp))
			ReplicatedHp = Hp;
	}
}

void ACultUlhuEntityActor::OnRep_CoreHp()
{
	// VERIFY IN EDITOR: hook death VFX / health bar updates here.
}

void ACultUlhuEntityActor::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACultUlhuEntityActor, ReplicatedHp);
}
