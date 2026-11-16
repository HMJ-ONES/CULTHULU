#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CultAutomationPIE.h"
#include "CultPlayHUD.h"
#include "CultRegionMap.h"
#include "CultRegionNavigation.h"
#include "CultHUDSpace.h"
#include "CultWorldDirector.h"
#include "CultPopulation.h"
#include "CultDestructible.h"
#include "CultFeralBeast.h"
#include "Engine/StaticMesh.h"
#include "CultUlhuPlayerPawn.h"
#include "CultUlhuGameInstance.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "InputKeyEventArgs.h"
#include "EngineUtils.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"

struct FCultRegionMapTestAccess {
 static FBox2D Map(ACultPlayHUD* H) { return H->RegionMapBounds_; }
 static FBox2D Button(ACultPlayHUD* H,int I) { return H->RegionMapButtons_[I]; }
 static int Region(ACultPlayHUD* H) { return H->MapRegion_; }
};
namespace {
void MapKey(APlayerController* PC,FKey Key,EInputEvent Event) { PC->InputKey(FInputKeyEventArgs(nullptr,FInputDeviceId::CreateFromInternalId(0),Key,Event,Event==IE_Released?0.f:1.f,false,FPlatformTime::Cycles64())); }
void MapMouse(APlayerController* PC,FVector2D P) { FIntRect Rect; if(CultHUDSpace::View(PC,Rect)) PC->SetMouseLocation(FMath::RoundToInt(P.X)+Rect.Min.X,FMath::RoundToInt(P.Y)+Rect.Min.Y); }
}
class FCultRegionMapObservation:public IAutomationLatentCommand {
 FAutomationTestBase* Test; int Phase=0; double Began=FPlatformTime::Seconds(),At=0;
 TWeakObjectPtr<ACultFeralBeast> Quarry;
 TArray<TWeakObjectPtr<ACultPopulation>> Units; FVector Destination,Before; int Region=-1;
public:
 explicit FCultRegionMapObservation(FAutomationTestBase* T):Test(T) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>100) { Test->AddError(TEXT("Region map input test timed out")); return true; }
  UWorld* W=nullptr; for(auto& C:GEngine->GetWorldContexts()) if(C.World()&&C.World()->IsGameWorld()) W=C.World(); if(!W) return false;
  if(W->GetName().Contains(TEXT("L_MainMenu"))) { if(!Phase) { auto* G=Cast<UCultUlhuGameInstance>(W->GetGameInstance()); G->SelectedCharacterId=TEXT("mi_go"); G->OpenCharacterSelect(ECultPlayMode::FreeRoam); G->SetInitialBeliefs({5,6,11}); G->StartPlay(); Phase=1; } return false; }
  auto* PC=W->GetFirstPlayerController(); auto* P=PC?Cast<ACultUlhuPlayerPawn>(PC->GetPawn()):nullptr; auto* H=PC?Cast<ACultPlayHUD>(PC->GetHUD()):nullptr; auto* D=ACultWorldDirector::Find(W); if(!P||!H||!D||W->GetTimeSeconds()<3) return false;
  const double Now=FPlatformTime::Seconds();
  if(Phase==1) {
   FVector Floor; Test->TestTrue(TEXT("Fixture has real landscape ground"),ACultLivingLandscape::Surface(W,FVector(-5000,-5000,0),Floor));
   for(int I=0;I<3;++I) { FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn; auto* Unit=W->SpawnActor<ACultPopulation>(Floor+FVector(I*150,0,110),FRotator::ZeroRotator,Spawn); Unit->RecruitTo(0); Unit->PlayerOrder(TEXT("Pray"),Unit->GetActorLocation()); Units.Add(Unit); }
   TArray<FVector> Route;
   for(int R=0;R<36;++R) if(FVector::Dist2D(CultRegionMap::Center(R),P->FeetLocation())>60000&&CultRegionNavigation::Route(W,Units[0]->GetActorLocation(),CultRegionMap::Center(R),Route)) { Destination=CultRegionMap::Center(R); Region=R; break; }
   if(!Test->TestTrue(TEXT("A distant terrain route exists"),Region>=0)) return true;
   D->SelectFollowers({Units[0].Get(),Units[1].Get()}); D->Execute(ACultWorldDirector::PrepareBattle,nullptr,P->FeetLocation());
   Test->TestTrue(TEXT("Prepared expedition requires a group"),D->PreparedCount()==2);
   MapKey(PC,EKeys::M,IE_Pressed); At=Now; Phase=2; return false;
  }
  if(Now-At<.2) return false;
  if(Phase==2) {
   MapKey(PC,EKeys::M,IE_Released); Test->TestTrue(TEXT("Real M opens the map with its own cursor"),H->RegionMapVisible()&&PC->bShowMouseCursor);
   const auto B=FCultRegionMapTestAccess::Map(H);
   for(int R=0;R<36;++R) Test->TestEqual(TEXT("Map projection round-trips every region"),CultRegionMap::Index(CultRegionMap::Unproject(CultRegionMap::Project(CultRegionMap::Center(R),B),B)),R);
   MapMouse(PC,CultRegionMap::Project(Destination,B)); MapKey(PC,EKeys::LeftMouseButton,IE_Pressed); At=Now; Phase=3; return false;
  }
  if(Phase==3) {
   MapKey(PC,EKeys::LeftMouseButton,IE_Released); Test->TestEqual(TEXT("Real map click selects the distant region"),FCultRegionMapTestAccess::Region(H),Region);
   for(int I=0;I<4;++I) { const auto B=FCultRegionMapTestAccess::Button(H,I); FIntRect View; CultHUDSpace::View(PC,View); Test->TestTrue(TEXT("Map action fits inside the view"),B.Min.X>=0&&B.Max.X<=View.Width()&&B.Min.Y>=0&&B.Max.Y<=View.Height()); }
   MapMouse(PC,FCultRegionMapTestAccess::Button(H,0).GetCenter()); MapKey(PC,EKeys::LeftMouseButton,IE_Pressed); At=Now; Phase=4; return false;
  }
  if(Phase==4) {
   MapKey(PC,EKeys::LeftMouseButton,IE_Released);
   for(int I=0;I<2;++I) Test->TestTrue(TEXT("The map sends exactly the prepared troops beyond the local command radius"),Units[I]->GetActivity()==TEXT("Explore")&&Units[I]->HasPlayerBattle()&&FVector::Dist2D(Units[I]->GetCommandPoint(),P->FeetLocation())>30000);
   Test->TestEqual(TEXT("Unselected follower remains at its own job"),Units[2]->GetActivity(),FString(TEXT("Pray")));
   Before=Units[0]->GetActorLocation(); FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots/RegionExpeditions.png")),true,false);
   At=Now; Phase=45; return false;
  }
  if(Phase==45) {
   if(FScreenshotRequest::IsScreenshotRequested()) return false;
   MapKey(PC,EKeys::Escape,IE_Pressed); At=Now; Phase=5; return false;
  }
  if(Phase==5) {
   MapKey(PC,EKeys::Escape,IE_Released); Test->TestFalse(TEXT("Escape closes the map without leaving Free Roam"),H->RegionMapVisible());
   if(Now-At<2) return false;
   Test->TestTrue(TEXT("Dispatched troops physically start marching"),FVector::Dist2D(Units[0]->GetActorLocation(),Before)>60);
   D->SelectFollowers({Units[2].Get()}); D->Execute(ACultWorldDirector::PrepareBattle,nullptr,P->FeetLocation()); D->Execute(ACultWorldDirector::HuntRegion,nullptr,Destination);
   Test->TestTrue(TEXT("Another group can hunt while the first expedition continues"),Units[2]->GetActivity()==TEXT("Hunt")&&Units[0]->GetActivity()==TEXT("Explore"));
   bool TestedRaid=false;
   for(TActorIterator<ACultDestructible> It(W);It;++It) if(It->ActorHasTag(TEXT("LivingCity"))&&!It->IsDestroyed()&&It->GetFaction()!=0) { const FString Result=D->Execute(ACultWorldDirector::RaidRegion,nullptr,It->GetActorLocation()); if(Result.Contains(TEXT("dispatched"))) { TestedRaid=Units[2]->GetActivity()==TEXT("Raid"); break; } }
   Test->TestTrue(TEXT("Regional raiding dispatches to an actual living city"),TestedRaid);
   Test->TestTrue(TEXT("Outside-world orders are rejected"),D->Execute(ACultWorldDirector::ExploreRegion,nullptr,FVector(1e9,0,0)).Contains(TEXT("inside")));
   FVector Refuge; if(ACultLivingLandscape::Surface(W,Units[2]->GetActorLocation()+FVector(6000,6000,0),Refuge)) Units[2]->SetActorLocation(Refuge+FVector(0,0,110));
   FActorSpawnParameters Spawn; Spawn.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
   Quarry=W->SpawnActor<ACultFeralBeast>(Units[2]->GetActorLocation()+FVector(180,0,0),FRotator::ZeroRotator,Spawn);
   Quarry->SetMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"))); Quarry->SetBredHealth(10); Quarry->SetActorTickEnabled(false);
   D->Execute(ACultWorldDirector::HuntRegion,nullptr,Destination);
   Test->TestEqual(TEXT("A hunt is a real persistent combat order"),Units[2]->GetActivity(),FString(TEXT("Hunt")));
   At=Now; Phase=6; return false;
  }
  if(Phase==6) {
   if(Quarry.IsValid()&&!Quarry->IsDead()&&Now-At<8) return false;
   Test->TestTrue(TEXT("A marching hunter physically defeats an ambush outside its destination region"),!Quarry.IsValid()||Quarry->IsDead());
   if(Quarry.IsValid()) Quarry->Destroy();
   D->Execute(ACultWorldDirector::GlobalRetreat,nullptr,P->FeetLocation()); for(auto& Unit:Units) Test->TestFalse(TEXT("Global retreat releases expedition battle assignments"),Unit->HasPlayerBattle());
   for(auto& Unit:Units) if(Unit.IsValid()) Unit->Destroy(); D->SelectFollowers({}); return true;
  }
  return false;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultRegionMapTest,"CultUlhu.FreeRoam.RegionExpeditions",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultRegionMapTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultRegionMapObservation(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
#endif
