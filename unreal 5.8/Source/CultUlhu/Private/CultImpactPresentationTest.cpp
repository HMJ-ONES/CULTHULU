#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CultAutomationPIE.h"
#include "CultBlastFX.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultMatchUnit.h"
#include "ProceduralMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "Slate/SceneViewport.h"
#include "HighResScreenshot.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "ShaderCompiler.h"

struct FCultImpactPresentationTestAccess {
 static float Radius(ACultBlastFX* F) { return F->RadiusM_; }
 static float LegacyRadius(ACultBlastFX* F) { return F->LegacyRadiusM_; }
 static FVector Direction(ACultBlastFX* F) { return F->ImpactDirection_; }
 static float Intensity(ACultBlastFX* F) { return F->SignalMaterial_->K2_GetScalarParameterValue(TEXT("Intensity")); }
 static FBox Geometry(ACultBlastFX* F) { FBox B(ForceInit); if(F->Signal_) if(auto* S=F->Signal_->GetProcMeshSection(0)) for(const auto& V:S->ProcVertexBuffer) B+=V.Position; return B; }
 static int Triangles(ACultBlastFX* F) { return F->Signal_?F->Signal_->GetProcMeshSection(0)->ProcIndexBuffer.Num()/3:0; }
};
class FCultImpactObservation:public IAutomationLatentCommand {
 FAutomationTestBase* Test; int Phase=0,Index=0,Settle=0,Previous=1; double Began=FPlatformTime::Seconds(); FString Shot;
 TWeakObjectPtr<ACameraActor> Camera; TWeakObjectPtr<APlayerController> PC; TWeakObjectPtr<AActor> Original;
 TArray<TWeakObjectPtr<ACultBlastFX>> Effects; TArray<TWeakObjectPtr<ACultMatchUnit>> Targets;
 FHighResScreenshotConfig OldCapture; uint32 OldX=0,OldY=0;
 int Components(ACultBlastFX* F) { TArray<UMeshComponent*> Parts; F->GetComponents(Parts); return Parts.Num(); }
 void VerifySignal(ACultBlastFX* F,int Triangles) {
  Test->TestEqual(TEXT("Refined impact has one mesh component"),Components(F),1);
  Test->TestEqual(TEXT("Impact geometry has bounded triangle count"),FCultImpactPresentationTestAccess::Triangles(F),Triangles);
  auto* Mesh=F->FindComponentByClass<UProceduralMeshComponent>(); Test->TestNotNull(TEXT("Impact uses one open signal mesh"),Mesh);
  if(Mesh) { Test->TestEqual(TEXT("Impact never adds gameplay collision"),Mesh->GetCollisionEnabled(),ECollisionEnabled::NoCollision); Test->TestFalse(TEXT("Impact never casts a target-obscuring shadow"),Mesh->CastShadow); }
 }
 void SetupView(UWorld* W) {
  for(auto& F:Effects) if(F.IsValid()) F->Destroy(); Effects.Reset();
  IConsoleManager::Get().FindConsoleVariable(TEXT("cult.FX.ImpactSignals"))->Set(Index>=2?1:0,ECVF_SetByCode);
  if(Index%2==0) {
   for(int Side:{-1,1}) { auto* F=ACultBlastFX::SpawnBlast(W,ECultBlastKind::HitFlash,FVector(Side*220,-75,60150),Side<0?.3f:.6f,FVector::YAxisVector); F->SetActorTickEnabled(false); F->Tick(.04f); Effects.Add(F); }
   Camera->SetActorLocation(FVector(0,-850,60450)); Camera->SetActorRotation((FVector(0,0,60120)-Camera->GetActorLocation()).Rotation());
  } else {
   auto* F=ACultBlastFX::SpawnBlast(W,ECultBlastKind::ShockRing,FVector(0,0,60015),8); F->SetActorTickEnabled(false); F->Tick(.04f); Effects.Add(F);
   Camera->SetActorLocation(FVector(0,-1700,61800)); Camera->SetActorRotation((FVector(0,0,60000)-Camera->GetActorLocation()).Rotation());
  }
  PC->SetViewTarget(Camera.Get()); PC->PlayerCameraManager->UpdateCamera(0);
 }
public:
 FCultImpactObservation(FAutomationTestBase* T):Test(T) { OldCapture=GetHighResScreenshotConfig(); OldX=GScreenshotResolutionX; OldY=GScreenshotResolutionY; Previous=IConsoleManager::Get().FindConsoleVariable(TEXT("cult.FX.ImpactSignals"))->GetInt(); }
 ~FCultImpactObservation() {
  IConsoleManager::Get().FindConsoleVariable(TEXT("cult.FX.ImpactSignals"))->Set(Previous,ECVF_SetByCode);
  GetHighResScreenshotConfig()=OldCapture; GScreenshotResolutionX=OldX; GScreenshotResolutionY=OldY;
  if(PC.IsValid()) { PC->SetPause(false); if(Original.IsValid()) PC->SetViewTarget(Original.Get()); }
 }
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>120) { Test->AddError(TEXT("Impact presentation verification timed out")); return true; }
  auto* W=GEditor->PlayWorld.Get(); if(!W||W->bIsTearingDown) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) { if(!Phase) { auto* GI=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); GI->SelectedCharacterId=TEXT("cthulhu_avatar"); GI->OpenCharacterSelect(ECultPlayMode::Dotgoo); GI->OpenBeliefSelect(); Phase=1; } return false; }
  auto* Controller=W->GetFirstPlayerController(); auto* Player=Cast<ACultUlhuPlayerPawn>(UGameplayStatics::GetPlayerPawn(W,0));
  if(!Controller||!Player||W->GetTimeSeconds()<1||(GShaderCompilingManager&&GShaderCompilingManager->IsCompiling())) return false;
  if(Phase==1) {
   PC=Controller; Original=PC->GetViewTarget(); PC->SetPause(true); Camera=W->SpawnActor<ACameraActor>(); Camera->GetCameraComponent()->SetConstraintAspectRatio(false);
   auto* Floor=W->SpawnActor<AStaticMeshActor>(FVector(0,0,59950),FRotator::ZeroRotator); auto* M=Floor->GetStaticMeshComponent(); M->SetMobility(EComponentMobility::Movable); M->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube"))); M->SetWorldScale3D(FVector(30,30,1)); M->SetMaterial(0,LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/CultUlhu/Materials/M_MatchStone")));
   for(int Side:{-1,1}) { const FTransform T(FVector(Side*220,0,60150)); auto* U=W->SpawnActorDeferred<ACultMatchUnit>(ACultMatchUnit::StaticClass(),T,nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn); U->Kind=0; U->Team=1; U->Health=120; U->FinishSpawning(T); U->SetActorTickEnabled(false); Targets.Add(U); }
   auto* Switch=IConsoleManager::Get().FindConsoleVariable(TEXT("cult.FX.ImpactSignals")); Switch->Set(0,ECVF_SetByCode);
   auto* OldNormal=ACultBlastFX::SpawnBlast(W,ECultBlastKind::HitFlash,FVector(0,0,65000),.3f); auto* OldKill=ACultBlastFX::SpawnBlast(W,ECultBlastKind::HitFlash,FVector(0,0,65000),.6f);
   Test->TestEqual(TEXT("Legacy normal hit was inflated to two metres"),FCultImpactPresentationTestAccess::LegacyRadius(OldNormal),2.f); Test->TestEqual(TEXT("Legacy kill was the same inflated size"),FCultImpactPresentationTestAccess::LegacyRadius(OldKill),2.f); OldNormal->Destroy(); OldKill->Destroy();
   Switch->Set(1,ECVF_SetByCode);
   auto* Normal=ACultBlastFX::SpawnBlast(W,ECultBlastKind::HitFlash,FVector(0,0,65000),.3f,FVector::YAxisVector); auto* Kill=ACultBlastFX::SpawnBlast(W,ECultBlastKind::HitFlash,FVector(0,0,65000),.6f,FVector::YAxisVector);
   VerifySignal(Normal,48); VerifySignal(Kill,48); const auto Small=FCultImpactPresentationTestAccess::Geometry(Normal),Large=FCultImpactPresentationTestAccess::Geometry(Kill);
   Test->TestTrue(TEXT("Kill streaks preserve twice the requested normal-hit scale"),FMath::IsNearlyEqual(Large.GetSize().GetMax()/Small.GetSize().GetMax(),2.f,.01f));
   Test->TestTrue(TEXT("Normal hit geometry stays within its small requested radius"),Small.GetExtent().GetMax()<30);
   const float Start=FCultImpactPresentationTestAccess::Intensity(Normal); Normal->Tick(.06f); Test->TestTrue(TEXT("Hit intensity fades continuously"),FCultImpactPresentationTestAccess::Intensity(Normal)<Start);
   Normal->Tick(.13f); Test->TestTrue(TEXT("Hit effect expires after 180 milliseconds"),Normal->IsActorBeingDestroyed()); Kill->Destroy();
   auto* Shock=ACultBlastFX::SpawnBlast(W,ECultBlastKind::ShockRing,FVector(0,0,65000),8); VerifySignal(Shock,576); Shock->Tick(.3f); const float Mid=FCultImpactPresentationTestAccess::Geometry(Shock).GetExtent().GetMax(); Test->TestTrue(TEXT("Shock expands toward its bounded requested radius"),Mid>600&&Mid<850); Shock->Tick(.26f); Test->TestTrue(TEXT("Shock effect expires after 550 milliseconds"),Shock->IsActorBeingDestroyed());
   auto* Dust=ACultBlastFX::SpawnBlast(W,ECultBlastKind::DustBurst,FVector(0,0,65000),3); Test->TestEqual(TEXT("Collapse dust retains its nine original puffs"),Components(Dust),9); Dust->Destroy();
   // Exercise the real local hit-feedback caller, including its directional payload.
   TSet<ACultBlastFX*> Before; for(TActorIterator<ACultBlastFX> It(W);It;++It) Before.Add(*It);
   const FVector Contact(180,0,60200); Player->RecordCombatHit(Targets[0].Get(),Contact,10,false); int Found=0;
   for(TActorIterator<ACultBlastFX> It(W);It;++It) if(!Before.Contains(*It)) { ++Found; Test->TestTrue(TEXT("Real hit feedback retains exact contact position"),It->GetActorLocation().Equals(Contact)); Test->TestEqual(TEXT("Real hit feedback retains 0.3m size"),FCultImpactPresentationTestAccess::Radius(*It),.3f); Test->TestTrue(TEXT("Real hit streaks receive the source-to-contact direction"),FCultImpactPresentationTestAccess::Direction(*It).Equals((Contact-Player->GetActorLocation()).GetSafeNormal(),.001f)); It->Destroy(); }
   Test->TestEqual(TEXT("Real feedback caller spawns one world effect"),Found,1);
   SetupView(W); Settle=30; Phase=2; return false;
  }
  if(Settle>0) { --Settle; return false; }
  if(Phase==2) {
   const TCHAR* Names[]={TEXT("LegacyHits"),TEXT("LegacyShock"),TEXT("OpenHits"),TEXT("OpenShock")};
   Shot=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/ImpactPresentation_%s.png"),Names[Index])); IFileManager::Get().Delete(*Shot);
   auto& C=GetHighResScreenshotConfig(); C.SetHDRCapture(false); C.SetMaskEnabled(false); C.SetForce128BitRendering(false); C.SetResolution(1280,720); C.SetFilename(Shot);
   if(!W->GetGameViewport()->GetGameViewport()->TakeHighResScreenShot()) { Test->AddError(TEXT("Impact capture failed")); return true; } Phase=3; return false;
  }
  if(FScreenshotRequest::IsScreenshotRequested()) return false;
  TArray<uint8> Bytes; if(!FFileHelper::LoadFileToArray(Bytes,*Shot)||Bytes.Num()<24) return false;
  auto Size=[&](int At){return (uint32(Bytes[At])<<24)|(uint32(Bytes[At+1])<<16)|(uint32(Bytes[At+2])<<8)|Bytes[At+3];}; Test->TestTrue(TEXT("Impact comparison captures exact 1280x720"),Size(16)==1280&&Size(20)==720);
  for(const auto& U:Targets) if(U.IsValid()) Test->TestEqual(TEXT("Visual impacts do not change target health"),U->Health,120.f);
  if(++Index<4) { SetupView(W); Settle=30; Phase=2; return false; }
  Test->AddInfo(TEXT("Impact signals: original two-metre clamp reproduced; distinct 0.3/0.6m hits, 48/576 triangles, one noncolliding mesh, bounded fade/expiry, real feedback direction/contact and four matched captures.")); return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultImpactPresentationTest,"CultUlhu.Menu.ImpactPresentation",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultImpactPresentationTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultImpactObservation(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
#endif
