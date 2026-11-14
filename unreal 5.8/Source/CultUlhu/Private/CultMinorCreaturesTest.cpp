#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Components/PoseableMeshComponent.h"
#include "CultFeralBeast.h"
#include "CultLivingLandscape.h"
#include "Engine/StaticMeshActor.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultMinorCreaturesTest, "CultUlhu.MinorCreatures",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

class FCultCreatureMovementCheck : public IAutomationLatentCommand
{
public:
	FCultCreatureMovementCheck(FAutomationTestBase* InTest, ACultFeralBeast* InEnemy,AActor* InGoal)
		: Test(InTest), Enemy(InEnemy), Goal(InGoal), Origin(InEnemy->GetActorLocation())
	{
		if (auto* Pose = InEnemy->FindComponentByClass<UPoseableMeshComponent>())
			InitialLeg = Pose->GetBoneRotationByName(TEXT("upperleg_l"), EBoneSpaces::ComponentSpace).Quaternion();
	}
	virtual bool Update() override
	{
		if (!Enemy.IsValid()) { Test->AddError(TEXT("Creature disappeared during movement check")); return true; }
		Elapsed += Enemy->GetWorld()->GetDeltaSeconds();
		if (auto* Pose = Enemy->FindComponentByClass<UPoseableMeshComponent>())
			bAnimated |= InitialLeg.AngularDistance(Pose->GetBoneRotationByName(TEXT("upperleg_l"), EBoneSpaces::ComponentSpace).Quaternion()) > .005f;
		if (Elapsed < 1.2f) return false;
		const float Distance = FVector::Dist2D(Origin, Enemy->GetActorLocation());
		Test->AddInfo(FString::Printf(TEXT("Authored ghoul moved %.1f cm; leg animation=%s"), Distance, bAnimated ? TEXT("yes") : TEXT("no")));
		Test->TestTrue(TEXT("Ghoul walks through the world"), Distance > 20.f);
		Test->TestTrue(TEXT("Ghoul's skeletal legs animate while walking"), bAnimated);
		Enemy->Destroy(); if(Goal.IsValid()) Goal->Destroy();
		return true;
	}
private:
	FAutomationTestBase* Test;
	TWeakObjectPtr<ACultFeralBeast> Enemy; TWeakObjectPtr<AActor> Goal;
	FVector Origin;
	FQuat InitialLeg = FQuat::Identity;
	float Elapsed = 0.f;
	bool bAnimated = false;
};

bool FCultMinorCreaturesTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
		if (Context.World() && Context.World()->IsGameWorld()) World = Context.World();
	if (!World) { AddError(TEXT("Load Dark Wilds as a game world first")); return false; }
	APawn* Player = UGameplayStatics::GetPlayerPawn(World, 0);
	if (!Player) { AddError(TEXT("Player missing")); return false; }
	const FVector Origin = Player->GetActorLocation() + FVector(4000, 3500, 0);
	const TArray<FString> Species = {TEXT("charnel_imp"), TEXT("skittering_ghoul"), TEXT("pale_wight"), TEXT("ossified_brute")};
	const TArray<int32> PartCounts = {9, 8, 7, 9};
	for (int32 I = 0; I < Species.Num(); ++I)
	{
		const FTransform Transform(Origin + FVector(1200 * I, 0, 0));
		auto* Enemy = World->SpawnActorDeferred<ACultFeralBeast>(ACultFeralBeast::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
		if (!Enemy) { AddError(TEXT("Creature spawn failed")); continue; }
		Enemy->SetSpecies(Species[I]); Enemy->FinishSpawning(Transform);
		TestTrue(Species[I] + TEXT(" uses imported creature art"), Enemy->HasCreatureMesh());
 if(auto* Pose=Enemy->FindComponentByClass<UPoseableMeshComponent>()) TestTrue(Species[I]+TEXT(" visual faces forward along movement"),FVector::DotProduct(Pose->GetRightVector(),Enemy->GetActorForwardVector())>.99f);
		TestEqual(Species[I] + TEXT(" includes every skeletal body part"), Enemy->GetCreaturePartCount(), PartCounts[I]);
		TestTrue(Species[I] + TEXT(" deals only minor swipe damage"), Enemy->GetAttackDamage() >= 5.f && Enemy->GetAttackDamage() <= 10.f);
  const int Required[]={1,2,10,15};
  for(int Hit=1;Hit<Required[I];++Hit) Enemy->ApplyDamage(135.f);
  TestFalse(Species[I]+TEXT(" survives until its baseline final hit"),Enemy->IsDead());
  Enemy->ApplyDamage(135.f); TestTrue(Species[I]+TEXT(" dies on the specified baseline hit"),Enemy->IsDead());
		Enemy->Destroy();
	}
 FVector Clearing=Origin; bool Found=false;
 for(int Try=0;Try<100&&!Found;++Try) {
  const float A=Try*2.399963f; FVector Candidate=Origin+FVector(FMath::Cos(A),FMath::Sin(A),0)*FMath::Sqrt(float(Try))*500;
  FVector Floor; if(!ACultLivingLandscape::Surface(World,Candidate,Floor,.9f)) continue;
  bool Safe=true; for(FVector Offset:{FVector(500,0,0),FVector(-500,0,0),FVector(0,500,0),FVector(0,-500,0),FVector(800,0,0)}) { FVector Other; if(!ACultLivingLandscape::Surface(World,Floor+Offset,Other,.9f)||FMath::Abs(Other.Z-Floor.Z)>50) Safe=false; }
  if(!Safe||World->OverlapBlockingTestByChannel(Floor+FVector(0,0,110),FQuat::Identity,ECC_Pawn,FCollisionShape::MakeCapsule(45,100))) continue;
  Clearing=Floor+FVector(0,0,120); Found=true;
 }
 if(!TestTrue(TEXT("Movement art test has a clear, walkable procedural site"),Found)) return false;
 const FTransform Transform(Clearing);
 auto* Ghoul = World->SpawnActorDeferred<ACultFeralBeast>(ACultFeralBeast::StaticClass(), Transform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn);
	if (!Ghoul) return false;
	Ghoul->SetSpecies(TEXT("skittering_ghoul")); Ghoul->FinishSpawning(Transform);
	auto* Goal=World->SpawnActor<AStaticMeshActor>(Clearing+FVector(800,0,0),FRotator::ZeroRotator);
 Ghoul->Capture(Goal); // deterministic escort motion, independent of random wander direction
 ADD_LATENT_AUTOMATION_COMMAND(FCultCreatureMovementCheck(this, Ghoul,Goal));
	return true;
}
#endif
