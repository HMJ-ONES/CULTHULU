#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CultAutomationPIE.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultMatchDirector.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "UI/CultHomeScreenWidget.h"
#include "UI/CultRoomMenuWidget.h"
#include "UI/CultCharacterSelectWidget.h"
#include "UI/CultCharacterCardWidget.h"
#include "InputKeyEventArgs.h"
#include "Kismet/GameplayStatics.h"

namespace {
bool ClickText(UUserWidget* Widget,const FString& Label) {
 if(!Widget||!Widget->WidgetTree) return false;
 TArray<UWidget*> Children; Widget->WidgetTree->GetAllWidgets(Children);
 for(auto* Child:Children) if(auto* Text=Cast<UTextBlock>(Child);Text&&Text->GetText().ToString()==Label) {
  for(auto* Parent=Text->GetParent();Parent;Parent=Parent->GetParent()) if(auto* Button=Cast<UButton>(Parent)) { Button->OnClicked.Broadcast(); return true; }
 }
 return false;
}
}
class FCultConsolidationFlow:public IAutomationLatentCommand {
 FAutomationTestBase* Test; int Phase=0; double Began=FPlatformTime::Seconds(); float At=0;
public:
 explicit FCultConsolidationFlow(FAutomationTestBase* T):Test(T) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>100) { Test->AddError(TEXT("Actual menu callback flow timed out")); return true; }
  auto* W=GEditor->PlayWorld.Get(); if(!W||W->bIsTearingDown) return false;
  auto* GI=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); if(!GI) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) {
   TArray<UUserWidget*> UI;
   if(Phase==0||Phase==5) {
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,UI,UCultHomeScreenWidget::StaticClass(),true); if(UI.Num()!=1) return false;
    if(Phase==5) { Test->TestFalse(TEXT("Returning home clears match pause"),UGameplayStatics::IsGamePaused(W)); return true; }
    Test->TestTrue(TEXT("Real Home Domination button opens selection"),ClickText(UI[0],TEXT("DOMINATION"))); Phase=1; return false;
   }
   if(Phase==1) {
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,UI,UCultRoomMenuWidget::StaticClass(),true); if(UI.Num()!=1) return false;
    Test->TestTrue(TEXT("Room screen opens offline practice"),ClickText(UI[0],TEXT("PRACTICE / OFFLINE"))); UI.Empty(); Phase=6; return false;
   }
   if(Phase==6) {
    UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,UI,UCultCharacterCardWidget::StaticClass(),false);
    Test->TestEqual(TEXT("Menu keeps exactly sixteen released character cards"),UI.Num(),16);
    bool Clicked=false; for(auto* Card:UI) if(ClickText(Card,TEXT("CTHULHU"))) { Clicked=true; break; }
    Test->TestTrue(TEXT("Real entity button selects Cthulhu"),Clicked); Test->TestEqual(TEXT("Chosen ID comes from the actual card callback"),GI->SelectedCharacterId,FString(TEXT("cthulhu_avatar")));
    UI.Empty(); UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,UI,UCultCharacterSelectWidget::StaticClass(),true);
    Test->TestTrue(TEXT("Actual Enter Battle button starts the chosen mode"),UI.Num()==1&&ClickText(UI[0],TEXT("ENTER BATTLE"))); Phase=2; return false;
   }
   return false;
  }
  auto* PC=W->GetFirstPlayerController(); auto* P=PC?Cast<ACultUlhuPlayerPawn>(PC->GetPawn()):nullptr; auto* Match=ACultMatchDirector::Find(W);
  if(!P||!Match) return false;
  if(Phase==2) {
   Test->TestEqual(TEXT("Menu callbacks reach a real Domination match"),Match->Mode(),ECultPlayMode::Domination);
   PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::Escape,IE_Pressed,1.f,false,FPlatformTime::Cycles64()));
   At=W->GetTimeSeconds(); Phase=3; return false;
  }
  if(Phase==3) {
   // Competitive menus must never pause the world.
   if(!Match->IsPaused()||W->GetTimeSeconds()-At<1) return false;
   Test->TestFalse(TEXT("Escape opens a local menu without pausing"),UGameplayStatics::IsGamePaused(W));
   Test->TestTrue(TEXT("World keeps advancing behind Escape menu"),W->GetTimeSeconds()-At>=1);
   PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),EKeys::Escape,IE_Released,0.f,false,FPlatformTime::Cycles64()));
   GI->ReturnToMenu(); Phase=5; return false;
  }
  return false;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultConsolidationFlowTest,"CultUlhu.Menu.ConsolidationFlow",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultConsolidationFlowTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultConsolidationFlow(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
#endif
