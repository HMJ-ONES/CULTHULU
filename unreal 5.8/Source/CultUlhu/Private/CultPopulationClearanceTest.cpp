#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CultAutomationPIE.h"
#include "CultPopulation.h"
#include "CultUlhuGameInstance.h"
#include "Engine/StaticMeshActor.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"

// Physical navigation regressions: an impassable low beam, a completely blocked
// surround, and an ordinary walkable incline. No teleport or manual actor Tick.
class FCultPopulationClearanceObservation : public IAutomationLatentCommand {
 FAutomationTestBase* Test; int32 Phase=0; double Began=FPlatformTime::Seconds(); float At=0;
 TArray<TWeakObjectPtr<AActor>> Fixtures; TWeakObjectPtr<ACultPopulation> Beam,Boxed,Ramp;
 static constexpr float Top=45010.f;
 AStaticMeshActor* Block(UWorld* World,FVector Center,FVector Size,FRotator Rotation=FRotator::ZeroRotator) {
  auto* Actor=World->SpawnActor<AStaticMeshActor>(Center,Rotation);
  auto* Mesh=Actor->GetStaticMeshComponent(); Mesh->SetMobility(EComponentMobility::Movable);
  Mesh->SetStaticMesh(LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cube.Cube")));
  Actor->SetActorScale3D(Size/100.f); Fixtures.Add(Actor); return Actor;
 }
 ACultPopulation* Person(UWorld* World,FVector Origin) {
  Block(World,Origin+FVector(0,0,45000),FVector(6000,3000,20));
  auto* P=World->SpawnActor<ACultPopulation>(Origin+FVector(0,0,45110),FRotator::ZeroRotator);
  P->RecruitTo(0); Fixtures.Add(P); return P;
 }
public:
 explicit FCultPopulationClearanceObservation(FAutomationTestBase* In):Test(In) {}
 ~FCultPopulationClearanceObservation() { for(auto& Actor:Fixtures) if(Actor.IsValid()) Actor->Destroy(); }
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>90) { Test->AddError(TEXT("Physical population clearance fixture timed out")); return true; }
  UWorld* World=GEditor->PlayWorld.Get(); if(!World||World->bIsTearingDown) return false;
  if(World->GetName().Contains(TEXT("L_MainMenu"))) {
   if(Phase==0) { auto* Game=Cast<UCultUlhuGameInstance>(World->GetGameInstance()); Game->SelectedCharacterId=TEXT("mi_go"); Game->OpenCharacterSelect(ECultPlayMode::FreeRoam); Game->SetInitialBeliefs({5,6,11}); Game->StartPlay(); Phase=1; }
   return false;
  }
  if(Phase==0||World->GetTimeSeconds()<4) return false;
  if(Phase==1) {
   const FVector A(600000,0,0),B(604000,0,0),C(608000,0,0);
   Beam=Person(World,A); Boxed=Person(World,B); Ramp=Person(World,C);
   const float Half=Beam->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
   // The old center-height sphere fits beneath this slab, but the head cannot.
   Block(World,A+FVector(300,0,Top+Half+65),FVector(300,400,20));
   for(FVector Offset:{FVector(100,0,0),FVector(-100,0,0),FVector(0,100,0),FVector(0,-100,0)})
    Block(World,B+Offset+FVector(0,0,Top+150),Offset.X!=0?FVector(20,300,300):FVector(300,20,300));
   Block(World,C+FVector(600,0,45190),FVector(1000,500,20),FRotator(20,0,0));
   Beam->PlayerOrder(TEXT("Patrol"),A+FVector(1000,0,45110));
   Boxed->PlayerOrder(TEXT("Patrol"),B+FVector(1000,0,45110));
   Ramp->PlayerOrder(TEXT("Patrol"),C+FVector(1000,0,45500));
   At=World->GetTimeSeconds(); Phase=2; return false;
  }
  if(!Beam.IsValid()||!Boxed.IsValid()||!Ramp.IsValid()) { Test->AddError(TEXT("Physical clearance subject disappeared")); return true; }
  if(World->GetTimeSeconds()-At<8) return false;
  Test->TestTrue(TEXT("A person routes around a low head obstruction and reaches the far side"),Beam->GetActorLocation().X>600650&&Beam->GetCharacterMovement()->IsMovingOnGround());
  Test->TestTrue(TEXT("All blocked directions produce no persistent movement input"),Boxed->GetLastMovementInputVector().IsNearlyZero());
  Test->TestTrue(TEXT("A fully surrounded person remains near its safe starting point"),FVector::Dist2D(Boxed->GetActorLocation(),FVector(604000,0,0))<10);
  const float RampFeet=Ramp->GetActorLocation().Z-Ramp->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
  Test->TestTrue(TEXT("Full-body avoidance still permits an ordinary walkable incline"),Ramp->GetActorLocation().X>608500&&RampFeet>Top+100&&Ramp->GetCharacterMovement()->IsMovingOnGround());
  Test->AddInfo(FString::Printf(TEXT("Clearance: beam=%s boxed=%s input=%s ramp=%s feet=%.1f"),*Beam->GetActorLocation().ToCompactString(),*Boxed->GetActorLocation().ToCompactString(),*Boxed->GetLastMovementInputVector().ToCompactString(),*Ramp->GetActorLocation().ToCompactString(),RampFeet));
  return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultPopulationClearanceTest,"CultUlhu.Menu.PopulationClearance",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultPopulationClearanceTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultPopulationClearanceObservation(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
#endif
