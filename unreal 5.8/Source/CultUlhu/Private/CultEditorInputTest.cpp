#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "Kismet2/DebuggerCommands.h"
#include "Framework/Commands/InputBindingManager.h"
#include "GenericPlatform/GenericApplication.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultSkillAttack.h"
#include "GameFramework/PlayerController.h"
#include "InputKeyEventArgs.h"
#include "Components/InputComponent.h"
#include "Kismet/GameplayStatics.h"

namespace {
void Key(APlayerController* P,FKey K,EInputEvent Event) { P->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),K,Event,Event==IE_Released?0.f:1.f,false,FPlatformTime::Cycles64())); }
bool EditorKey(FKey K,bool Alt=false) { return FPlayWorldCommands::GlobalPlayWorldActions->ProcessCommandBindings(K,FModifierKeysState(false,false,false,false,Alt,false,false,false,false),false); }
bool SimulateShortcutAllowed() { auto C=FPlayWorldCommands::Get().Simulate; return FInputBindingManager::Get().CommandPassesFilter(C->GetBindingContext(),C->GetCommandName()); }
}
class FCultEditorInputObservation:public IAutomationLatentCommand {
 FAutomationTestBase* T; int Phase=0; double Start=FPlatformTime::Seconds(); float At=0; FVector Origin,Forward; TWeakObjectPtr<ACultUlhuPlayerPawn> Pawn;
public:
 explicit FCultEditorInputObservation(FAutomationTestBase* Test):T(Test) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Start>60) { T->AddError(TEXT("Editor input regression timed out")); return true; }
  UWorld* W=nullptr; for(auto& C:GEngine->GetWorldContexts()) if(C.World()&&C.World()->IsGameWorld()) W=C.World(); if(!W) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) { if(Phase==0) { auto* GI=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); GI->SelectedCharacterId=TEXT("cthulhu_avatar"); GI->OpenCharacterSelect(ECultPlayMode::FreeRoam); GI->SetInitialBeliefs({5,6,11}); GI->StartPlay(); Phase=1; } return false; }
  auto* PC=W->GetFirstPlayerController(); if(!PC||W->GetTimeSeconds()<2) return false;
  auto* P=Cast<ACultUlhuPlayerPawn>(UGameplayStatics::GetPlayerPawn(W,0));
  if(Phase==1) {
   if(!P) return false; Pawn=P; Origin=P->FeetLocation(); Forward=FRotator(0,PC->GetControlRotation().Yaw,0).Vector();
   int Releases=0;
   if(P->InputComponent) for(const auto& Binding:P->InputComponent->KeyBindings) if(Binding.KeyEvent==IE_Released&&Binding.Chord.Key==EKeys::LeftMouseButton) ++Releases;
   T->TestEqual(TEXT("One mouse release has exactly one cult-command binding"),Releases,1);
   T->TestFalse(TEXT("Simulate shortcut is filtered during gameplay"),SimulateShortcutAllowed());
   T->TestFalse(TEXT("Plain S does not dispatch an editor command"),EditorKey(EKeys::S));
   Key(PC,EKeys::S,IE_Pressed); Key(PC,EKeys::LeftMouseButton,IE_Pressed); At=W->GetTimeSeconds(); Phase=2; return false;
  }
  if(Phase==2) {
   if(W->GetTimeSeconds()-At<.1f) return false;
   T->TestTrue(TEXT("S plus LMB starts the real melee action"),Pawn->MeleeRecovery()>0); Key(PC,EKeys::LeftMouseButton,IE_Released); Phase=3; return false;
  }
  if(Phase==3) {
   if(W->GetTimeSeconds()-At<.45f) return false;
   const float Backward=FVector::DotProduct(Pawn->FeetLocation()-Origin,-Forward);
   T->AddInfo(FString::Printf(TEXT("Backward attack travel: %.1f cm"),Backward)); T->TestTrue(TEXT("Player actually walks backward while attacking"),Backward>100);
   T->TestFalse(TEXT("Backward attack never enters SIE"),GEditor->bIsSimulatingInEditor); T->TestTrue(TEXT("Backward attack preserves possession"),PC->GetPawn()==Pawn.Get());
   Key(PC,EKeys::S,IE_Released); Key(PC,EKeys::LeftAlt,IE_Pressed); At=W->GetTimeSeconds(); Phase=4; return false;
  }
  if(Phase==4) {
   if(W->GetTimeSeconds()-At<.1f) return false;
   T->TestTrue(TEXT("ALT still releases the cult-command cursor"),PC->bShowMouseCursor);
   T->TestFalse(TEXT("The real global ALT+S dispatcher cannot enter Simulate"),EditorKey(EKeys::S,true));
   Key(PC,EKeys::S,IE_Pressed); Key(PC,EKeys::LeftMouseButton,IE_Pressed); At=W->GetTimeSeconds(); Phase=5; return false;
  }
  if(Phase==5) {
   if(W->GetTimeSeconds()-At<.15f) return false;
   Key(PC,EKeys::LeftMouseButton,IE_Released); Key(PC,EKeys::S,IE_Released); Key(PC,EKeys::LeftAlt,IE_Released); At=W->GetTimeSeconds(); Phase=6; return false;
  }
  if(Phase==6) {
   if(W->GetTimeSeconds()-At<.15f) return false;
   T->TestFalse(TEXT("ALT release restores gameplay mouse capture mode"),PC->bShowMouseCursor); T->TestFalse(TEXT("ALT+S+LMB cannot eject the player"),GEditor->bIsSimulatingInEditor); T->TestTrue(TEXT("Command selection preserves its original pawn"),PC->GetPawn()==Pawn.Get());
   T->TestTrue(TEXT("Intentional F8 editor eject remains available"),EditorKey(EKeys::F8)); Phase=7; At=W->GetTimeSeconds(); return false;
  }
  if(Phase==7) {
   if(W->GetTimeSeconds()-At<.2f) return false;
   T->TestTrue(TEXT("Explicit F8 still enters editor simulation"),GEditor->bIsSimulatingInEditor);
   T->TestTrue(TEXT("Editor shortcuts are available in intentional simulation"),SimulateShortcutAllowed());
   T->TestTrue(TEXT("F8 can return to the character"),EditorKey(EKeys::F8)); Phase=8; At=W->GetTimeSeconds(); return false;
  }
  if(Phase==8) {
   if(W->GetTimeSeconds()-At<.2f) return false;
   T->TestFalse(TEXT("F8 returns to gameplay"),GEditor->bIsSimulatingInEditor); T->TestTrue(TEXT("F8 restores the same pawn"),PC->GetPawn()==Pawn.Get()); T->TestFalse(TEXT("Shortcut protection returns after repossession"),SimulateShortcutAllowed()); return true;
  }
  return false;
 }
};
class FCultEditorShortcutRestoration:public IAutomationLatentCommand {
 FAutomationTestBase* T; FInputChord Original;
public:
 FCultEditorShortcutRestoration(FAutomationTestBase* Test,FInputChord Chord):T(Test),Original(Chord) {}
 bool Update() override {
  if(GEditor->PlayWorld) return false;
  T->TestTrue(TEXT("Ending Play restores the editor Simulate command"),SimulateShortcutAllowed());
  T->TestTrue(TEXT("User shortcut preferences remain unchanged"),*FPlayWorldCommands::Get().Simulate->GetActiveChord(EMultipleKeyBindingIndex::Primary)==Original); return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultEditorInputTest,"CultUlhu.Menu.EditorInputSafety",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultEditorInputTest::RunTest(const FString&) {
 const FInputChord Original=*FPlayWorldCommands::Get().Simulate->GetActiveChord(EMultipleKeyBindingIndex::Primary);
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false)); ADD_LATENT_AUTOMATION_COMMAND(FCultEditorInputObservation(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); ADD_LATENT_AUTOMATION_COMMAND(FCultEditorShortcutRestoration(this,Original)); return true;
}
#endif
