// CULT-ULHU player controller implementation.
#include "CultUlhuPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

// Each handler below is a wiring stub: the exact core calls depend on how
// the pawn exposes the core input model (InputManager) and ability system.
// The comments name the intended core touch-points.
// VERIFY IN EDITOR: implement the bodies, then playtest every binding
// against Docs/EnhancedInputMapping.md.

void ACultUlhuPlayerController::BeginPlay()
{
	Super::BeginPlay();
	if (UEnhancedInputLocalPlayerSubsystem* Subsys =
		ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer()))
	{
		if (MappingContext)
			Subsys->AddMappingContext(MappingContext, 0);
		// VERIFY IN EDITOR: priority 0 is fine solo; raise above UI contexts
		// if menus need to swallow input first.
	}
}

void ACultUlhuPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	UEnhancedInputComponent* EI = Cast<UEnhancedInputComponent>(InputComponent);
	if (!EI) return;
	// VERIFY IN EDITOR: confirm every BindAction below matches the action
	// assets from Docs/EnhancedInputMapping.md (value types included).

	// if (MoveAction) EI->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ACultUlhuPlayerController::OnMove);
	// if (JumpAction) EI->BindAction(JumpAction, ETriggerEvent::Started, this, &ACultUlhuPlayerController::OnJumpStarted);
	// if (SprintAction) { Started -> OnSprintStarted; Completed -> OnSprintCompleted; }
	// Q/F/R -> OnAbilityQ/F/R (Started)
	// if (InteractAction) -> OnInteract (Started)
	// if (StatsOverlayAction) -> OnStatsOverlay (Started; toggle)
	// if (AttackAction) -> OnAttack (Started; feeds ComboTracker timestamps)
	// RMB: Started -> OnHeavyAttackStarted; Completed -> OnHeavyAttackCompleted
	//   (Wave of Domination: press = cast/hold, release = drop, LMB during hold = launch)
	// if (CommandMenuAction) EI->BindAction(CommandMenuAction, ETriggerEvent::Started, this, &ACultUlhuPlayerController::OnCommandMenu);
}

void ACultUlhuPlayerController::OnMove(const FInputActionValue& Value)
{
	// Core: InputManager WASD vector -> pawn movement (2D axis -> FVector).
	(void)Value;
}

void ACultUlhuPlayerController::OnJumpStarted()
{
	// Core: InputManager jump buffer; pawn->Jump() if grounded/coyote.
}

void ACultUlhuPlayerController::OnSprintStarted()
{
	// Core: StaminaGauge drain while held; pawn MaxWalkSpeed multiplier.
}

void ACultUlhuPlayerController::OnSprintCompleted() {}
void ACultUlhuPlayerController::OnAbilityQ() {}  // SpellDef qAbility via cooldown/stamina gates
void ACultUlhuPlayerController::OnAbilityF() {}
void ACultUlhuPlayerController::OnAbilityR() {}
void ACultUlhuPlayerController::OnInteract() {} // E: nearest Interactable (altar/captive/relic/door)
void ACultUlhuPlayerController::OnStatsOverlay() {} // Tab: toggle UCultStatsPanelWidget
void ACultUlhuPlayerController::OnAttack() {}   // LMB: ComboTracker chain -> anim + damage
void ACultUlhuPlayerController::OnHeavyAttackStarted() {}   // RMB press: wave_of_domination cast / targeted stun
void ACultUlhuPlayerController::OnHeavyAttackCompleted() {} // RMB release: drop victims

void ACultUlhuPlayerController::OnCommandMenu(const FInputActionValue& Value)
{
	// Alt+LMB chord: raycast from click point, build MenuContext, open
	// UCultCommandMenuWidget at the screen position.
	(void)Value;
}
