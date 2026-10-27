// CULT-ULHU player controller: Enhanced Input -> core InputManager.
//
// Binds the owner's full control scheme (Docs/EnhancedInputMapping.md):
// WASD move, Space jump, Shift sprint, Q/F/R abilities, E interact,
// Tab stats overlay, LMB melee combo, RMB heavy attack, held ALT command selection.
//
// The Input Actions + Mapping Context are editor assets; this class only
// holds references and forwards to the core input model + game systems.
// VERIFY IN EDITOR: assign every UInputAction* below in the Blueprint
// defaults, and add the Mapping Context in BeginPlay order.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "CultUlhuPlayerController.generated.h"

enum class ECultPlayerInput : uint8;
class UInputAction;
class UInputMappingContext;

UCLASS(Blueprintable)
class CULTULHU_API ACultUlhuPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	// Optional Enhanced Input assets. Keyboard/mouse work without these references.
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
 void OnActionStarted(ECultPlayerInput Input);
 void OnActionCompleted(ECultPlayerInput Input);
 void OnMove(const struct FInputActionValue& Value);
 void OnMoveCompleted();
 void OnSprintStarted();
 void OnSprintCompleted();
};
