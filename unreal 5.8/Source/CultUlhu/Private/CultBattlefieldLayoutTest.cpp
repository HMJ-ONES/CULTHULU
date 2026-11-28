#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CultAutomationPIE.h"
#include "CultBattlefieldLayout.h"
#include "CultLivingLandscape.h"
#include "CultUlhuGameInstance.h"
#include "CultUlhuPlayerPawn.h"
#include "CultMobaRoute.h"
#include "CultMatchDirector.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/PlayerController.h"
#include "UnrealClient.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

class FCultExpandedBattlefieldCheck:public IAutomationLatentCommand {
 FAutomationTestBase* Test; ECultPlayMode Mode; bool Entered=false,Checked=false;
 FString Shot; double Began=FPlatformTime::Seconds();
public:
 FCultExpandedBattlefieldCheck(FAutomationTestBase* T,ECultPlayMode M):Test(T),Mode(M) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>120) { Test->AddError(TEXT("Expanded battlefield check timed out")); return true; }
  auto* W=GEditor->PlayWorld.Get(); if(!W||W->bIsTearingDown) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) {
   if(!Entered) { auto* G=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); G->SelectedCharacterId=TEXT("cthulhu_avatar"); G->SelectedMode=Mode; G->SetInitialBeliefs({1,2,3}); G->FreeRoamSaveSlot=TEXT("Automation_ExpandedMap"); G->StartPlay(); Entered=true; }
   return false;
  }
  auto* PC=W->GetFirstPlayerController(); auto* Player=PC?Cast<ACultUlhuPlayerPawn>(PC->GetPawn()):nullptr;
  if(!Player||W->GetTimeSeconds()<2) return false;
  if(Checked) return IFileManager::Get().FileSize(*Shot)>100;
  float Half=0,Radius=0;
  for(const auto& Entry:UCultUlhuGameInstance::Roster()) {
   Half=FMath::Max(Half,Player->GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight()*Entry.HeightM/1.8f);
   Radius=FMath::Max(Radius,Player->GetCapsuleComponent()->GetUnscaledCapsuleRadius()*Entry.HeightM/1.8f);
  }
  FVector Look;
  if(Mode==ECultPlayMode::FreeRoam) {
   auto* Land=ACultLivingLandscape::Find(W); Test->TestNotNull(TEXT("Expanded free roam landscape exists"),Land); if(!Land) return true;
   Test->TestTrue(TEXT("Free Roam spans 5.2km"),Land->GroundBounds().GetSize().X>=519900);
   Test->TestEqual(TEXT("Eight cavern sites are populated"),ACultLivingLandscape::CaveSites().Num(),8);
   FCollisionQueryParams Query(SCENE_QUERY_STAT(ExpandedCaveTraversal),true);
   for(TActorIterator<AActor> It(W);It;++It) if(*It!=Land) Query.AddIgnoredActor(*It);
   for(const auto& Site:ACultLivingLandscape::CaveSites()) {
    FVector Floor; Test->TestTrue(TEXT("Cave founding ground remains reachable"),ACultLivingLandscape::Surface(W,Site,Floor,.5f));
    FHitResult Ceiling;
    Test->TestTrue(TEXT("Flying creatures cannot escape through the open arches' crossing corners"),W->LineTraceSingleByChannel(Ceiling,Site+FVector(2500,2500,4000),Site+FVector(2500,2500,9000),ECC_Visibility,Query));
    for(int Axis=0;Axis<2;++Axis) {
     const FVector Along=Axis?FVector(0,10000,0):FVector(10000,0,0),Center=Site+FVector(0,0,1600+Half+25);
     FHitResult Hit;
     const bool Blocked=W->SweepSingleByChannel(Hit,Center-Along,Center+Along,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half),Query);
     Test->TestFalse(TEXT("Largest roster capsule crosses both cave branches without a sealed intersection"),Blocked);
     if(Blocked) Test->AddInfo(FString::Printf(TEXT("CAVE_CONTACT site=%s axis=%d component=%s point=%s half=%.1f radius=%.1f"),*Site.ToString(),Axis,*GetNameSafe(Hit.GetComponent()),*Hit.ImpactPoint.ToString(),Half,Radius));
    }
   }
   Look=ACultLivingLandscape::CaveSites()[0]+FVector(0,0,3500);
  } else {
   const bool Moba=Mode==ECultPlayMode::Dotgoo; const auto Extent=CultBattlefieldLayout::HalfExtent(Moba);
   const float Y=CultBattlefieldLayout::OuterPassageY(Moba),Length=Moba?54000.f:66000.f;
   int Covers=0,Roofs=0;
   FCollisionQueryParams Query(SCENE_QUERY_STAT(ExpandedVaultTraversal),false,Player);
   for(TActorIterator<AActor> It(W);It;++It) {
    if(It->ActorHasTag(TEXT("CultExpansionCover"))) ++Covers;
    if(It->ActorHasTag(TEXT("CultExpansionRoof"))) ++Roofs;
    if(It->IsA<APawn>()) Query.AddIgnoredActor(*It);
   }
   Test->TestTrue(TEXT("Expanded map has substantial usable cover and fighting decks"),Covers>30&&Roofs>=6);
   for(int Side:{-1,1}) {
    const FVector Center(0,Side*Y,Half+25); FHitResult Hit;
    Test->TestFalse(TEXT("Largest roster capsule can traverse both outer vaults"),W->SweepSingleByChannel(Hit,Center-FVector(Length*.5+1200,0,0),Center+FVector(Length*.5+1200,0,0),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half),Query));
    Test->TestFalse(TEXT("Central side entrances are open at both ends"),W->SweepSingleByChannel(Hit,Center-FVector(0,6000,0),Center+FVector(0,6000,0),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Radius,Half),Query));
    Test->TestTrue(TEXT("Vault roof is a real fighting surface"),W->LineTraceSingleByChannel(Hit,FVector(0,Side*Y,9000),FVector(0,Side*Y,5000),ECC_Visibility,Query)&&FMath::IsNearlyEqual(float(Hit.ImpactPoint.Z),7000.f,5.f));
   }
   if(Moba) {
    FCollisionQueryParams LaneQuery(SCENE_QUERY_STAT(ExpandedLaneClearance),false);
    for(TActorIterator<AActor> It(W);It;++It) if(!It->ActorHasTag(TEXT("CultBattlefieldExpansion"))) LaneQuery.AddIgnoredActor(*It);
    for(int Lane=0;Lane<3;++Lane) for(int I=0;I<FCultMobaRoute::Segments;++I) {
     FHitResult Hit; const FVector Lift(0,0,175);
     Test->TestFalse(TEXT("New structures leave the actual creep routes clear"),W->SweepSingleByChannel(Hit,FCultMobaRoute::Knot(Lane,I)+Lift,FCultMobaRoute::Knot(Lane,I+1)+Lift,FQuat::Identity,ECC_Visibility,FCollisionShape::MakeCapsule(65,150),LaneQuery));
    }
   }
   FHitResult Ground; Test->TestTrue(TEXT("Expanded outskirts have solid ground"),W->LineTraceSingleByChannel(Ground,FVector(Extent.X*.86,0,2000),FVector(Extent.X*.86,0,-500),ECC_Visibility,Query));
   Look=FVector(0,Y,3000);
  }
  auto* Camera=W->SpawnActor<ACameraActor>(Look+FVector(-21000,-19000,14000),FRotator::ZeroRotator);
  Camera->SetActorRotation((Look-Camera->GetActorLocation()).Rotation()); PC->SetViewTarget(Camera); PC->PlayerCameraManager->UpdateCamera(0);
  Shot=FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/FString::Printf(TEXT("Screenshots/ExpandedBattlefield_%d.png"),int(Mode)));
  IFileManager::Get().Delete(*Shot); FScreenshotRequest::RequestScreenshot(Shot,false,false); Checked=true; return false;
 }
};
IMPLEMENT_COMPLEX_AUTOMATION_TEST(FCultExpandedBattlefields,"CultUlhu.World.ExpandedBattlefields",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
void FCultExpandedBattlefields::GetTests(TArray<FString>& Names,TArray<FString>& Commands) const { Names={TEXT("FreeRoam"),TEXT("Domination"),TEXT("DOTGOO")}; Commands={TEXT("0"),TEXT("1"),TEXT("2")}; }
bool FCultExpandedBattlefields::RunTest(const FString& Parameter) {
 ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultExpandedBattlefieldCheck(this,ECultPlayMode(FCString::Atoi(*Parameter)))); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
#endif
