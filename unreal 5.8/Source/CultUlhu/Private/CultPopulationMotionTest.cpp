#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationEditorCommon.h"
#include "CultAutomationPIE.h"
#include "CultPopulation.h"
#include "CultUlhuGameInstance.h"
#include "Engine/Engine.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshActor.h"
#include "Components/StaticMeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "HAL/IConsoleManager.h"

class FCultPopulationMotionObservation:public IAutomationLatentCommand {
 FAutomationTestBase* Test; int Phase=0; double Began=FPlatformTime::Seconds(); float At=0,Before=0,HoldAt=0; bool HoldStarted=false;
 FVector Stopped; TWeakObjectPtr<ACultPopulation> Unit,Enemy; TWeakObjectPtr<AStaticMeshActor> Floor;
public:
 explicit FCultPopulationMotionObservation(FAutomationTestBase* In):Test(In) {
  auto* Interval=IConsoleManager::Get().FindConsoleVariable(TEXT("cult.Population.DecisionInterval")); Before=Interval->GetFloat(); Interval->Set(2.f,ECVF_SetByCode);
 }
 ~FCultPopulationMotionObservation() {
  if(Unit.IsValid()) Unit->Destroy(); if(Enemy.IsValid()) Enemy->Destroy(); if(Floor.IsValid()) Floor->Destroy();
  IConsoleManager::Get().FindConsoleVariable(TEXT("cult.Population.DecisionInterval"))->Set(Before,ECVF_SetByCode);
 }
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>120) { Test->AddError(TEXT("Native population movement test timed out")); return true; }
  UWorld* World=nullptr; for(auto& C:GEngine->GetWorldContexts()) if(C.World()&&C.WorldType==EWorldType::PIE) World=C.World(); if(!World) return false;
  if(World->GetName().Contains(TEXT("L_MainMenu"))) {
   if(Phase==0) { auto* Game=Cast<UCultUlhuGameInstance>(World->GetGameInstance()); Game->SelectedCharacterId=TEXT("mi_go"); Game->OpenCharacterSelect(ECultPlayMode::FreeRoam); Game->SetInitialBeliefs({5,6,11}); Game->StartPlay(); Phase=1; } return false;
  }
  // A preceding test may still be ending PIE while FStartPIECommand starts the
  // menu world. Never inspect a subject before this fixture requested gameplay.
  if(Phase==0) return false;
  auto* PC=World->GetFirstPlayerController(); if(!PC||!PC->GetPawn()||World->GetTimeSeconds()<4) return false;
  if(Phase==1) {
   Floor=World->SpawnActor<AStaticMeshActor>(FVector(0,0,45000),FRotator::ZeroRotator);
   auto* Mesh=Floor->GetStaticMeshComponent(); Mesh->SetMobility(EComponentMobility::Movable); Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube"))); Floor->SetActorScale3D(FVector(60,30,.2));
   Unit=World->SpawnActor<ACultPopulation>(FVector(0,0,45110),FRotator::ZeroRotator); Unit->RecruitTo(0); Unit->SetLoyalty(0,false);
   Unit->PlayerOrder(TEXT("Patrol"),FVector(900,0,45110)); At=World->GetTimeSeconds(); Phase=2; return false;
  }
  if(!Unit.IsValid()) { Test->AddError(TEXT("Movement subject disappeared")); return true; }
  const float Elapsed=World->GetTimeSeconds()-At;
  // Native control projection is a short-lived snapshot; refresh it as an
  // active caster does instead of accidentally testing an expired stun.
  if(Phase==4) Unit->SetControlState(true,false,1,Unit->GetActorLocation(),!HoldStarted||World->GetTimeSeconds()-HoldAt<.25f,-8511);
  if(Phase==2&&Elapsed>.4f) {
   Test->TestTrue(TEXT("Native character physics keeps moving between infrequent decisions"),Unit->GetVelocity().X>80&&Unit->GetActorLocation().X>20);
   Unit->PlayerOrder(TEXT("Patrol"),FVector(-900,0,45110)); At=World->GetTimeSeconds(); Phase=3;
  } else if(Phase==3&&Elapsed>.4f) {
   Test->TestTrue(TEXT("A new player order reverses movement before the two-second cached decision expires"),Unit->GetVelocity().X<-80);
   Unit->Capture(PC->GetPawn(),0); Unit->SetControlState(true,false,1,Unit->GetActorLocation(),true,-8511);
   Stopped=Unit->GetActorLocation(); At=World->GetTimeSeconds(); Phase=4;
  } else if(Phase==4&&Elapsed>.05f&&!HoldStarted) {
   Stopped=Unit->GetActorLocation(); HoldAt=World->GetTimeSeconds(); HoldStarted=true;
  } else if(Phase==4&&HoldStarted&&World->GetTimeSeconds()-HoldAt>.5f) {
   Test->TestTrue(TEXT("Stun and root both prevent a captive following its captor"),FVector::Dist2D(Stopped,Unit->GetActorLocation())<3&&Unit->GetVelocity().Size2D()<1);
   Unit->ReleaseCaptive(); Unit->SetControlState(false,false,1,FVector::ZeroVector,false,-8511);
   Unit->PlayerOrder(TEXT("Patrol"),FVector(900,0,45110)); At=World->GetTimeSeconds(); Phase=5;
  } else if(Phase==5&&Elapsed>.4f) {
   Test->TestTrue(TEXT("Releasing control resumes native movement immediately"),Unit->GetVelocity().X>80);
   IConsoleManager::Get().FindConsoleVariable(TEXT("cult.Population.DecisionInterval"))->Set(.1f,ECVF_SetByCode);
   Enemy=World->SpawnActor<ACultPopulation>(Unit->GetActorLocation()+FVector(250,0,0),FRotator::ZeroRotator); Enemy->RecruitTo(1); Enemy->SetActorTickEnabled(false); Enemy->GetCharacterMovement()->DisableMovement();
   Unit->SetLoyalty(0,false); Test->TestEqual(TEXT("Battle order is issued at zero native loyalty"),Unit->GetLoyalty(),0.f);
   Unit->PlayerOrder(TEXT("Attack"),Enemy->GetActorLocation(),Enemy.Get(),true); At=World->GetTimeSeconds(); Phase=6;
  } else if(Phase==6&&(Enemy->GetHealth()<100||Elapsed>3)) {
   Test->TestTrue(TEXT("A zero-loyalty battle order still lands a real delayed physical hit"),Enemy->GetHealth()<100&&Unit->HasPlayerBattle());
   Enemy->Destroy(); Enemy.Reset(); Unit->PlayerOrder(TEXT("Patrol"),FVector(10000,0,45110)); At=World->GetTimeSeconds(); Phase=7;
  } else if(Phase==7&&Elapsed>16) {
   Test->TestTrue(TEXT("Cached decisions retain ground lookahead at a real platform edge"),Unit->GetActorLocation().X>2400&&Unit->GetActorLocation().X<2958&&Unit->GetCharacterMovement()->IsMovingOnGround());
   Test->TestTrue(TEXT("The character brakes instead of continuously walking off the edge"),Unit->GetVelocity().Size2D()<5);
   Test->AddInfo(FString::Printf(TEXT("Native order reversal, control interruption/recovery, physical combat and cliff stop verified; final X=%.1f"),Unit->GetActorLocation().X)); return true;
  }
  return false;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultPopulationMotionTest,"CultUlhu.Menu.PopulationMotion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultPopulationMotionTest::RunTest(const FString&) {
 ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultPopulationMotionObservation(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true;
}
#endif
