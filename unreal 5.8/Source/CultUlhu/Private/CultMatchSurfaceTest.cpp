#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CultAutomationPIE.h"
#include "CultMatchSurfaces.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultMatchDirector.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MaterialShared.h"
#include "ShaderCompiler.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "Slate/SceneViewport.h"
#include "HighResScreenshot.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "ProfilingDebugging/CsvProfiler.h"
CSV_DECLARE_CATEGORY_EXTERN(CultRuntime);

class FCultMatchSurfaceObservation:public IAutomationLatentCommand {
 FAutomationTestBase* Test; ECultPlayMode Mode; double Began=FPlatformTime::Seconds();
 int Phase=0,ShotIndex=0,Settle=0,Segment=0,Frames=0,Actors=0,Tagged=0;
 FString Shot; TSharedFuture<FString> Written;
 TWeakObjectPtr<APlayerController> Controller; TWeakObjectPtr<ACameraActor> Overview; TWeakObjectPtr<AActor> Original;
 FHighResScreenshotConfig Previous; uint32 PreviousX=0,PreviousY=0; FTransform HeroTransform;
 struct FGeometry { TWeakObjectPtr<UStaticMeshComponent> Mesh; FTransform Transform; ECollisionEnabled::Type Collision; bool Visible; UStaticMesh* Asset; };
 TArray<FGeometry> Geometry;
 FString ModeName() const { return Mode==ECultPlayMode::Domination?TEXT("Domination"):TEXT("Moba"); }
 void View(int Which) {
  if(auto* PC=Controller.Get()) { PC->SetViewTarget(Which?Overview.Get():Original.Get()); PC->PlayerCameraManager->UpdateCamera(0); }
 }
 void Verify(UWorld* W,ACultUlhuPlayerPawn* P) {
  int Now=0; for(TActorIterator<AActor> It(W);It;++It) ++Now;
  Test->TestEqual(TEXT("Material comparison adds no geometry or gameplay actors"),Now,Actors);
  Test->TestTrue(TEXT("Surface treatment preserves the actual player transform and height"),HeroTransform.Equals(P->GetActorTransform(),.01f));
  for(const auto& G:Geometry) if(auto* Mesh=G.Mesh.Get()) {
   Test->TestTrue(TEXT("Surface treatment preserves mesh placement"),G.Transform.Equals(Mesh->GetComponentTransform(),.01f));
   Test->TestEqual(TEXT("Surface treatment preserves collision"),Mesh->GetCollisionEnabled(),G.Collision);
   Test->TestEqual(TEXT("Surface treatment preserves visibility"),Mesh->IsVisible(),G.Visible);
   Test->TestTrue(TEXT("Surface treatment preserves the original mesh"),Mesh->GetStaticMesh()==G.Asset);
  } else Test->AddError(TEXT("An arena component disappeared during the frozen comparison"));
 }
public:
 FCultMatchSurfaceObservation(FAutomationTestBase* T,ECultPlayMode M):Test(T),Mode(M) { Previous=GetHighResScreenshotConfig(); PreviousX=GScreenshotResolutionX; PreviousY=GScreenshotResolutionY; }
 ~FCultMatchSurfaceObservation() {
  GetHighResScreenshotConfig()=Previous; GScreenshotResolutionX=PreviousX; GScreenshotResolutionY=PreviousY;
  if(auto* PC=Controller.Get()) { PC->SetPause(false); if(Original.IsValid()) PC->SetViewTarget(Original.Get()); }
  if(Overview.IsValid()) Overview->Destroy();
 }
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>180) { Test->AddError(TEXT("Match surface comparison timed out")); if(FCsvProfiler::Get()->IsCapturing()) FCsvProfiler::Get()->EndCapture(); return true; }
  auto* W=GEditor->PlayWorld.Get(); if(!W||W->bIsTearingDown) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) {
   if(!Phase) { auto* GI=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); GI->SelectedCharacterId=TEXT("mi_go"); GI->OpenCharacterSelect(Mode); GI->OpenBeliefSelect(); Phase=1; } return false;
  }
  auto* P=Cast<ACultUlhuPlayerPawn>(UGameplayStatics::GetPlayerPawn(W,0)); auto* PC=W->GetFirstPlayerController();
  if(!P||!PC||W->GetTimeSeconds()<1||(GShaderCompilingManager&&GShaderCompilingManager->IsCompiling())) return false;
  if(Phase==1) {
   auto* Stone=LoadObject<UMaterial>(nullptr,TEXT("/Game/CultUlhu/Materials/M_MatchStone")); Test->TestNotNull(TEXT("Original stone material is available"),Stone); if(!Stone) return true;
   auto* Resource=Stone->GetMaterialResource(GMaxRHIShaderPlatform);
   Test->TestTrue(TEXT("Stone has a compiled non-fallback shader map"),Resource&&Resource->GetGameThreadShaderMap()&&Resource->GetCompileErrors().IsEmpty());
   if(Resource) for(const auto& Error:Resource->GetCompileErrors()) Test->AddError(Error);
   Controller=PC; Original=PC->GetViewTarget(); HeroTransform=P->GetActorTransform();
   const FVector Site=Mode==ECultPlayMode::Domination?FVector(-21000,-23000,17000):FVector(-14000,-14000,9000);
   Overview=W->SpawnActor<ACameraActor>(Site,FRotator::ZeroRotator); Overview->SetActorRotation((FVector(0,0,1800)-Site).Rotation());
   PC->SetPause(true);
   for(TActorIterator<AActor> It(W);It;++It) ++Actors;
   for(TActorIterator<AStaticMeshActor> It(W);It;++It) if(It->ActorHasTag(TEXT("MatchArena"))) { auto* C=It->GetStaticMeshComponent(); Geometry.Add({C,C->GetComponentTransform(),C->GetCollisionEnabled(),C->IsVisible(),C->GetStaticMesh()}); }
   Tagged=CultMatchSurfaces::Apply(W,false); Test->TestTrue(TEXT("Real arena surfaces participate in the material comparison"),Tagged>50); View(0); Settle=30; Phase=2; return false;
  }
  if(Settle>0) { --Settle; return false; }
  if(Phase==2) {
   const TCHAR* Names[]={TEXT("FlatSpawn"),TEXT("FlatOverview"),TEXT("StoneSpawn"),TEXT("StoneOverview")};
   Shot=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/MatchSurface_%s_%s.png"),*ModeName(),Names[ShotIndex]));
   IFileManager::Get().Delete(*Shot); auto& Capture=GetHighResScreenshotConfig(); Capture.SetHDRCapture(false); Capture.SetMaskEnabled(false); Capture.SetForce128BitRendering(false);
   Capture.SetResolution(1280,720); Capture.SetFilename(Shot);
   if(!W->GetGameViewport()->GetGameViewport()->TakeHighResScreenShot()) { Test->AddError(TEXT("Dedicated arena capture failed")); return true; }
   Phase=3; return false;
  }
  if(Phase==3) {
   if(FScreenshotRequest::IsScreenshotRequested()) return false;
   TArray<uint8> Bytes; if(!FFileHelper::LoadFileToArray(Bytes,*Shot)||Bytes.Num()<24) return false;
   auto Size=[&](int At){return (uint32(Bytes[At])<<24)|(uint32(Bytes[At+1])<<16)|(uint32(Bytes[At+2])<<8)|Bytes[At+3];};
   Test->TestTrue(TEXT("Saved scene is an actual 1280x720 render"),Size(16)==1280&&Size(20)==720); Verify(W,P);
   if(++ShotIndex<4) { if(ShotIndex==2) Test->TestEqual(TEXT("Stone switches the same surfaces"),CultMatchSurfaces::Apply(W,true),Tagged); View(ShotIndex%2); Settle=30; Phase=2; return false; }
   View(0); CultMatchSurfaces::Apply(W,false); Settle=30; Phase=4; return false;
  }
  if(Phase==4) {
   const auto Dir=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Reports/Captures/MatchSurface")); IFileManager::Get().MakeDirectory(*Dir,true);
   FCsvProfiler::Get()->BeginCapture(-1,Dir,FString::Printf(TEXT("%s_%d.csv"),*ModeName(),Segment)); Frames=0; Phase=5; return false;
  }
  if(Phase==5) {
   CSV_CUSTOM_STAT(CultRuntime, ArenaStoneEnabled,Segment==1||Segment==2,ECsvCustomStatOp::Set);
   CSV_CUSTOM_STAT(CultRuntime, ArenaTaggedSurfaces,Tagged,ECsvCustomStatOp::Set);
   if(++Frames<120) return false;
   Written=FCsvProfiler::Get()->EndCapture(); Phase=6; return false;
  }
  if(!Written.IsReady()) return false;
  Test->TestTrue(TEXT("Frozen-scene render-cost CSV exists"),IFileManager::Get().FileSize(*Written.Get())>0); Test->AddInfo(TEXT("Match surface profile: ")+Written.Get());
  Verify(W,P);
  if(++Segment<4) { CultMatchSurfaces::Apply(W,Segment==1||Segment==2); Settle=30; Phase=4; return false; }
  CultMatchSurfaces::Apply(W,true);
  Test->AddInfo(FString::Printf(TEXT("Match surface %s: %d tagged surfaces, %d arena meshes, %d actors; four exact captures and A-B-B-A render windows in one frozen world"),*ModeName(),Tagged,Geometry.Num(),Actors)); return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultDominationSurfaceTest,"CultUlhu.Menu.MatchSurfaces.Domination",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultDominationSurfaceTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultMatchSurfaceObservation(this,ECultPlayMode::Domination)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultMobaSurfaceTest,"CultUlhu.Menu.MatchSurfaces.Moba",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultMobaSurfaceTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultMatchSurfaceObservation(this,ECultPlayMode::Dotgoo)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
#endif
