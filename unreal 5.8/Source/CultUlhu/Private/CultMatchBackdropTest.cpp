#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CultAutomationPIE.h"
#include "CultMatchBackdrop.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "ProceduralMeshComponent.h"
#include "Components/MeshComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "EngineUtils.h"
#include "Engine/GameViewportClient.h"
#include "Slate/SceneViewport.h"
#include "HighResScreenshot.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ShaderCompiler.h"

class FCultMatchBackdropObservation:public IAutomationLatentCommand {
 FAutomationTestBase* Test; ECultPlayMode Mode; int Phase=0,Index=0,Settle=0,Previous=1; double Began=FPlatformTime::Seconds();
 TWeakObjectPtr<ACameraActor> Camera; TWeakObjectPtr<AActor> Original; TWeakObjectPtr<APlayerController> Controller;
 FHighResScreenshotConfig OldCapture; uint32 OldX=0,OldY=0; FString Shot;
 struct FBody { TWeakObjectPtr<UMeshComponent> Mesh; FTransform Transform; ECollisionEnabled::Type Collision; bool Visible; };
 TArray<FBody> Bodies;
 bool Moba() const { return Mode==ECultPlayMode::Dotgoo; }
 FString Label() const { return Moba()?TEXT("Moba"):TEXT("Domination"); }
 void Verify() {
  for(const auto& B:Bodies) if(auto* C=B.Mesh.Get()) {
   Test->TestTrue(FString::Printf(TEXT("Backdrop preserves mesh transform %s; before %s, after %s"),*C->GetPathName(),*B.Transform.ToHumanReadableString(),*C->GetComponentTransform().ToHumanReadableString()),C->GetComponentTransform().Equals(B.Transform,.01f));
   Test->TestEqual(TEXT("Backdrop preserves playable collision"),C->GetCollisionEnabled(),B.Collision);
   Test->TestEqual(TEXT("Backdrop preserves playable visibility"),C->IsVisible(),B.Visible);
  } else Test->AddError(TEXT("Playable geometry disappeared during backdrop comparison"));
 }
 void View(UWorld* W) {
  auto* PC=W->GetFirstPlayerController();
  if(Index%2==0) PC->SetViewTarget(Original.Get());
  else {
   const FVector Site=Moba()?FVector(-14000,-18000,12500):FVector(-21000,-23000,17000);
   Camera->SetActorLocation(Site); Camera->SetActorRotation((FVector(0,16000,5000)-Site).Rotation()); PC->SetViewTarget(Camera.Get());
  }
  PC->PlayerCameraManager->UpdateCamera(0);
 }
public:
 FCultMatchBackdropObservation(FAutomationTestBase* T,ECultPlayMode M):Test(T),Mode(M) {
  OldCapture=GetHighResScreenshotConfig(); OldX=GScreenshotResolutionX; OldY=GScreenshotResolutionY;
  auto* V=IConsoleManager::Get().FindConsoleVariable(TEXT("cult.Match.Backdrop")); Previous=V->GetInt(); V->Set(1,ECVF_SetByCode);
 }
 ~FCultMatchBackdropObservation() {
  GetHighResScreenshotConfig()=OldCapture; GScreenshotResolutionX=OldX; GScreenshotResolutionY=OldY;
  IConsoleManager::Get().FindConsoleVariable(TEXT("cult.Match.Backdrop"))->Set(Previous,ECVF_SetByCode);
  if(auto* PC=Controller.Get()) { PC->SetPause(false); if(Original.IsValid()) PC->SetViewTarget(Original.Get()); }
  if(Camera.IsValid()) Camera->Destroy();
 }
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>150) { Test->AddError(TEXT("Backdrop observation timed out")); return true; }
  auto* W=GEditor->PlayWorld.Get(); if(!W||W->bIsTearingDown) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) {
   if(!Phase) { auto* GI=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); GI->SelectedCharacterId=TEXT("mi_go"); GI->OpenCharacterSelect(Mode); GI->OpenBeliefSelect(); Phase=1; } return false;
  }
  auto* PC=W->GetFirstPlayerController(); auto* P=Cast<ACultUlhuPlayerPawn>(UGameplayStatics::GetPlayerPawn(W,0));
  if(!PC||!P||W->GetTimeSeconds()<1||(GShaderCompilingManager&&GShaderCompilingManager->IsCompiling())) return false;
  if(Phase==1) {
   Controller=PC; Original=PC->GetViewTarget(); Camera=W->SpawnActor<ACameraActor>(); Camera->GetCameraComponent()->SetConstraintAspectRatio(false); PC->SetPause(true);
   int Ranges=0,Legacy=0,Triangles=0; float Low=FLT_MAX,High=-FLT_MAX;
   const FVector2D Radius=Moba()?FVector2D(58000,40000):FVector2D(50000,48000);
   for(TActorIterator<AActor> It(W);It;++It) {
    // This fixture deliberately moves its own editor camera proxy. Retain
    // every real arena/hero/objective mesh in the preservation snapshot.
    if(*It==Camera.Get()) continue;
    if(It->ActorHasTag(TEXT("CultMatchBackdropLegacy"))) ++Legacy;
    if(It->ActorHasTag(TEXT("CultMatchBackdropRange"))) {
     ++Ranges; auto* Mesh=It->FindComponentByClass<UProceduralMeshComponent>(); Test->TestNotNull(TEXT("Production range has procedural geometry"),Mesh); if(!Mesh) return true;
     Test->TestEqual(TEXT("Exterior range never adds gameplay collision"),Mesh->GetCollisionEnabled(),ECollisionEnabled::NoCollision);
     Test->TestFalse(TEXT("Exterior mountains do not cast gameplay-space shadows"),Mesh->CastShadow);
     Test->TestEqual(TEXT("Five exterior strata are present"),Mesh->GetNumSections(),5);
     for(int S=0;S<Mesh->GetNumSections();++S) {
      auto* Section=Mesh->GetProcMeshSection(S); Triangles+=Section->ProcIndexBuffer.Num()/3;
      if(S==0) for(const auto& V:Section->ProcVertexBuffer) {
       const FVector2D Inward(-V.Position.X,-V.Position.Y);
       Test->TestTrue(TEXT("Inner mountain faces point into the arena and up"),FVector2D::DotProduct(Inward,FVector2D(V.Normal.X,V.Normal.Y))>0&&V.Normal.Z>0);
      }
      for(const auto& V:Section->ProcVertexBuffer) {
       const auto Position=Mesh->GetComponentTransform().TransformPosition(V.Position);
       Test->TestTrue(TEXT("Every backdrop vertex is finite and outside the playable ellipse"),!Position.ContainsNaN()&&FMath::Square(Position.X/Radius.X)+FMath::Square(Position.Y/Radius.Y)>.999);
       Low=FMath::Min(Low,float(Position.Z)); High=FMath::Max(High,float(Position.Z));
      }
     }
    } else {
     TArray<UMeshComponent*> Parts; It->GetComponents(Parts); for(auto* C:Parts) Bodies.Add({C,C->GetComponentTransform(),C->GetCollisionEnabled(),C->IsVisible()});
    }
   }
   Test->TestEqual(TEXT("Production creates one exterior mesh actor"),Ranges,1); Test->TestEqual(TEXT("Production creates no legacy cones"),Legacy,0);
   Test->TestEqual(TEXT("Complete exterior range uses 960 triangles"),Triangles,960); Test->TestTrue(TEXT("Exterior rock range has varied geological height"),High>20000&&Low<0);
   CultMatchBackdrop::Compare(W,false); View(W); Settle=45; Phase=2; return false;
  }
  if(Settle>0) { --Settle; return false; }
  if(Phase==2) {
   const TCHAR* Names[]={TEXT("LegacySpawn"),TEXT("LegacyOverview"),TEXT("RangeSpawn"),TEXT("RangeOverview")};
   Shot=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/MatchBackdrop_%s_%s.png"),*Label(),Names[Index]));
   IFileManager::Get().Delete(*Shot); auto& Config=GetHighResScreenshotConfig(); Config.SetHDRCapture(false); Config.SetMaskEnabled(false); Config.SetForce128BitRendering(false); Config.SetResolution(1280,720); Config.SetFilename(Shot);
   if(!W->GetGameViewport()->GetGameViewport()->TakeHighResScreenShot()) { Test->AddError(TEXT("Backdrop render request failed")); return true; } Phase=3; return false;
  }
  if(FScreenshotRequest::IsScreenshotRequested()) return false;
  TArray<uint8> Bytes; if(!FFileHelper::LoadFileToArray(Bytes,*Shot)||Bytes.Num()<24) return false;
  auto Size=[&](int At) { return (uint32(Bytes[At])<<24)|(uint32(Bytes[At+1])<<16)|(uint32(Bytes[At+2])<<8)|Bytes[At+3]; };
  Test->TestTrue(TEXT("Backdrop image is exactly 1280x720"),Size(16)==1280&&Size(20)==720); Verify();
  if(++Index<4) { if(Index==2) CultMatchBackdrop::Compare(W,true); View(W); Settle=45; Phase=2; return false; }
  Test->AddInfo(TEXT("Backdrop ")+Label()+TEXT(": one 960-triangle exterior mesh, no collision or shadows; all playable mesh transforms/collision/visibility preserved; four matched native renders.")); return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultDominationBackdropTest,"CultUlhu.Menu.MatchBackdrop.Domination",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultDominationBackdropTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultMatchBackdropObservation(this,ECultPlayMode::Domination)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultMobaBackdropTest,"CultUlhu.Menu.MatchBackdrop.Moba",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultMobaBackdropTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultMatchBackdropObservation(this,ECultPlayMode::Dotgoo)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
#endif
