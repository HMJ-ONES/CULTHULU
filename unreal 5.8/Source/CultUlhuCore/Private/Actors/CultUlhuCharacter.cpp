// CULT-ULHU character implementation.
#include "Actors/CultUlhuCharacter.h"
#include "Data/CultCharacterData.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"

ACultUlhuCharacter::ACultUlhuCharacter()
{
	PrimaryActorTick.bCanEverTick = true;
	bReplicates = true;
}

void ACultUlhuCharacter::BeginPlay()
{
	Super::BeginPlay();
	if (CharacterData)
	{
		CoreDef_ = CharacterData->ToCoreDef();
		bHasCoreDef = true;
		ApplyCoreDef();
	}
	// VERIFY IN EDITOR: CharacterData assigned on every character Blueprint;
	// log a warning here if null so missing data is loud, not silent.
}

void ACultUlhuCharacter::ApplyCoreDef()
{
	// Vitals -> UE components.
	// VERIFY IN EDITOR: tune HealthComponent (if you add one) from
	// CoreDef_.maxHp; here we just seed the replicated values.
	ReplicatedHp = CoreDef_.maxHp;
	ReplicatedStamina = CoreDef_.maxStamina;

	if (UCharacterMovementComponent* Move = GetCharacterMovement())
	{
		// Core moveSpeed is m/s; UE MaxWalkSpeed is cm/s.
		Move->MaxWalkSpeed = CoreDef_.moveSpeed * 100.0f;
		// VERIFY IN EDITOR: sprint multiplier / jump Z velocity per character.
	}
}

void ACultUlhuCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	// VERIFY IN EDITOR: stamina drain/regen (core StaminaGauge) ticks here
	// on the owning client + host; replicate ReplicatedStamina at low Hz.
}

void ACultUlhuCharacter::OnRep_Vitals()
{
	// VERIFY IN EDITOR: drive health/stamina bar widgets from these.
}

void ACultUlhuCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(ACultUlhuCharacter, ReplicatedHp);
	DOREPLIFETIME(ACultUlhuCharacter, ReplicatedStamina);
}
