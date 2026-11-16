#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultPlayHUD.h"
#include "UI/CultHomeScreenWidget.h"
#include "UI/CultCharacterSelectWidget.h"
#include "UI/CultCharacterCardWidget.h"
#include "GameFramework/PlayerController.h"
#include "EngineUtils.h"
#include "CultDestructible.h"
#include "CultPopulation.h"

namespace {
UWorld* MenuTestWorld() { for(const auto& C:GEngine->GetWorldContexts()) if(C.World()&&C.World()->IsGameWorld()) return C.World(); return nullptr; }
}
class FCultMenuSelectCheck:public IAutomationLatentCommand
{
public:
 explicit FCultMenuSelectCheck(FAutomationTestBase* InTest):Test(InTest),Started(FPlatformTime::Seconds()){}
 bool Update() override {
  if(Captured) { if(FPlatformTime::Seconds()-CaptureTime<2) return false; if(!Requested) { FScreenshotRequest::RequestScreenshot(Path,true,false); Requested=true; CaptureTime=FPlatformTime::Seconds(); return false; } Test->TestTrue(TEXT("Character-select screenshot saved"),FPaths::FileExists(Path)); return true; }
  UWorld* W=MenuTestWorld(); if(W&&W->GetName().Contains(TEXT("L_MainMenu"))) {
   auto* GI=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); if(!GI) return false;
   TArray<UUserWidget*> Widgets; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Widgets,UCultHomeScreenWidget::StaticClass(),true);
   if(Widgets.Num()==0 && FPlatformTime::Seconds()-Started<10) return false;
   Test->TestEqual(TEXT("Play opens the mode menu"),Widgets.Num(),1);
   if(!HomeShot) { FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots/PolishedModeSelect.png")),true,false); HomeShot=true; CaptureTime=FPlatformTime::Seconds(); return false; }
   if(FPlatformTime::Seconds()-CaptureTime<1.5) return false;
   GI->OpenCharacterSelect(ECultPlayMode::FreeRoam);
   UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Widgets,UCultCharacterSelectWidget::StaticClass(),true);
   Test->TestEqual(TEXT("Mode selection opens character selection"),Widgets.Num(),1);
   UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,Widgets,UCultCharacterCardWidget::StaticClass(),false);
   Test->TestEqual(TEXT("All released entity cards are present"),Widgets.Num(),UCultUlhuGameInstance::Roster().Num());
   Path=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots/CodexCharacterSelect.png")); Captured=true; CaptureTime=FPlatformTime::Seconds(); return false;
  }
  if(FPlatformTime::Seconds()-Started>20) { Test->AddError(TEXT("PIE did not reach the main menu")); return true; } return false;
 }
private: FAutomationTestBase* Test; double Started; bool Captured=false; bool HomeShot=false; bool Requested=false; double CaptureTime=0; FString Path;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultMenuSelectTest,"CultUlhu.Menu.Select",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCultMenuSelectTest::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
 ADD_LATENT_AUTOMATION_COMMAND(FCultMenuSelectCheck(this)); return true;
}
class FCultMenuEnterCheck:public IAutomationLatentCommand
{
public:
 explicit FCultMenuEnterCheck(FAutomationTestBase* InTest):Test(InTest),Started(FPlatformTime::Seconds()){}
 bool Update() override {
  if(Captured) { if(FPlatformTime::Seconds()-CaptureTime<2) return false; Test->TestTrue(TEXT("World command-menu screenshot saved"),FPaths::FileExists(Path)); return true; }
  UWorld* W=MenuTestWorld();
  if(W&&!Entered&&W->GetName().Contains(TEXT("L_MainMenu"))) { if(auto* GI=Cast<UCultUlhuGameInstance>(W->GetGameInstance())) { GI->SelectedCharacterId=TEXT("cthulhu_avatar"); GI->OpenCharacterSelect(ECultPlayMode::FreeRoam); GI->SetInitialBeliefs({11,5,6}); GI->StartPlay(); Entered=true; } return false; }
  if(W&&W->GetName().Contains(TEXT("L_DarkWilds"))) if(auto* PC=W->GetFirstPlayerController()) if(auto* P=Cast<ACultUlhuPlayerPawn>(PC->GetPawn())) {
   Test->TestEqual(TEXT("Chosen Cthulhu kit reaches the world"),P->GetMaxHealth(),500.f);
   P->AltPressed(); Test->TestTrue(TEXT("ALT releases the command cursor"),PC->bShowMouseCursor);
   if(auto* HUD=Cast<ACultPlayHUD>(PC->GetHUD())) HUD->CommandClick();
   Path=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots/CodexCommandMenu.png")); FScreenshotRequest::RequestScreenshot(Path,true,false); Captured=true; CaptureTime=FPlatformTime::Seconds(); return false;
  }
  if(FPlatformTime::Seconds()-Started>20) { Test->AddError(TEXT("Character selection did not reach the playable world")); return true; } return false;
 }
private: FAutomationTestBase* Test; double Started; bool Entered=false; bool Captured=false; double CaptureTime=0; FString Path;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultMenuEnterTest,"CultUlhu.Menu.Enter",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCultMenuEnterTest::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
 ADD_LATENT_AUTOMATION_COMMAND(FCultMenuEnterCheck(this)); return true;
}
class FCultAutonomyCheck:public IAutomationLatentCommand
{
public:
 explicit FCultAutonomyCheck(FAutomationTestBase* InTest):Test(InTest),Started(FPlatformTime::Seconds()){}
 bool Update() override {
  UWorld* W=MenuTestWorld();
  if(W&&!Entered&&W->GetName().Contains(TEXT("L_MainMenu"))) { if(auto* GI=Cast<UCultUlhuGameInstance>(W->GetGameInstance())) { GI->OpenCharacterSelect(ECultPlayMode::FreeRoam); GI->SetInitialBeliefs({11,5,6}); GI->StartPlay(); Entered=true; } return false; }
  if(!W||!W->GetName().Contains(TEXT("L_DarkWilds"))) { if(FPlatformTime::Seconds()-Started>30) { Test->AddError(TEXT("Autonomy test never reached the game world")); return true; } return false; }
  if(WorldStart<0) WorldStart=W->GetTimeSeconds();
  const float Elapsed=W->GetTimeSeconds()-WorldStart;
  if(Elapsed>12&&!VistaShot) {
   W->GetFirstPlayerController()->SetControlRotation(FRotator(-12,35,0)); VistaShot=true;
  }
  if(Elapsed>14&&!SavedVista) { FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots/LivingWildsVista.png")),true,false); SavedVista=true; }
  if(Elapsed>22&&!TempleShot) { W->GetFirstPlayerController()->SetControlRotation(FRotator(-12,-130,0)); TempleShot=true; }
  if(Elapsed>24&&!SavedTemple) { FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots/LivingWildsTemple.png")),true,false); SavedTemple=true; }
  int Builders=0,Converters=0,Built=0,NonPlayerGoals=0;
  for(TActorIterator<ACultPopulation> It(W);It;++It) if(It->GetFaction()>0) { Builders+=It->GetActivity()==TEXT("Build"); Converters+=It->GetActivity()==TEXT("Convert"); }
  for(TActorIterator<ACultDestructible> It(W);It;++It) Built+=It->ActorHasTag(TEXT("CultBuilt"))&&It->GetFaction()>0;
  for(TActorIterator<ACultUlhuPlayerPawn> It(W);It;++It) if(It->IsBot()&&It->GetBotAim()&&It->GetBotAim()!=W->GetFirstPlayerController()->GetPawn()) ++NonPlayerGoals;
  SawBuilders|=Builders>0; SawConverters|=Converters>0; SawOtherTargets|=NonPlayerGoals>0;
  if(W->GetTimeSeconds()-WorldStart<100) return false;
  Test->TestTrue(TEXT("Rivals assign cultists construction work"),SawBuilders);
  Test->TestTrue(TEXT("Rivals assign conversion work while world runs"),SawConverters);
  Test->TestTrue(TEXT("Rivals pursue goals involving actors other than the human"),SawOtherTargets);
  Test->TestTrue(TEXT("Rival cultists physically complete a new structure"),Built>0);
  Test->AddInfo(FString::Printf(TEXT("Autonomy observed for 100 game seconds: %d completed rival structures, %d active builders, %d converters"),Built,Builders,Converters));
  return true;
 }
private: FAutomationTestBase* Test; double Started; float WorldStart=-1; bool Entered=false,SawBuilders=false,SawConverters=false,SawOtherTargets=false,VistaShot=false,SavedVista=false,TempleShot=false,SavedTemple=false;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultWorldAutonomyTest,"CultUlhu.Menu.WorldAutonomy",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCultWorldAutonomyTest::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
 ADD_LATENT_AUTOMATION_COMMAND(FCultAutonomyCheck(this)); return true;
}
class FCultBeliefPanelCheck:public IAutomationLatentCommand
{
public:
 explicit FCultBeliefPanelCheck(FAutomationTestBase* InTest):Test(InTest),Started(FPlatformTime::Seconds()){}
 bool Update() override {
  UWorld* W=MenuTestWorld();
  if(Captured) { if(FPlatformTime::Seconds()-CaptureTime<3) return false; Test->TestTrue(TEXT("Belief review screenshot saved"),FPaths::FileExists(Path)); return true; }
  if(W&&!Entered&&W->GetName().Contains(TEXT("L_MainMenu"))) { if(auto* GI=Cast<UCultUlhuGameInstance>(W->GetGameInstance())) { GI->OpenCharacterSelect(ECultPlayMode::FreeRoam); GI->SetInitialBeliefs({11,5,6}); GI->StartPlay(); Entered=true; } return false; }
  if(W&&W->GetName().Contains(TEXT("L_DarkWilds"))) if(auto* PC=W->GetFirstPlayerController()) if(auto* P=Cast<ACultUlhuPlayerPawn>(PC->GetPawn())) if(auto* HUD=Cast<ACultPlayHUD>(PC->GetHUD())) {
   if(W->GetTimeSeconds()<8) return false;
   if(!HUD->FaithVisible()) P->ToggleFaith(); P->AltPressed(); Test->TestTrue(TEXT("B opens the live belief panel"),HUD->FaithVisible()); Test->TestTrue(TEXT("ALT gives the panel a real cursor"),PC->bShowMouseCursor);
   Path=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots/CultBeliefsReview.png")); FScreenshotRequest::RequestScreenshot(Path,true,false); Captured=true; CaptureTime=FPlatformTime::Seconds(); return false;
  }
  if(FPlatformTime::Seconds()-Started>25) { Test->AddError(TEXT("Belief review did not reach the playable world")); return true; } return false;
 }
private: FAutomationTestBase* Test; double Started,CaptureTime=0; bool Entered=false,Captured=false; FString Path;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultBeliefPanelTest,"CultUlhu.Menu.Beliefs",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCultBeliefPanelTest::RunTest(const FString& Parameters)
{
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f));
 ADD_LATENT_AUTOMATION_COMMAND(FCultBeliefPanelCheck(this)); return true;
}
#endif
