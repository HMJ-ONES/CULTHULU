#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CultAutomationPIE.h"
#include "CultVesselSurface.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultMatchDirector.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Materials/Material.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "MaterialShared.h"
#include "ShaderCompiler.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "Slate/SceneViewport.h"
#include "HighResScreenshot.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "ProfilingDebugging/CsvProfiler.h"
CSV_DECLARE_CATEGORY_EXTERN(CultRuntime);

class FCultVesselSurfaceObservation:public IAutomationLatentCommand {
 FAutomationTestBase* Test; ECultPlayMode Mode; int Phase=0,Index=0,Settle=0,Segment=0,Frames=0;
 double Began=FPlatformTime::Seconds(); FString Shot; TSharedFuture<FString> Written;
 TWeakObjectPtr<APlayerController> Controller; TWeakObjectPtr<AActor> Original; TWeakObjectPtr<ACameraActor> Camera;
 FHighResScreenshotConfig OldCapture; uint32 OldX=0,OldY=0;
 struct FBody { ACultUlhuPlayerPawn* Hero; UStaticMeshComponent* Body; FTransform Transform; FTransform HeroTransform; ECollisionEnabled::Type Collision; float Radius,Half,Health; };
 TArray<FBody> Bodies;
 FString Label() const { return Mode==ECultPlayMode::Domination?TEXT("Domination"):TEXT("Moba"); }
 void Apply(bool Legacy) { for(const auto& B:Bodies) CultVesselSurface::Apply(B.Hero,B.Body,Legacy); }
 void View() { auto* PC=Controller.Get(); PC->SetViewTarget(Index%2?Original.Get():Camera.Get()); PC->PlayerCameraManager->UpdateCamera(0); }
 void Verify() {
  for(const auto& B:Bodies) {
   Test->TestTrue(TEXT("Surface preserves actual hero placement and canonical height"),B.Hero->GetActorTransform().Equals(B.HeroTransform,.01f));
   Test->TestTrue(TEXT("Surface preserves body mesh placement"),B.Body->GetComponentTransform().Equals(B.Transform,.01f));
   Test->TestEqual(TEXT("Surface preserves non-colliding visual body"),B.Body->GetCollisionEnabled(),B.Collision);
   Test->TestEqual(TEXT("Surface preserves gameplay capsule radius"),B.Hero->GetCapsuleComponent()->GetScaledCapsuleRadius(),B.Radius);
   Test->TestEqual(TEXT("Surface preserves gameplay capsule half-height"),B.Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),B.Half);
   Test->TestEqual(TEXT("Surface cannot change health"),B.Hero->GetHealth(),B.Health);
  }
 }
public:
 FCultVesselSurfaceObservation(FAutomationTestBase* T,ECultPlayMode M):Test(T),Mode(M) { OldCapture=GetHighResScreenshotConfig(); OldX=GScreenshotResolutionX; OldY=GScreenshotResolutionY; }
 ~FCultVesselSurfaceObservation() { GetHighResScreenshotConfig()=OldCapture; GScreenshotResolutionX=OldX; GScreenshotResolutionY=OldY; if(auto* PC=Controller.Get()) { PC->SetPause(false); if(Original.IsValid()) PC->SetViewTarget(Original.Get()); } if(Camera.IsValid()) Camera->Destroy(); }
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>180) { Test->AddError(TEXT("Vessel surface verification timed out")); if(FCsvProfiler::Get()->IsCapturing()) FCsvProfiler::Get()->EndCapture(); return true; }
  auto* W=GEditor->PlayWorld.Get(); if(!W||W->bIsTearingDown) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) { if(!Phase) { auto* GI=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); GI->SelectedCharacterId=TEXT("cthulhu_avatar"); GI->OpenCharacterSelect(Mode); GI->OpenBeliefSelect(); Phase=1; } return false; }
  auto* PC=W->GetFirstPlayerController(); auto* Match=ACultMatchDirector::Find(W);
  if(!PC||!Match||W->GetTimeSeconds()<1||(GShaderCompilingManager&&GShaderCompilingManager->IsCompiling())) return false;
  if(Phase==1) {
   auto* Material=LoadObject<UMaterial>(nullptr,TEXT("/Game/CultUlhu/Materials/M_VesselSurface")); Test->TestNotNull(TEXT("Original vessel material exists"),Material); if(!Material) return true;
   auto* Resource=Material->GetMaterialResource(GMaxRHIShaderPlatform);
   Test->TestTrue(TEXT("Vessel uses a compiled non-fallback shader map"),Resource&&Resource->GetGameThreadShaderMap()&&Resource->GetCompileErrors().IsEmpty());
   if(Resource) for(const auto& Error:Resource->GetCompileErrors()) Test->AddError(Error);
   Controller=PC; Original=PC->GetViewTarget(); Match->SetActorTickEnabled(false); PC->SetPause(true);
   for(int I=1;I<10;++I) Match->Fighters()[I].Pawn->RelocateMatch(FVector(60000+I*2000,60000,0));
   Match->Fighters()[1].Pawn->RelocateMatch(FVector(-3500,1500,0)); Match->Fighters()[3].Pawn->RelocateMatch(FVector(3500,1500,0));
   Match->Fighters()[5].Pawn->RelocateMatch(FVector(0,0,0)); Match->Fighters()[6].Pawn->RelocateMatch(FVector(1700,1800,0));
   Camera=W->SpawnActor<ACameraActor>(FVector(0,-9000,5000),FRotator::ZeroRotator); Camera->SetActorRotation((FVector(0,500,3100)-Camera->GetActorLocation()).Rotation());
   TSet<UMaterialInterface*> Materials;
   for(const auto& F:Match->Fighters()) {
    auto* Hero=F.Pawn.Get(); TArray<UStaticMeshComponent*> Parts; Hero->GetComponents(Parts);
    for(auto* C:Parts) if(C->GetFName()==TEXT("VesselBody")) {
     auto* MID=Cast<UMaterialInstanceDynamic>(C->GetMaterial(0)); Test->TestNotNull(TEXT("Each production hero has its own surface instance"),MID);
     if(MID) { Test->TestTrue(TEXT("Production uses the original vessel shader"),MID->Parent==Material); Materials.Add(MID);
      Test->TestTrue(TEXT("Faction accent is installed on the real body"),MID->K2_GetVectorParameterValue(TEXT("VeinColor")).Equals(F.Team==0?FLinearColor(.12f,.36f,.32f):FLinearColor(.45f,.14f,.09f),.001f)); }
     Bodies.Add({Hero,C,C->GetComponentTransform(),Hero->GetActorTransform(),C->GetCollisionEnabled(),Hero->GetCapsuleComponent()->GetScaledCapsuleRadius(),Hero->GetCapsuleComponent()->GetScaledCapsuleHalfHeight(),Hero->GetHealth()});
    }
   }
   Test->TestEqual(TEXT("All ten physical hero bodies participate"),Bodies.Num(),10); Test->TestEqual(TEXT("Hero surface tints are independent"),Materials.Num(),10);
   Apply(true); View(); Settle=45; Phase=2; return false;
  }
  if(Settle>0) { --Settle; return false; }
  if(Phase==2) {
   const TCHAR* Names[]={TEXT("BeforeGroup"),TEXT("BeforeSpawn"),TEXT("AfterGroup"),TEXT("AfterSpawn")};
   Shot=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/VesselSurface_%s_%s.png"),*Label(),Names[Index]));
   IFileManager::Get().Delete(*Shot); auto& Capture=GetHighResScreenshotConfig(); Capture.SetHDRCapture(false); Capture.SetMaskEnabled(false); Capture.SetForce128BitRendering(false); Capture.SetResolution(1280,720); Capture.SetFilename(Shot);
   if(!W->GetGameViewport()->GetGameViewport()->TakeHighResScreenShot()) { Test->AddError(TEXT("Vessel surface capture failed")); return true; } Phase=3; return false;
  }
  if(Phase==3) {
   if(FScreenshotRequest::IsScreenshotRequested()) return false;
   TArray<uint8> Bytes; if(!FFileHelper::LoadFileToArray(Bytes,*Shot)||Bytes.Num()<24) return false;
   auto Size=[&](int N){return (uint32(Bytes[N])<<24)|(uint32(Bytes[N+1])<<16)|(uint32(Bytes[N+2])<<8)|Bytes[N+3];};
   Test->TestTrue(TEXT("Vessel comparison is an exact native 1280x720 render"),Size(16)==1280&&Size(20)==720); Verify();
   if(++Index<4) { if(Index==2) Apply(false); View(); Settle=45; Phase=2; return false; }
   Index=0; View(); Apply(true); Settle=30; Phase=4; return false;
  }
  if(Phase==4) {
   const auto Dir=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Reports/Captures/VesselSurface")); IFileManager::Get().MakeDirectory(*Dir,true);
   FCsvProfiler::Get()->BeginCapture(-1,Dir,FString::Printf(TEXT("%s_%d.csv"),*Label(),Segment)); Frames=0; Phase=5; return false;
  }
  if(Phase==5) { if(++Frames<120) return false; Written=FCsvProfiler::Get()->EndCapture(); Phase=6; return false; }
  if(!Written.IsReady()) return false;
  Test->TestTrue(TEXT("Frozen vessel render-cost CSV exists"),IFileManager::Get().FileSize(*Written.Get())>0); Test->AddInfo(TEXT("Vessel surface profile: ")+Written.Get()); Verify();
  if(++Segment<4) { Apply(!(Segment==1||Segment==2)); Settle=30; Phase=4; return false; }
  Apply(false); Test->AddInfo(TEXT("Vessel surface ")+Label()+TEXT(": ten independent body instances, unchanged geometry/collision/health, four native renders and frozen A-B-B-A render windows")); return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultDomVesselSurfaceTest,"CultUlhu.Menu.VesselSurface.Domination",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultDomVesselSurfaceTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultVesselSurfaceObservation(this,ECultPlayMode::Domination)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultMobaVesselSurfaceTest,"CultUlhu.Menu.VesselSurface.Moba",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultMobaVesselSurfaceTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultVesselSurfaceObservation(this,ECultPlayMode::Dotgoo)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
#endif
