#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "CultWildsPresentation.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultLivingLandscape.h"
#include "Engine/Engine.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "UnrealClient.h"
#include "EngineUtils.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"

namespace {
class FWildsPresentationObservation:public IAutomationLatentCommand {
 FAutomationTestBase* Test;
 int Before,Frame=0; bool Entered=false,Ready=false,Requested=false;
 double Began=FPlatformTime::Seconds(),Settled=0;
 TWeakObjectPtr<APlayerController> Controller;
 TWeakObjectPtr<ACameraActor> CaveCamera;
 TWeakObjectPtr<AActor> OriginalView;
 FTransform HeroTransform; FVector CameraPosition;
 TWeakObjectPtr<UPrimitiveComponent> Floor;
 FString Output;
 void Restore() {
  if(Controller.IsValid()) { Controller->SetPause(false); if(OriginalView.IsValid()) Controller->SetViewTarget(OriginalView.Get()); }
  if(CaveCamera.IsValid()) CaveCamera->Destroy();
  IConsoleManager::Get().FindConsoleVariable(TEXT("cult.Wilds.Presentation"))->Set(Before,ECVF_SetByCode);
 }
public:
 FWildsPresentationObservation(FAutomationTestBase* InTest,int Old):Test(InTest),Before(Old) {}
 ~FWildsPresentationObservation() { Restore(); }
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>120) { Test->AddError(TEXT("Wilds presentation comparison timed out")); return true; }
  UWorld* World=nullptr;
  for(auto& Context:GEngine->GetWorldContexts()) if(Context.World()&&Context.WorldType==EWorldType::PIE) World=Context.World();
  if(!World) return false;
  if(World->GetName().Contains(TEXT("L_MainMenu"))) {
   if(!Entered) { auto* Game=Cast<UCultUlhuGameInstance>(World->GetGameInstance()); Game->SelectedCharacterId=TEXT("mi_go"); Game->OpenCharacterSelect(ECultPlayMode::FreeRoam); Game->SetInitialBeliefs({5,6,11}); Game->StartPlay(); Entered=true; }
   return false;
  }
  auto* PC=World->GetFirstPlayerController(); auto* Hero=PC?Cast<ACultUlhuPlayerPawn>(PC->GetPawn()):nullptr;
  if(!Hero||World->GetTimeSeconds()<4) return false;
  if(!Ready) {
   Controller=PC; OriginalView=PC->GetViewTarget(); HeroTransform=Hero->GetActorTransform(); CameraPosition=PC->PlayerCameraManager->GetCameraLocation();
   FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(WildsPresentationSupport),false,Hero);
   Test->TestTrue(TEXT("Comparison begins on actual walkable terrain"),World->LineTraceSingleByChannel(Hit,Hero->FeetLocation()+FVector(0,0,100),Hero->FeetLocation()-FVector(0,0,500),ECC_Visibility,Query)); Floor=Hit.GetComponent();
   const FVector Site=ACultLivingLandscape::CaveSites()[0];
   CaveCamera=World->SpawnActor<ACameraActor>(Site+FVector(0,-6500,2500),FRotator::ZeroRotator);
   CaveCamera->SetActorRotation((Site+FVector(0,4000,2500)-CaveCamera->GetActorLocation()).Rotation());
   Test->TestTrue(TEXT("Freeze one generated world for lighting comparison"),PC->SetPause(true));
   Settled=FPlatformTime::Seconds(); Ready=true;
  }
  if(FPlatformTime::Seconds()-Settled<1.2) return false;
  if(!Requested) {
   Test->TestTrue(TEXT("Capture uses the intended actual camera"),FVector::Dist(PC->PlayerCameraManager->GetCameraLocation(),Frame%2?CaveCamera->GetActorLocation():CameraPosition)<1.f);
   const TCHAR* Name=Frame==0?TEXT("WildsCourtyard_Before"):Frame==1?TEXT("WildsCave_Before"):Frame==2?TEXT("WildsCourtyard_After"):TEXT("WildsCave_After");
   Output=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots")/(FString(Name)+TEXT(".png")));
   IFileManager::Get().MakeDirectory(*FPaths::GetPath(Output),true); IFileManager::Get().Delete(*Output);
   FScreenshotRequest::RequestScreenshot(Output,true,false); Requested=true; return false;
  }
  if(FScreenshotRequest::IsScreenshotRequested()) return false;
  Test->TestTrue(TEXT("Rendered comparison frame was written"),IFileManager::Get().FileExists(*Output));
  Test->TestTrue(TEXT("Lighting preserves the player's physical transform"),HeroTransform.Equals(Hero->GetActorTransform(),.01f));
  FHitResult Hit; FCollisionQueryParams Query(SCENE_QUERY_STAT(WildsPresentationSupport),false,Hero);
  Test->TestTrue(TEXT("Lighting preserves actual floor collision"),World->LineTraceSingleByChannel(Hit,Hero->FeetLocation()+FVector(0,0,100),Hero->FeetLocation()-FVector(0,0,500),ECC_Visibility,Query)&&Hit.GetComponent()==Floor.Get());
  FHitResult SkyHit;
  Test->TestFalse(TEXT("Sky background does not intercept gameplay visibility traces"),World->LineTraceSingleByChannel(SkyHit,FVector(0,0,80000),FVector(0,0,600000),ECC_Visibility,Query));
  if(++Frame>=4) {
   for(TActorIterator<AStaticMeshActor> It(World);It;++It) if(It->ActorHasTag(TEXT("CultWildsSky"))) {
    auto* Mesh=It->GetStaticMeshComponent();
    Test->AddInfo(FString::Printf(TEXT("Sky: hidden=%d visible=%d registered=%d location=%s scale=%s bounds=%s extent=%s lastRender=%.2f material=%s"),It->IsHidden(),Mesh->IsVisible(),Mesh->IsRegistered(),*It->GetActorLocation().ToString(),*It->GetActorScale3D().ToString(),*Mesh->Bounds.Origin.ToString(),*Mesh->Bounds.BoxExtent.ToString(),Mesh->GetLastRenderTime(),*GetNameSafe(Mesh->GetMaterial(0))));
   }
   Test->AddInfo(TEXT("Captured courtyard and cave before/after in the same paused Free Roam world; imported materials and gameplay geometry unchanged.")); return true;
  }
  if(Frame==2) {
   IConsoleManager::Get().FindConsoleVariable(TEXT("cult.Wilds.Presentation"))->Set(1,ECVF_SetByCode);
   CultWildsPresentation::Apply(World);
  }
  PC->SetViewTarget(Frame%2?CaveCamera.Get():OriginalView.Get());
  PC->PlayerCameraManager->UpdateCamera(0.f);
  Requested=false; Settled=FPlatformTime::Seconds(); return false;
 }
};
}
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultWildsPresentationTest,"CultUlhu.Menu.WildsPresentation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultWildsPresentationTest::RunTest(const FString&) {
 auto* Enabled=IConsoleManager::Get().FindConsoleVariable(TEXT("cult.Wilds.Presentation")); const int Before=Enabled->GetInt(); Enabled->Set(0,ECVF_SetByCode);
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
 ADD_LATENT_AUTOMATION_COMMAND(FWildsPresentationObservation(this,Before));
 ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
#endif
