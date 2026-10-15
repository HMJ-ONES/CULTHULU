// CULT-ULHU: playable / AI humanoid character.
//
// ACharacter driven by a core CharacterDef (stats, ability kit, combo,
// RMB). Movement uses the standard CharacterMovementComponent; the core
// InputManager/StaminaGauge logic feeds it (see Docs/EnhancedInputMapping).
//
// Core mapping: src/characters/CharacterDef.h, src/characters/CharacterRegistry.h,
// src/input/InputManager.h, src/combat/Combos.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "characters/CharacterDef.h"

#include "CultUlhuCharacter.generated.h"

class UCultCharacterData;

UCLASS(Blueprintable)
class CULTULHUCORE_API ACultUlhuCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ACultUlhuCharacter();

	// Data asset produced by the character pipeline (Docs/CharacterPipeline.md).
	// VERIFY IN EDITOR: assign per character Blueprint (Cthulhu Avatar, ...).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu")
	UCultCharacterData* CharacterData = nullptr;

	// Resolved core def (copied from the registry at BeginPlay).
	const cultulhu::CharacterDef& CoreDef() const { return CoreDef_; }
	bool HasCoreDef() const { return bHasCoreDef; }

	// ---- Replicated vitals (host sim -> clients) ----
	UPROPERTY(ReplicatedUsing = OnRep_Vitals)
	float ReplicatedHp = 0.0f;

	UPROPERTY(Replicated)
	float ReplicatedStamina = 0.0f;

	UFUNCTION()
	void OnRep_Vitals();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

	// Applies CharacterDef vitals/locomotion to the UE components.
	void ApplyCoreDef();

private:
	cultulhu::CharacterDef CoreDef_;
	bool bHasCoreDef = false;
};
