#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "Engine/Engine.h"
#include "CultUlhuGameInstance.h"
#include "UI/CultBeliefSelectWidget.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
class FCultBeliefPickerObservation:public IAutomationLatentCommand {
public:
 explicit FCultBeliefPickerObservation(FAutomationTestBase* T):Test(T),Started(FPlatformTime::Seconds()) {}
 bool Update() override {
  UWorld* W=nullptr; for(auto& C:GEngine->GetWorldContexts()) if(C.World()&&C.World()->IsGameWorld()) W=C.World();
  if(!W) { if(FPlatformTime::Seconds()-Started>30) { Test->AddError(TEXT("No PIE world for belief picker")); return true; } return false; }
  auto* GI=Cast<UCultUlhuGameInstance>(W->GetGameInstance());
  if(!Opened) {
   // Every roster entity must pass through fresh choices, including a return from a prior selection.
   for(const auto& Entity:UCultUlhuGameInstance::Roster()) {
    GI->SelectedCharacterId=Entity.Id; GI->OpenCharacterSelect(ECultPlayMode::FreeRoam); GI->SetInitialBeliefs({1,5,6}); GI->OpenBeliefSelect();
    Test->TestEqual(TEXT("Every entity gets a fresh belief selection"),GI->InitialBeliefs().Num(),0);
    TArray<UUserWidget*> Widgets; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Widgets,UCultBeliefSelectWidget::StaticClass(),true);
    Test->TestEqual(TEXT("One belief picker is visible"),Widgets.Num(),1);
   }
   TArray<UUserWidget*> Widgets; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Widgets,UCultBeliefSelectWidget::StaticClass(),true);
   if(Widgets.Num()) { auto* Picker=Cast<UCultBeliefSelectWidget>(Widgets[0]); Picker->ToggleChoice(1); Picker->ToggleChoice(5); Picker->ToggleChoice(6); Test->TestTrue(TEXT("Three selected cards enable entry"),Picker->CanEnter()); }
   Opened=true; At=FPlatformTime::Seconds(); return false;
  }
  if(FPlatformTime::Seconds()-At<2) return false;
  const FString Path=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots/InitialBeliefs.png"));
  if(!Captured) { FScreenshotRequest::RequestScreenshot(Path,true,false); Captured=true; At=FPlatformTime::Seconds(); return false; }
  Test->TestTrue(TEXT("Initial belief screen screenshot saved"),FPaths::FileExists(Path)); return true;
 }
private: FAutomationTestBase* Test; double Started,At=0; bool Opened=false,Captured=false;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultBeliefPickerTest,"CultUlhu.Menu.InitialBeliefs",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultBeliefPickerTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false)); ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1)); ADD_LATENT_AUTOMATION_COMMAND(FCultBeliefPickerObservation(this)); return true; }
#endif
