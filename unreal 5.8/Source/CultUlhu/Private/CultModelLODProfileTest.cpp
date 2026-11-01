#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CultAutomationPIE.h"
#include "CultCharacterModel.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/UserWidget.h"
#include "ProfilingDebugging/CsvProfiler.h"
#include "ShaderCompiler.h"
#include "Misc/Paths.h"
#include "HAL/FileManager.h"

// One unchanged source model and view: original LOD0 / used LOD1 / LOD1 / LOD0.
// This measures a small Editor scene, never a full-match or low-spec FPS claim.
class FCultModelLODProfile:public IAutomationLatentCommand {
 FAutomationTestBase* Test; int Phase=0,Segment=0,Frames=0,Settle=0;
 double Began=FPlatformTime::Seconds(); ACultCharacterModel* Model=nullptr;
 ACameraActor* Camera=nullptr; AActor* Stage=nullptr; TSharedFuture<FString> Written;
public:
 explicit FCultModelLODProfile(FAutomationTestBase* T):Test(T) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>180) { Test->AddError(TEXT("Character LOD profile timed out")); if(FCsvProfiler::Get()->IsCapturing()) FCsvProfiler::Get()->EndCapture(); return true; }
  auto* W=GEditor->PlayWorld.Get(); if(!W||W->bIsTearingDown) return false;
  auto* PC=W->GetFirstPlayerController(); if(!PC||W->GetTimeSeconds()<.5f) return false;
  if(Phase==0) {
   TArray<UUserWidget*> UI; UWidgetBlueprintLibrary::GetAllWidgetsOfClass(W,UI,UUserWidget::StaticClass(),true); for(auto* Widget:UI) Widget->RemoveFromParent();
   Model=W->SpawnActor<ACultCharacterModel>(); Test->TestTrue(TEXT("Profile uses the actual approved Cthulhu model"),Model->Initialize(TEXT("cthulhu_avatar"),true));
   Model->SetActorLocation(FVector(0,0,1000000)); Model->SetActorRotation(FRotator(0,-35,0));
   Camera=W->SpawnActor<ACameraActor>(FVector(410,0,1000120),FRotator(-4,180,0)); Camera->GetCameraComponent()->FieldOfView=50; PC->SetViewTarget(Camera);
   Stage=W->SpawnActor<AActor>(); auto* Root=NewObject<USceneComponent>(Stage); Stage->SetRootComponent(Root); Root->RegisterComponent(); Stage->SetActorLocation(Model->GetActorLocation());
   auto* Light=NewObject<UPointLightComponent>(Stage); Light->SetupAttachment(Root); Light->RegisterComponent(); Light->SetRelativeLocation(FVector(180,-170,240)); Light->SetIntensity(50000); Light->SetAttenuationRadius(1500); Light->SetCastShadows(false);
   Phase=1; return false;
  }
  if(GShaderCompilingManager&&GShaderCompilingManager->IsCompiling()) return false;
  if(Phase==1) {
   const int Forced=(Segment==1||Segment==2)?2:1;
   for(UMeshComponent* Part:Model->Parts()) if(auto* Static=Cast<UStaticMeshComponent>(Part)) Static->SetForcedLodModel(Forced);
   Settle=30; Phase=2; return false;
  }
  if(Phase==2) {
   if(Settle-->0) return false;
   const auto Directory=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Consolidation20261027/LODProfile")); IFileManager::Get().MakeDirectory(*Directory,true);
   FCsvProfiler::Get()->BeginCapture(-1,Directory,FString::Printf(TEXT("Cthulhu_%d.csv"),Segment)); Frames=0; Phase=3; return false;
  }
  if(Phase==3) { if(++Frames<120) return false; Written=FCsvProfiler::Get()->EndCapture(); Phase=4; return false; }
  if(!Written.IsReady()) return false;
  Test->TestTrue(TEXT("Actual render frame-time CSV exists"),IFileManager::Get().FileSize(*Written.Get())>0);
  Test->AddInfo(TEXT("Character LOD profile: ")+Written.Get());
  if(++Segment<4) { Phase=1; return false; }
  Model->Destroy(); Camera->Destroy(); Stage->Destroy(); return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultModelLODProfileTest,"CultUlhu.Art.LODRenderProfile",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultModelLODProfileTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultModelLODProfile(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
#endif
