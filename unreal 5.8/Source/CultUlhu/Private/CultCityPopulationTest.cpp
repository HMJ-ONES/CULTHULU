#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "CultAutomationPIE.h"
#include "CultUlhuGameInstance.h"
#include "CultPopulation.h"
#include "CultDestructible.h"
#include "CultFeralBeast.h"
#include "CultStructurePlacement.h"
#include "Components/CapsuleComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/OverlapResult.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"

// Observe actual generated settlements immediately after world initialization,
// before a long-running movement test can obscure an initial placement overlap.
class FCultCityPopulationObservation : public IAutomationLatentCommand {
 FAutomationTestBase* Test; double Began=FPlatformTime::Seconds(); int32 Round=0; bool Entered=false;
 const TArray<int32> Seeds={11,29,73,151};
public:
 explicit FCultCityPopulationObservation(FAutomationTestBase* In):Test(In) {}
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>180) { Test->AddError(TEXT("Generated-city population check timed out")); return true; }
  auto* World=GEditor->PlayWorld.Get(); if(!World||World->bIsTearingDown) return false;
  if(World->GetName().Contains(TEXT("L_MainMenu"))) {
   if(!Entered) { FMath::RandInit(Seeds[Round]); auto* Game=Cast<UCultUlhuGameInstance>(World->GetGameInstance()); Game->SelectedCharacterId=TEXT("mi_go"); Game->OpenCharacterSelect(ECultPlayMode::FreeRoam); Game->SetInitialBeliefs({5,6,11}); Game->StartPlay(); Entered=true; }
   return false;
  }
  if(!Entered||!World->HasBegunPlay()||!World->AreActorsInitialized()) return false;
  int32 Buildings=0,People=0,Embedded=0; FString Details;
  for(TActorIterator<ACultDestructible> It(World);It;++It) if(It->ActorHasTag(TEXT("LivingCity"))) ++Buildings;
  FCollisionObjectQueryParams Objects; Objects.AddObjectTypesToQuery(ECC_WorldStatic); Objects.AddObjectTypesToQuery(ECC_WorldDynamic);
  for(TActorIterator<ACultPopulation> It(World);It;++It) {
   ++People; auto* Cap=It->GetCapsuleComponent(); TArray<FOverlapResult> Hits;
   FCollisionQueryParams Query(SCENE_QUERY_STAT(CityPopulationClearance),false,*It);
   World->OverlapMultiByObjectType(Hits,It->GetActorLocation(),FQuat::Identity,Objects,
    FCollisionShape::MakeCapsule(Cap->GetScaledCapsuleRadius()-.5f,Cap->GetScaledCapsuleHalfHeight()-.5f),Query);
   for(const auto& Hit:Hits) if(auto* Building=Cast<ACultDestructible>(Hit.GetActor())) if(Building->ActorHasTag(TEXT("LivingCity"))) {
    ++Embedded; if(Embedded<=8) Details+=FString::Printf(TEXT("%s at %s in %s; "),*It->GetName(),*It->GetActorLocation().ToCompactString(),*Building->GetName()); break;
   }
  }
  Test->TestTrue(TEXT("Placement check retains actual populated generated cities"),Buildings>=50&&People>=60);
  Test->TestEqual(FString::Printf(TEXT("Initial city bodies never intersect completed settlement geometry (seed %d)"),Seeds[Round]),Embedded,0);
  Test->AddInfo(FString::Printf(TEXT("City placement seed=%d time=%.3f buildings=%d people=%d embedded=%d %s"),Seeds[Round],World->GetTimeSeconds(),Buildings,People,Embedded,*Details));
  if(++Round<Seeds.Num()) { Entered=false; UGameplayStatics::OpenLevel(World,FName(TEXT("/Game/Maps/L_MainMenu"))); return false; }
  return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultCityPopulationTest,"CultUlhu.Menu.CityPopulationClearance",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultCityPopulationTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultCityPopulationObservation(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }

class FCultSettlementFootprintObservation : public IAutomationLatentCommand {
 FAutomationTestBase* Test; bool Entered=false; double Began=FPlatformTime::Seconds(); TArray<TWeakObjectPtr<AActor>> Fixtures;
public:
 explicit FCultSettlementFootprintObservation(FAutomationTestBase* In):Test(In) {}
 ~FCultSettlementFootprintObservation() { for(auto& Actor:Fixtures) if(Actor.IsValid()) Actor->Destroy(); }
 bool Update() override {
  if(FPlatformTime::Seconds()-Began>90) { Test->AddError(TEXT("Settlement footprint fixture timed out")); return true; }
  auto* World=GEditor->PlayWorld.Get(); if(!World||World->bIsTearingDown) return false;
  if(World->GetName().Contains(TEXT("L_MainMenu"))) {
   if(!Entered) { auto* Game=Cast<UCultUlhuGameInstance>(World->GetGameInstance()); Game->SelectedCharacterId=TEXT("mi_go"); Game->OpenCharacterSelect(ECultPlayMode::FreeRoam); Game->SetInitialBeliefs({5,6,11}); Game->StartPlay(); Entered=true; }
   return false;
  }
  if(!Entered||!World->HasBegunPlay()) return false;
  const FString Name=TEXT("stone-wall-damaged"); auto* Mesh=CultStructurePlacement::LoadMesh(Name);
  Test->TestNotNull(TEXT("Footprint check uses the actual imported city wall"),Mesh); if(!Mesh) return true;
  const FVector Ground(600000,0,45010),Size(1200,200,1000); const float Yaw=90;
  const auto Transform=CultStructurePlacement::MakeTransform(Mesh,Ground,Size,Yaw);
  const auto Center=Transform.TransformPosition(Mesh->GetBoundingBox().GetCenter());
  const FVector Occupied=FVector(Center.X,Center.Y,Ground.Z+100)+Transform.GetRotation().GetForwardVector()*450;
  FActorSpawnParameters Params; Params.SpawnCollisionHandlingOverride=ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
  auto* Person=World->SpawnActor<ACultPopulation>(Occupied,FRotator::ZeroRotator,Params); Fixtures.Add(Person);
  Person->GetCharacterMovement()->DisableMovement(); Person->SetActorTickEnabled(false);
  auto* Rejected=CultStructurePlacement::Spawn(World,Name,Ground,Size,Yaw,-1,true); if(Rejected) Fixtures.Add(Rejected);
  Test->TestNull(TEXT("A rotated future building cannot materialize over an existing civilian"),Rejected);
  Test->TestTrue(TEXT("Rejected placement preserves the civilian's actual position"),Person->GetActorLocation().Equals(Occupied,.01));
  // Close to the same wall, but outside its rotated narrow side: placement must
  // remain possible rather than rejecting every site with anyone nearby.
  const FVector Outside(Center.X+350,Center.Y,Ground.Z+100); Person->SetActorLocation(Outside,false,nullptr,ETeleportType::TeleportPhysics);
  auto* Allowed=CultStructurePlacement::Spawn(World,Name,Ground,Size,Yaw,-1,true); if(Allowed) Fixtures.Add(Allowed);
  Test->TestNotNull(TEXT("Clear rotated footprint still creates the original city structure"),Allowed);
  if(Allowed) {
   const auto Bounds=Allowed->GetComponentsBoundingBox();
   Test->TestTrue(TEXT("Original mesh base remains on the requested ground plane"),FMath::IsNearlyEqual(Bounds.Min.Z,Ground.Z,1.));
   Test->TestTrue(TEXT("Requested dimensions and yaw agree with the actual spawned mesh bounds"),Bounds.GetSize().Equals(FVector(Size.Y,Size.X,Size.Z),1.));
  }
  FCollisionQueryParams Query(SCENE_QUERY_STAT(SettlementFixtureBody),false,Person);
  Test->TestFalse(TEXT("Accepted nearby placement leaves the real civilian capsule unblocked"),World->OverlapBlockingTestByChannel(Outside,FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(Person->GetCapsuleComponent()->GetScaledCapsuleRadius(),Person->GetCapsuleComponent()->GetScaledCapsuleHalfHeight()),Query));
  if(Allowed) Allowed->Destroy();
  auto* Beast=World->SpawnActorDeferred<ACultFeralBeast>(ACultFeralBeast::StaticClass(),FTransform(Occupied),nullptr,nullptr,ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
  Beast->SetSpecies(TEXT("skittering_ghoul")); Beast->FinishSpawning(FTransform(Occupied)); Beast->SetActorTickEnabled(false); Fixtures.Add(Beast);
  auto* BeastRejected=CultStructurePlacement::Spawn(World,Name,Ground,Size,Yaw,-1,true); if(BeastRejected) Fixtures.Add(BeastRejected);
  Test->TestNull(TEXT("Settlement placement also protects a live non-pawn creature body"),BeastRejected);
  return true;
 }
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultSettlementFootprintTest,"CultUlhu.Menu.SettlementFootprints",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCultSettlementFootprintTest::RunTest(const FString&) { ADD_LATENT_AUTOMATION_COMMAND(FCultStartFreshPIE()); ADD_LATENT_AUTOMATION_COMMAND(FCultSettlementFootprintObservation(this)); ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand()); return true; }
#endif
