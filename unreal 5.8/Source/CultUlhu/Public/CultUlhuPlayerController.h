// CULT-ULHU player controller: Enhanced Input -> core InputManager.
//
// Binds the owner's full control scheme (Docs/EnhancedInputMapping.md):
// WASD move, Space jump, Shift sprint, Q/F/R abilities, E interact,
// Tab stats overlay, LMB melee combo, RMB heavy attack, Alt+LMB command menu.
//
// The Input Actions + Mapping Context are editor assets; this class only
// holds references and forwards to the core input model + game systems.
// VERIFY IN EDITOR: assign every UInputAction* below in the Blueprint
// defaults, and add the Mapping Context in BeginPlay order.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "CultUlhuPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;

UCLASS(Blueprintable)
class CULTULHU_API ACultUlhuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// VERIFY IN EDITOR: assign all actions in the Blueprint defaults.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu|Input")
	UInputAction* MoveAction = nullptr;        // WASD, 2D axis

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu|Input")
	UInputAction* JumpAction = nullptr;        // Space

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu|Input")
	UInputAction* SprintAction = nullptr;      // Shift (hold)

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu|Input")
	UInputAction* AbilityQAction = nullptr;    // Q

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu|Input")
	UInputAction* AbilityFAction = nullptr;    // F

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu|Input")
	UInputAction* AbilityRAction = nullptr;    // R

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu|Input")
	UInputAction* InteractAction = nullptr;    // E

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu|Input")
	UInputAction* StatsOverlayAction = nullptr; // Tab

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu|Input")
	UInputAction* AttackAction = nullptr;      // LMB (melee combo chain)

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu|Input")
	UInputAction* HeavyAttackAction = nullptr; // RMB (per-character kit)

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu|Input")
	UInputAction* CommandMenuAction = nullptr; // Alt+LMB chord

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu|Input")
	UInputMappingContext* MappingContext = nullptr;

	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;

private:
	// Handlers forward into the core InputManager / game systems.
	// VERIFY IN EDITOR: implement each handler body (see the .cpp stubs and
	// Docs/EnhancedInputMapping.md for the intended wiring).
	void OnMove(const struct FInputActionValue& Value);
	void OnJumpStarted();
	void OnSprintStarted();
	void OnSprintCompleted();
	void OnAbilityQ();
	void OnAbilityF();
	void OnAbilityR();
	void OnInteract();
	void OnStatsOverlay();
	void OnAttack();
	void OnHeavyAttackStarted();
	void OnHeavyAttackCompleted();
	void OnCommandMenu(const struct FInputActionValue& Value);
};
