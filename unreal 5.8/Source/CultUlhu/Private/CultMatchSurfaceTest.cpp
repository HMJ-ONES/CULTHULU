#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CultAutomationPIE.h"
#include "CultMatchSurfaces.h"
#include "CultDominationFinish.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultMatchDirector.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Components/MeshComponent.h"
#include "ProceduralMeshComponent.h"
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
 FAutomationTestBase* Test; ECultPlayMode Mode; bool Finish=false; double Began=FPlatformTime::Seconds();
 int Phase=0,ShotIndex=0,Settle=0,Segment=0,Frames=0,Actors=0,Tagged=0;
 FString Shot; TSharedFuture<FString> Written;
 TWeakObjectPtr<APlayerController> Controller; TWeakObjectPtr<ACameraActor> Overview; TWeakObjectPtr<AActor> Original;
 FHighResScreenshotConfig Previous; uint32 PreviousX=0,PreviousY=0; FTransform HeroTransform;
 struct FGeometry { TWeakObjectPtr<UMeshComponent> Mesh; FTransform Transform; ECollisionEnabled::Type Collision; bool Visible; UStaticMesh* Asset; uint32 Shape; };
 TArray<FGeometry> Geometry;
 uint32 Shape(UMeshComponent* Mesh) const {
  uint32 Hash=0; if(auto* Proc=Cast<UProceduralMeshComponent>(Mesh)) for(int I=0;I<Proc->GetNumSections();++I) if(auto* Section=Proc->GetProcMeshSection(I)) {
   for(const auto& V:Section->ProcVertexBuffer) Hash=HashCombineFast(Hash,GetTypeHash(V.Position));
   for(auto Index:Section->ProcIndexBuffer) Hash=HashCombineFast(Hash,GetTypeHash(Index));
  } return Hash;
 }
 int Switch(UWorld* W,bool New) { return Finish?CultDominationFinish::Compare(W,New):CultMatchSurfaces::Apply(W,New); }
 FString ModeName() const { if(Finish) return TEXT("DominationFinish"); return Mode==ECultPlayMode::Domination?TEXT("Domination"):TEXT("Moba"); }
 void View(int Which) {
  if(auto* PC=Controller.Get()) {
   if(Finish&&Which) {
    const FVector Sites[]={FVector(-9000,2000,6500),FVector(-18500,12500,6000),FVector(-21000,-23000,17000)};
    const FVector Goals[]={FVector(-3300,5000,900),FVector(-10500,12500,3600),FVector(0,0,1800)};
    Overview->SetActorLocation(Sites[Which-1]); Overview->SetActorRotation((Goals[Which-1]-Sites[Which-1]).Rotation());
   }
   PC->SetViewTarget(Which?Overview.Get():Original.Get()); PC->PlayerCameraManager->UpdateCamera(0);
  }
 }
 void Verify(UWorld* W,ACultUlhuPlayerPawn* P) {
  int Now=0; for(TActorIterator<AActor> It(W);It;++It) ++Now;
  Test->TestEqual(TEXT("Material comparison adds no geometry or gameplay actors"),Now,Actors);
  Test->TestTrue(TEXT("Surface treatment preserves the actual player transform and height"),HeroTransform.Equals(P->GetActorTransform(),.01f));
  for(const auto& G:Geometry) if(auto* Mesh=G.Mesh.Get()) {
   Test->TestTrue(TEXT("Surface treatment preserves mesh placement"),G.Transform.Equals(Mesh->GetComponentTransform(),.01f));
   Test->TestEqual(TEXT("Surface treatment preserves collision"),Mesh->GetCollisionEnabled(),G.Collision);
   Test->TestEqual(TEXT("Surface treatment preserves visibility"),Mesh->IsVisible(),G.Visible);
   if(auto* Static=Cast<UStaticMeshComponent>(Mesh)) Test->TestTrue(TEXT("Surface treatment preserves the original mesh"),Static->GetStaticMesh()==G.Asset);
   Test->TestEqual(TEXT("Surface treatment preserves procedural collision geometry"),Shape(Mesh),G.Shape);
  } else Test->AddError(TEXT("An arena component disappeared during the frozen comparison"));
 }
public:
 FCultMatchSurfaceObservation(FAutomationTestBase* T,ECultPlayMode M,bool Refine=false):Test(T),Mode(M),Finish(Refine) { Previous=GetHighResScreenshotConfig(); PreviousX=GScreenshotResolutionX; PreviousY=GScreenshotResolutionY; }
 ~FCultMatchSurfaceObservation() {
  GetHighResScreenshotConfig()=Previous; GScreenshotResolutionX=PreviousX; GScreenshotResolutionY=PreviousY;
  if(Finish&&Controller.IsValid()) CultDominationFinish::Compare(Controller->GetWorld(),true);
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
   auto* Stone=LoadObject<UMaterial>(nullptr,Finish?TEXT("/Game/CultUlhu/Materials/M_DominationStone_v1"):TEXT("/Game/CultUlhu/Materials/M_MatchStone")); Test->TestNotNull(TEXT("Original stone material is available"),Stone); if(!Stone) return true;
   auto* Resource=Stone->GetMaterialResource(GMaxRHIShaderPlatform);
   Test->TestTrue(TEXT("Stone has a compiled non-fallback shader map"),Resource&&Resource->GetGameThreadShaderMap()&&Resource->GetCompileErrors().IsEmpty());
   if(Resource) for(const auto& Error:Resource->GetCompileErrors()) Test->AddError(Error);
   if(Finish) {
    int Assemblies=0,Triangles=0;
    for(TActorIterator<AActor> It(W);It;++It) if(It->ActorHasTag(TEXT("CultDominationRefinement"))) {
     ++Assemblies; auto* Mesh=It->FindComponentByClass<UProceduralMeshComponent>();
     Test->TestTrue(TEXT("Architectural relief cannot add hidden movement or attack blockers"),Mesh&&Mesh->GetCollisionEnabled()==ECollisionEnabled::NoCollision&&!It->GetActorEnableCollision());
     if(Mesh) for(int S=0;S<Mesh->GetNumSections();++S) if(auto* Section=Mesh->GetProcMeshSection(S)) Triangles+=Section->ProcIndexBuffer.Num()/3;
    }
    Test->TestEqual(TEXT("Refinement uses one merged procedural assembly"),Assemblies,1);
    Test->TestTrue(TEXT("Original relief has a bounded geometry budget"),Triangles>1000&&Triangles<10000);
    Test->AddInfo(FString::Printf(TEXT("Domination relief: %d triangles, one component, three material sections, no collision"),Triangles));
   }
   Controller=PC; Original=PC->GetViewTarget(); HeroTransform=P->GetActorTransform();
   const FVector Site=Mode==ECultPlayMode::Domination?FVector(-21000,-23000,17000):FVector(-14000,-14000,9000);
   Overview=W->SpawnActor<ACameraActor>(Site,FRotator::ZeroRotator); Overview->SetActorRotation((FVector(0,0,1800)-Site).Rotation());
   PC->SetPause(true);
   for(TActorIterator<AActor> It(W);It;++It) ++Actors;
   for(TActorIterator<AActor> It(W);It;++It) if(It->ActorHasTag(TEXT("MatchArena"))) {
    TArray<UMeshComponent*> Meshes; It->GetComponents(Meshes); for(auto* C:Meshes) { auto* Static=Cast<UStaticMeshComponent>(C); Geometry.Add({C,C->GetComponentTransform(),C->GetCollisionEnabled(),C->IsVisible(),Static?Static->GetStaticMesh():nullptr,Shape(C)}); }
   }
   Tagged=Switch(W,false); Test->TestTrue(TEXT("Real arena surfaces participate in the material comparison"),Tagged>50); View(0); Settle=30; Phase=2; return false;
  }
  if(Settle>0) { --Settle; return false; }
  if(Phase==2) {
   const TCHAR* OldNames[]={TEXT("FlatSpawn"),TEXT("FlatOverview"),TEXT("StoneSpawn"),TEXT("StoneOverview")};
   const TCHAR* NewNames[]={TEXT("LegacySpawn"),TEXT("LegacyPerch"),TEXT("LegacyCrypt"),TEXT("LegacyOverview"),TEXT("RefinedSpawn"),TEXT("RefinedPerch"),TEXT("RefinedCrypt"),TEXT("RefinedOverview")};
   const TCHAR* Label=Finish?NewNames[ShotIndex]:OldNames[ShotIndex];
   Shot=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/MatchSurface_%s_%s.png"),*ModeName(),Label));
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
   const int Views=Finish?4:2;
   if(++ShotIndex<Views*2) { if(ShotIndex==Views) Test->TestEqual(TEXT("Stone switches the same surfaces"),Switch(W,true),Tagged); View(ShotIndex%Views); Settle=30; Phase=2; return false; }
   View(0); Switch(W,false); Settle=30; Phase=4; return false;
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
  if(++Segment<4) { Switch(W,Segment==1||Segment==2); Settle=30; Phase=4; return false; }
  Switch(W,true);
  Test->AddInfo(FString::Printf(TEXT("Match surface %s: %d tagged surfaces, %d arena meshes, %d actors; %d exact captures and A-B-B-A render windows in one frozen world"),*ModeName(),Tagged,Geometry.Num(),Actors,Finish?8:4)); return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultDominationSurfaceTest,"CultUlhu.Menu.MatchSurfaces.Domination",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultDominationSurfaceTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultMatchSurfaceObservation(this,ECultPlayMode::Domination)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultMobaSurfaceTest,"CultUlhu.Menu.MatchSurfaces.Moba",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultMobaSurfaceTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultMatchSurfaceObservation(this,ECultPlayMode::Dotgoo)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultDominationFinishTest,"CultUlhu.Menu.DominationFinish",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultDominationFinishTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultMatchSurfaceObservation(this,ECultPlayMode::Domination,true)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
#endif
