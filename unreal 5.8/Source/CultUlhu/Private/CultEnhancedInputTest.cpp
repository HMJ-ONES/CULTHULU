#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CultAutomationPIE.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultUlhuPlayerController.h"
#include "CultMatchDirector.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "InputKeyEventArgs.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace {
void Physical(APlayerController* PC,FKey Key,EInputEvent Event) {
 PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),Key,Event,Event==IE_Released?0.f:1.f,false,FPlatformTime::Cycles64()));
}
}
class FCultEnhancedInputObservation:public IAutomationLatentCommand {
 FAutomationTestBase* Test; int Phase=0; double Began=FPlatformTime::Seconds(); float At=0;
 FVector Origin; bool InitialStats=false;
public:
 explicit FCultEnhancedInputObservation(FAutomationTestBase* T):Test(T) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>100) { Test->AddError(TEXT("Enhanced Input verification timed out")); return true; }
  auto* W=GEditor->PlayWorld.Get(); if(!W||W->bIsTearingDown) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) {
   if(!Phase) { auto* G=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); G->SelectedCharacterId=TEXT("cthulhu_avatar"); G->OpenCharacterSelect(ECultPlayMode::Domination); G->StartPlay(); Phase=1; }
   return false;
  }
  auto* PC=Cast<ACultUlhuPlayerController>(W->GetFirstPlayerController());
  auto* P=PC?Cast<ACultUlhuPlayerPawn>(PC->GetPawn()):nullptr;
  if(!P||W->GetTimeSeconds()<1) return false;
  auto* Subsystem=ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer());
  if(!Subsystem) { Test->AddError(TEXT("No Enhanced Input local subsystem")); return true; }
  if(Phase==1) {
   UInputAction** Actions[]={&PC->MoveAction,&PC->JumpAction,&PC->SprintAction,&PC->AbilityQAction,&PC->AbilityFAction,&PC->AbilityRAction,&PC->InteractAction,&PC->StatsOverlayAction,&PC->AttackAction,&PC->HeavyAttackAction,&PC->CommandMenuAction};
   for(auto** Action:Actions) { *Action=NewObject<UInputAction>(PC); (*Action)->ValueType=EInputActionValueType::Boolean; }
   PC->MoveAction->ValueType=EInputActionValueType::Axis2D;
   PC->MappingContext=NewObject<UInputMappingContext>(PC);
   PC->MappingContext->MapKey(PC->StatsOverlayAction,EKeys::Tab);
   PC->MappingContext->MapKey(PC->MoveAction,EKeys::Gamepad_Left2D);
   PC->MappingContext->MapKey(PC->SprintAction,EKeys::Gamepad_LeftThumbstick);
   Subsystem->AddMappingContext(PC->MappingContext,1);
   PC->SetupInputComponent(); auto* EI=Cast<UEnhancedInputComponent>(PC->InputComponent);
   Test->TestNotNull(TEXT("Controller owns a real Enhanced Input component"),EI);
   if(!EI) return true;
   Test->TestEqual(TEXT("All eleven optional actions bind start/value and completed/canceled events"),EI->GetActionEventBindings().Num(),33);
   InitialStats=P->StatsVisible(); Physical(PC,EKeys::Tab,IE_Pressed);
   Subsystem->InjectInputForAction(PC->StatsOverlayAction,FInputActionValue(true),{},{});
   At=W->GetTimeSeconds(); Phase=2; return false;
  }
  if(Phase==2) {
   if(W->GetTimeSeconds()-At<.15f) return false;
   Test->TestEqual(TEXT("Legacy key plus Enhanced action toggles exactly once"),P->StatsVisible(),!InitialStats);
   Physical(PC,EKeys::Tab,IE_Released); At=W->GetTimeSeconds(); Phase=3; return false;
  }
  if(Phase==3) {
   if(W->GetTimeSeconds()-At<.1f) return false;
   Subsystem->InjectInputForAction(PC->StatsOverlayAction,FInputActionValue(true),{},{});
   At=W->GetTimeSeconds(); Phase=4; return false;
  }
  if(Phase==4) {
   if(W->GetTimeSeconds()-At<.15f) return false;
   // The actual match intentionally blocks movement during preparation.
   if(auto* Match=ACultMatchDirector::Find(W);Match&&!Match->CanAct(P)) return false;
   Test->TestEqual(TEXT("Enhanced-only input reaches the same actual stats action"),P->StatsVisible(),InitialStats);
   Origin=P->GetActorLocation(); At=W->GetTimeSeconds(); Phase=5; return false;
  }
  if(Phase==5) {
   Subsystem->InjectInputForAction(PC->MoveAction,FInputActionValue(FVector2D(0,1)),{},{});
   Subsystem->InjectInputForAction(PC->SprintAction,FInputActionValue(true),{},{});
   if(W->GetTimeSeconds()-At<.4f) return false;
   Test->AddInfo(FString::Printf(TEXT("Enhanced movement advanced %.1f cm after match preparation"),FVector::Dist2D(Origin,P->GetActorLocation())));
   Test->TestTrue(TEXT("Enhanced movement physically advances the canonical pawn"),FVector::Dist2D(Origin,P->GetActorLocation())>50);
   Test->TestEqual(TEXT("Holding the Enhanced sprint action never starts a dash"),P->CombatHUD().DashCooldown,0.f);
   Subsystem->InjectInputForAction(PC->MoveAction,FInputActionValue(FVector2D::ZeroVector),{},{});
   Subsystem->InjectInputForAction(PC->SprintAction,FInputActionValue(false),{},{});
   At=W->GetTimeSeconds(); Phase=6; return false;
  }
  if(W->GetTimeSeconds()-At<.6f) return false;
  Test->TestEqual(TEXT("A long sprint release never produces a tap dash"),P->CombatHUD().DashCooldown,0.f);
  Test->TestTrue(TEXT("Completed movement clears its input and lets the pawn stop"),P->GetVelocity().Size2D()<20);
  return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultEnhancedInputTest,"CultUlhu.Input.EnhancedAndFallback",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultEnhancedInputTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultEnhancedInputObservation(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
#endif
