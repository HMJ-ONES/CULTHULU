#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "CultWorldDirector.h"
#include "CultPopulation.h"
#include "CultDestructible.h"
#include "CultUlhuGameInstance.h"
#include "core/Events.h"
#include "Camera/CameraActor.h"
#include "GameFramework/PlayerController.h"
#include "UnrealClient.h"
#include "Misc/Paths.h"
#if WITH_EDITOR
#include "Tests/AutomationEditorCommon.h"
#include "Tests/AutomationCommon.h"
#endif

FString CultTestConstructionReport(ACultWorldDirector*);
class FCultSocietyObservation:public IAutomationLatentCommand
{
public:
 FCultSocietyObservation(FAutomationTestBase* T,bool Capture):Test(T),Visual(Capture),Started(FPlatformTime::Seconds()){}
 bool Update() override {
  UWorld* W=nullptr; for(auto& C:GEngine->GetWorldContexts()) if(C.World()&&C.World()->IsGameWorld()) W=C.World();
  if(W&&W->GetName().Contains(TEXT("L_MainMenu"))&&!Entered) { if(auto* GI=Cast<UCultUlhuGameInstance>(W->GetGameInstance())) { GI->SelectedCharacterId=TEXT("cthulhu_avatar"); GI->OpenCharacterSelect(ECultPlayMode::FreeRoam); GI->SetInitialBeliefs({11,5,6}); GI->StartPlay(); Entered=true; } return false; }
  auto* D=W&&W->GetName().Contains(TEXT("L_DarkWilds"))?ACultWorldDirector::Find(W):nullptr;
  if(!D) { if(FPlatformTime::Seconds()-Started>30) { Test->AddError(TEXT("Society observation never reached the game world")); return true; } return false; }
  if(W->GetTimeSeconds()<2) return false;
  if(WorldStart<0) {
   WorldStart=W->GetTimeSeconds(); int Cults=0,Humans=0,Captives=0;
   for(TActorIterator<ACultPopulation> It(W);It;++It) { Cults+=It->IsCultist(); Humans+=It->HumanPartCount()>=6&&It->HasAuthoredHumanModel(); Captives+=It->IsCaptive(); }
   Test->TestTrue(TEXT("A substantial actual cult population inhabits the map"),Cults>=40); Test->TestTrue(TEXT("People have visible articulated humanoid bodies"),Humans>=40); Test->TestTrue(TEXT("Temple prisoners are real people in the world"),Captives>=3);
   for(int E=0;E<int(cultulhu::EventType::Count);++E) for(int F=0;F<3;++F) Initial.FindOrAdd(E)+=D->FaithEventCount(E,F);
   UE_LOG(LogTemp,Log,TEXT("[CultUlhu] Society observation initial enemy deaths: %d"),Initial.FindRef(int(cultulhu::EventType::EnemyCultistSlain)));
   if(Visual) {
    Camera=W->SpawnActor<ACameraActor>(FVector(-10400,-13800,850),FRotator(-16,145,0));
    Camera->SetActorRotation((FVector(-12100,-12300,200)-Camera->GetActorLocation()).Rotation()); W->GetFirstPlayerController()->SetViewTarget(Camera);
   }
  }
  const float Elapsed=W->GetTimeSeconds()-WorldStart;
  if(Visual&&Elapsed>10&&!PrayerShot) { FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots/CultSocietyWork.png")),true,false); PrayerShot=true; }
  if(Visual&&Elapsed>40&&!RiteShot) { FScreenshotRequest::RequestScreenshot(FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir()/TEXT("Screenshots/CultSocietyRitual.png")),true,false); RiteShot=true; }
  if(Elapsed<130) return false;
  using E=cultulhu::EventType;
  auto Count=[&](E Type) { int N=0; for(int F=0;F<3;++F) N+=D->FaithEventCount(int(Type),F); return N-Initial.FindRef(int(Type)); };
  Test->TestTrue(TEXT("Cultists complete autonomous torture on real captives"),Count(E::TorturePerformed)+Count(E::CapturedTortured)>0);
  Test->TestTrue(TEXT("Cultists complete an autonomous real sacrifice"),Count(E::SacrificeCompleted)>0);
  Test->TestTrue(TEXT("Actual prayer completes during ordinary world play"),Count(E::PrayerOffered)>0);
  Test->TestTrue(TEXT("Real people convert during ordinary world play"),Count(E::ConversionPerformed)>0);
  Test->TestTrue(TEXT("Cultists finish actual construction"),Count(E::BuildCompleted)>0);
  if(Count(E::BuildCompleted)==0) Test->AddInfo(CultTestConstructionReport(D));
  Test->TestTrue(TEXT("Faction warfare produces actual enemy cultist deaths in this live world"),Count(E::EnemyCultistSlain)+Initial.FindRef(int(E::EnemyCultistSlain))>0);
  Test->TestTrue(TEXT("Raids destroy actual city buildings"),Count(E::CityBuildingDestroyed)>0);
  Test->TestTrue(TEXT("Faith activity raises the war faction's actual combat strength"),D->CombatMultiplier(1)>1.10f);
  Test->AddInfo(FString::Printf(TEXT("Initial war kills %d. 130s society: torture %d, sacrifices %d, prayers %d, conversions %d, construction %d, war kills %d, city buildings destroyed %d; strength x%.2f / x%.2f / x%.2f"),Initial.FindRef(int(E::EnemyCultistSlain)),Count(E::TorturePerformed)+Count(E::CapturedTortured),Count(E::SacrificeCompleted),Count(E::PrayerOffered),Count(E::ConversionPerformed),Count(E::BuildCompleted),Count(E::EnemyCultistSlain),Count(E::CityBuildingDestroyed),D->CombatMultiplier(0),D->CombatMultiplier(1),D->CombatMultiplier(2)));
  if(Camera) { W->GetFirstPlayerController()->SetViewTarget(W->GetFirstPlayerController()->GetPawn()); Camera->Destroy(); } return true;
 }
private:
 FAutomationTestBase* Test; bool Visual,Entered=false,PrayerShot=false,RiteShot=false; double Started; float WorldStart=-1; TMap<int,int> Initial; ACameraActor* Camera=nullptr;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultSocietyGameplayTest,"CultUlhu.Society",
 EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FCultSocietyGameplayTest::RunTest(const FString& Parameters) { ADD_LATENT_AUTOMATION_COMMAND(FCultSocietyObservation(this,false)); return true; }
#if WITH_EDITOR
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultSocietyEditorTest,"CultUlhu.Menu.Society",
 EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FCultSocietyEditorTest::RunTest(const FString& Parameters) {
 ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false)); ADD_LATENT_AUTOMATION_COMMAND(FWaitLatentCommand(1.f)); ADD_LATENT_AUTOMATION_COMMAND(FCultSocietyObservation(this,true)); return true;
}
#endif
#endif
