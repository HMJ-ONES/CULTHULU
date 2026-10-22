#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CultAutomationPIE.h"
#include "CultHeroPlates.h"
#include "CultHUDTextProbe.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultMatchDirector.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/GameViewportClient.h"
#include "Slate/SceneViewport.h"
#include "HighResScreenshot.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"

class FCultHeroPlateObservation:public IAutomationLatentCommand {
 FAutomationTestBase* Test; ECultPlayMode Mode; int Phase=0,ShotIndex=0; double Started=FPlatformTime::Seconds(),At=0;
 FHighResScreenshotConfig OldCapture; uint32 OldX=0,OldY=0; int OldEnabled=1;
 TWeakObjectPtr<ACultUlhuPlayerPawn> Focus; TWeakObjectPtr<AStaticMeshActor> Wall; TWeakObjectPtr<ACameraActor> Camera;
 FString Shot; const TArray<FIntPoint> Sizes={FIntPoint(1280,720),FIntPoint(1280,720),FIntPoint(640,360),FIntPoint(1920,720),FIntPoint(1280,720),FIntPoint(1920,720)};
 FString ModeName() const { return Mode==ECultPlayMode::Domination?TEXT("Domination"):TEXT("Moba"); }
public:
 FCultHeroPlateObservation(FAutomationTestBase* In,ECultPlayMode Choice):Test(In),Mode(Choice) {
  OldCapture=GetHighResScreenshotConfig(); OldX=GScreenshotResolutionX; OldY=GScreenshotResolutionY;
  OldEnabled=IConsoleManager::Get().FindConsoleVariable(TEXT("cult.Match.HeroPlates"))->GetInt(); CultHeroPlates::Probe=true; CultHUDTextProbe::Enabled=true;
 }
 ~FCultHeroPlateObservation() {
  GetHighResScreenshotConfig()=OldCapture; GScreenshotResolutionX=OldX; GScreenshotResolutionY=OldY;
  IConsoleManager::Get().FindConsoleVariable(TEXT("cult.Match.HeroPlates"))->Set(OldEnabled,ECVF_SetByCode);
  CultHeroPlates::Probe=false; CultHeroPlates::Captured.Reset(); if(Wall.IsValid()) Wall->Destroy(); if(Camera.IsValid()) Camera->Destroy();
  CultHUDTextProbe::Enabled=false; CultHUDTextProbe::Captured.Reset(); CultHUDTextProbe::Samples.Reset();
  if(auto* W=GEditor->PlayWorld.Get()) UGameplayStatics::SetGamePaused(W,false);
 }
 bool Update() override {
  if(FPlatformTime::Seconds()-Started>120) { Test->AddError(TEXT("Hero plate render check timed out")); return true; }
  auto* W=GEditor->PlayWorld.Get(); if(!W||W->bIsTearingDown) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) { if(!Phase) { auto* G=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); G->SelectedCharacterId=TEXT("cthulhu_avatar"); G->OpenCharacterSelect(Mode); G->OpenBeliefSelect(); Phase=1; } return false; }
  auto* M=ACultMatchDirector::Find(W); auto* PC=W->GetFirstPlayerController(); if(!M||!PC||W->GetTimeSeconds()<1) return false;
  if(Phase==1) {
   M->Tick(20); M->Tick(4); M->SetActorTickEnabled(false); UGameplayStatics::SetGamePaused(W,true);
   for(int I=0;I<10;++I) { auto* H=M->Fighters()[I].Pawn.Get(); H->GetCharacterMovement()->DisableMovement(); H->RelocateMatch(FVector(60000+I*2000,60000,0)); }
   auto* P=M->Fighters()[0].Pawn.Get(); P->RelocateMatch(FVector(-9000,-8000,0));
   M->Fighters()[1].Pawn->RelocateMatch(FVector(-3500,1500,0)); M->Fighters()[3].Pawn->RelocateMatch(FVector(3500,1500,0));
   Focus=M->Fighters()[5].Pawn; Focus->RelocateMatch(FVector(0,0,0)); M->Fighters()[6].Pawn->RelocateMatch(FVector(1700,1800,0));
   Focus->ReceiveDamage(60,P); M->Fighters()[1].Pawn->ReceiveDamage(45,Focus.Get());
   Test->TestEqual(TEXT("Fixture uses real competitive forty-health enemy"),Focus->GetHealth(),40.f);
   Camera=W->SpawnActor<ACameraActor>(FVector(0,-9000,5000),FRotator::ZeroRotator);
   Camera->SetActorRotation((FVector(0,500,3100)-Camera->GetActorLocation()).Rotation()); PC->SetViewTarget(Camera.Get()); PC->PlayerCameraManager->UpdateCamera(0);
   Phase=2; return false;
  }
  if(Phase==2) {
   auto* Client=W->GetGameViewport(); if(!Client||!Client->GetGameViewport()) return false;
   IConsoleManager::Get().FindConsoleVariable(TEXT("cult.Match.HeroPlates"))->Set(ShotIndex?1:0,ECVF_SetByCode);
   Camera->GetCameraComponent()->bConstrainAspectRatio=ShotIndex!=3;
   PC->PlayerCameraManager->UpdateCamera(0);
   if(ShotIndex==4) {
    Wall=W->SpawnActor<AStaticMeshActor>(FVector(0,-4000,3500),FRotator::ZeroRotator);
    Wall->GetStaticMeshComponent()->SetMobility(EComponentMobility::Movable); Wall->GetStaticMeshComponent()->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube"))); Wall->SetActorScale3D(FVector(18,1,80));
   }
   if(ShotIndex==5&&Wall.IsValid()) Wall->Destroy();
   CultHeroPlates::ProbeRequest=FVector2D(ShotIndex==5?FIntPoint(1280,720):Sizes[ShotIndex]); CultHeroPlates::ProbeReady=false;
   CultHUDTextProbe::Captured.Reset(); CultHUDTextProbe::CaptureFrame=MAX_uint64;
   Shot=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/HeroPlates_%s_%d_%dx%d.png"),*ModeName(),ShotIndex,Sizes[ShotIndex].X,Sizes[ShotIndex].Y));
   IFileManager::Get().Delete(*Shot); auto& Capture=GetHighResScreenshotConfig(); Capture.SetHDRCapture(false); Capture.SetMaskEnabled(false); Capture.SetForce128BitRendering(false);
   if(!Capture.SetResolution(Sizes[ShotIndex].X,Sizes[ShotIndex].Y)) { Test->AddError(TEXT("Hero plate capture size rejected")); return true; }
   Capture.SetFilename(Shot); if(!Client->GetGameViewport()->TakeHighResScreenShot()) { Test->AddError(TEXT("Hero plate capture could not start")); return true; }
   At=FPlatformTime::Seconds(); Phase=3; return false;
  }
  if(FPlatformTime::Seconds()-At<.4||!CultHeroPlates::ProbeReady||IFileManager::Get().FileSize(*Shot)<24) return false;
  TArray<uint8> Bytes; FFileHelper::LoadFileToArray(Bytes,*Shot);
  auto Dimension=[&](int N) { return (uint32(Bytes[N])<<24)|(uint32(Bytes[N+1])<<16)|(uint32(Bytes[N+2])<<8)|uint32(Bytes[N+3]); };
  Test->TestEqual(TEXT("Actual PNG width matches requested plate viewport"),Dimension(16),uint32(Sizes[ShotIndex].X));
  Test->TestEqual(TEXT("Actual PNG height matches requested plate viewport"),Dimension(20),uint32(Sizes[ShotIndex].Y));
  const auto& Cards=CultHeroPlates::Captured;
  Test->TestTrue(TEXT("Plate layout matches the actual constrained or unconstrained Canvas"),CultHeroPlates::ProbeSize.Equals(CultHeroPlates::ProbeRequest,1));
  if(ShotIndex==0) Test->TestEqual(TEXT("Presentation opt-out removes all hero plates"),Cards.Num(),0);
  else if(ShotIndex<4||ShotIndex==5) {
   Test->TestTrue(TEXT("Visible competitive heroes receive actual rendered identity plates"),Cards.Num()>0);
   Test->TestTrue(TEXT("Small viewports limit plates to two; normal viewports to four"),Cards.Num()<=(ShotIndex==2?2:4));
   const auto* Primary=Cards.FindByPredicate([&](const auto& C){return C.Hero==Focus;});
   Test->TestNotNull(TEXT("A visible damaged combat focus survives plate priority selection"),Primary);
   if(Primary) { Test->TestEqual(TEXT("Plate uses actual remaining health"),Primary->Health,40.f); Test->TestFalse(TEXT("Enemy plate cannot be styled as an ally"),Primary->Friendly); Test->TestTrue(TEXT("The damaged local combat focus receives focus styling"),Primary->Focused); }
   if(ShotIndex==1) Test->TestTrue(TEXT("An injured visible ally also receives a health plate"),Cards.ContainsByPredicate([](const auto& C){return C.Friendly&&C.Health<100;}));
  } else {
   Test->TestFalse(TEXT("Solid cover removes the remembered focus's overhead plate"),Cards.ContainsByPredicate([&](const auto& C){return C.Hero==Focus;}));
   Test->TestTrue(TEXT("Cover does not hide unrelated visible allied heroes"),Cards.ContainsByPredicate([](const auto& C){return C.Friendly;}));
  }
  for(int I=0;I<Cards.Num();++I) {
   Test->TestTrue(TEXT("Every plate remains inside the reserved play region"),CultHeroPlates::CapturedSafe.IsInside(Cards[I].Bounds.Min)&&CultHeroPlates::CapturedSafe.IsInside(Cards[I].Bounds.Max));
   for(int J=0;J<I;++J) Test->TestFalse(TEXT("Adjacent projected hero plates never overlap"),Cards[I].Bounds.Intersect(Cards[J].Bounds));
   const auto* Glyph=CultHUDTextProbe::Captured.FindByPredicate([&](const auto& G){return G.Text==Cards[I].Name&&Cards[I].Bounds.IsInside(G.Position);});
   Test->TestNotNull(TEXT("Each reported plate has an actual Canvas name glyph"),Glyph);
   if(Glyph) Test->TestTrue(TEXT("Actual name glyph advances fit their plate"),(Glyph->Position*Glyph->DPI+Glyph->Drawn).X<=Cards[I].Bounds.Max.X*Glyph->DPI+1);
  }
  if(ShotIndex==1) Test->TestTrue(TEXT("Normal view retains the real floating damage number"),CultHUDTextProbe::Captured.ContainsByPredicate([](const auto& G){return G.Text==TEXT("60");}));
  if(Mode==ECultPlayMode::Dotgoo&&ShotIndex==2) Test->TestFalse(TEXT("Compact damage numbers never cover the minion-throw instruction"),CultHUDTextProbe::Captured.ContainsByPredicate([](const auto& G){return G.Text==TEXT("60");}));
  Test->AddInfo(FString::Printf(TEXT("Hero plates %s shot=%d canvas=%dx%d actualCards=%d"),*ModeName(),ShotIndex,Sizes[ShotIndex].X,Sizes[ShotIndex].Y,Cards.Num()));
  if(++ShotIndex<6) { Phase=2; return false; } return true;
 }
};
#define CULT_PLATE_TEST(Type,Path,Mode) \
 IMPLEMENT_SIMPLE_AUTOMATION_TEST(Type,Path,EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter) \
 bool Type::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultHeroPlateObservation(this,Mode)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
CULT_PLATE_TEST(FCultDomHeroPlatesTest,"CultUlhu.Menu.HeroPlates.Domination",ECultPlayMode::Domination)
CULT_PLATE_TEST(FCultMobaHeroPlatesTest,"CultUlhu.Menu.HeroPlates.Moba",ECultPlayMode::Dotgoo)
#undef CULT_PLATE_TEST
#endif
