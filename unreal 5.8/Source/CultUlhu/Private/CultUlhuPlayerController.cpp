// CULT-ULHU player controller implementation.
#include "CultUlhuPlayerController.h"
#include "CultUlhuGameInstance.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"

#include "CultUlhuPlayerPawn.h"
#include "InputAction.h"

void ACultUlhuPlayerController::BeginPlay()
{
	Super::BeginPlay();
 if(IsLocalController()&&GetNetMode()==NM_Client) if(auto* G=Cast<UCultUlhuGameInstance>(GetGameInstance())) ServerChooseFreeRoamCharacter(G->SelectedCharacterId);
	if (IsLocalController() && !GetWorld()->GetName().Contains(TEXT("L_MainMenu")))
	{
		FInputModeGameOnly Mode;
		Mode.SetConsumeCaptureMouseDown(false);
		SetInputMode(Mode);
		bShowMouseCursor = false;
		ResetIgnoreLookInput();
		ResetIgnoreMoveInput();
	}
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
 const TPair<UInputAction*,ECultPlayerInput> Actions[]={
  {JumpAction,ECultPlayerInput::Jump},{AbilityQAction,ECultPlayerInput::Q},
  {AbilityFAction,ECultPlayerInput::F},{AbilityRAction,ECultPlayerInput::R},
  {InteractAction,ECultPlayerInput::Interact},{StatsOverlayAction,ECultPlayerInput::Stats},
  {AttackAction,ECultPlayerInput::Primary},{HeavyAttackAction,ECultPlayerInput::Heavy},
  {CommandMenuAction,ECultPlayerInput::Command}};
 for(const auto& Entry:Actions) if(Entry.Key) {
  EI->BindAction(Entry.Key,ETriggerEvent::Started,this,&ACultUlhuPlayerController::OnActionStarted,Entry.Value);
  EI->BindAction(Entry.Key,ETriggerEvent::Completed,this,&ACultUlhuPlayerController::OnActionCompleted,Entry.Value);
  EI->BindAction(Entry.Key,ETriggerEvent::Canceled,this,&ACultUlhuPlayerController::OnActionCompleted,Entry.Value);
 }
 if(MoveAction) {
  EI->BindAction(MoveAction,ETriggerEvent::Triggered,this,&ACultUlhuPlayerController::OnMove);
  EI->BindAction(MoveAction,ETriggerEvent::Completed,this,&ACultUlhuPlayerController::OnMoveCompleted);
  EI->BindAction(MoveAction,ETriggerEvent::Canceled,this,&ACultUlhuPlayerController::OnMoveCompleted);
 }
 if(SprintAction) {
  EI->BindAction(SprintAction,ETriggerEvent::Started,this,&ACultUlhuPlayerController::OnSprintStarted);
  EI->BindAction(SprintAction,ETriggerEvent::Completed,this,&ACultUlhuPlayerController::OnSprintCompleted);
  EI->BindAction(SprintAction,ETriggerEvent::Canceled,this,&ACultUlhuPlayerController::OnSprintCompleted);
 }
}
void ACultUlhuPlayerController::OnActionStarted(ECultPlayerInput Input) {
 if(auto* P=Cast<ACultUlhuPlayerPawn>(GetPawn())) P->RoutePlayerInput(Input,true);
}
void ACultUlhuPlayerController::OnActionCompleted(ECultPlayerInput Input) {
 if(auto* P=Cast<ACultUlhuPlayerPawn>(GetPawn())) P->RoutePlayerInput(Input,false);
}
void ACultUlhuPlayerController::OnMove(const FInputActionValue& Value) {
 if(auto* P=Cast<ACultUlhuPlayerPawn>(GetPawn())) P->SetEnhancedMove(Value.Get<FVector2D>().GetClampedToMaxSize(1));
}
void ACultUlhuPlayerController::OnMoveCompleted() {
 if(auto* P=Cast<ACultUlhuPlayerPawn>(GetPawn())) P->SetEnhancedMove(FVector2D::ZeroVector);
}
void ACultUlhuPlayerController::OnSprintStarted() {
 if(auto* P=Cast<ACultUlhuPlayerPawn>(GetPawn())) P->SetEnhancedSprint(true);
}
void ACultUlhuPlayerController::OnSprintCompleted() {
 if(auto* P=Cast<ACultUlhuPlayerPawn>(GetPawn())) P->SetEnhancedSprint(false);
}
