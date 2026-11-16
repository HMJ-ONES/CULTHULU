// Run in the Dark Wilds game world with Automation RunTests CultUlhu.GroundSpawn.
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "CultUlhuPlayerPawn.h"
#include "CultUlhuGameInstance.h"
#include "CultDestructible.h"
#include "CultPopulation.h"
#include "GameFramework/PlayerController.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCultGroundSpawnTest, "CultUlhu.GroundSpawn",
	EAutomationTestFlags::ClientContext | EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

class FCultTraversalCheck : public IAutomationLatentCommand
{
public:
	FCultTraversalCheck(FAutomationTestBase* InTest, ACultUlhuPlayerPawn* InPawn) : Test(InTest), Pawn(InPawn) {}
	virtual bool Update() override
	{
		if (!Pawn.IsValid()) { Test->AddError(TEXT("Pawn lost during traversal")); return true; }
		if (Stage == 0)
		{
			Origin = Pawn->GetActorLocation();
			Pawn->Jump(); Stage = 1;
            // Let movement consume the press before a slow test frame releases it.
            return false;
		}
		Elapsed += Pawn->GetWorld()->GetDeltaSeconds();
		if (Stage == 1)
		{
			MaxRise = FMath::Max(MaxRise, Pawn->GetActorLocation().Z - Origin.Z);
			if(Elapsed>.1f) Pawn->StopJumping();
   if(Elapsed<2.5f || (!Pawn->GetCharacterMovement()->IsMovingOnGround() && Elapsed<6.f)) return false;
   Test->AddInfo(FString::Printf(TEXT("Landing check time %.2f mode %d velocity %s"),Elapsed,int(Pawn->GetCharacterMovement()->MovementMode),*Pawn->GetVelocity().ToString()));
			Test->AddInfo(FString::Printf(TEXT("Jump rise %.1f cm"), MaxRise));
			Test->TestTrue(TEXT("Giant jump clears at least 5 meters"), MaxRise > 500.f);
			Test->TestTrue(TEXT("Jump lands on terrain"), Pawn->GetCharacterMovement()->IsMovingOnGround());
			Pawn->SetActorLocation(FVector(1000000, 1000000, Origin.Z), false, nullptr, ETeleportType::TeleportPhysics);
			Stage = 2; Elapsed = 0.f; return false;
		}
		if (Elapsed < .2f) return false;
		Test->TestTrue(TEXT("Outside-map excursion recovers onto terrain"), FVector::Dist2D(Pawn->GetActorLocation(), Origin) < 10000.f);
		Test->TestTrue(TEXT("Recovered character can walk"), Pawn->GetCharacterMovement()->IsMovingOnGround());
		return true;
	}
private:
	FAutomationTestBase* Test;
	TWeakObjectPtr<ACultUlhuPlayerPawn> Pawn;
	FVector Origin = FVector::ZeroVector;
	float MaxRise = 0.f;
	float Elapsed = 0.f;
	int32 Stage = 0;
};

class FCultWalkCheck : public IAutomationLatentCommand
{
public:
	FCultWalkCheck(FAutomationTestBase* InTest, ACultUlhuPlayerPawn* InPawn)
		: Test(InTest), Pawn(InPawn), Start(InPawn->GetActorLocation()) {}
	virtual bool Update() override
	{
		if (!Pawn.IsValid()) { Test->AddError(TEXT("Pawn lost during movement")); return true; }
		Elapsed += Pawn->GetWorld()->GetDeltaSeconds();
		if (Elapsed < 2.f)
		{
			Pawn->AddMovementInput(FVector::ForwardVector, 1.f);
			return false;
		}
		Test->AddInfo(FString::Printf(TEXT("Walk distance %.1f cm; final position %s"), FVector::Dist2D(Start, Pawn->GetActorLocation()), *Pawn->GetActorLocation().ToString()));
		Test->TestTrue(TEXT("Character moves across terrain"), FVector::Dist2D(Start, Pawn->GetActorLocation()) > 100.f);
		Test->TestTrue(TEXT("Character remains grounded"), Pawn->GetCharacterMovement()->IsMovingOnGround());
		return true;
	}
private:
	FAutomationTestBase* Test;
	TWeakObjectPtr<ACultUlhuPlayerPawn> Pawn;
	FVector Start;
	float Elapsed = 0.f;
};

bool FCultGroundSpawnTest::RunTest(const FString& Parameters)
{
	UWorld* World = nullptr;
	for (const FWorldContext& Context : GEngine->GetWorldContexts())
		if (Context.World() && Context.World()->IsGameWorld()) World = Context.World();
	if (!World) { AddError(TEXT("Load L_DarkWilds as a game world first")); return false; }
	ACultUlhuPlayerPawn* Pawn = nullptr;
	for (TActorIterator<ACultUlhuPlayerPawn> It(World); It; ++It) { if(!It->IsBot()) { Pawn = *It; break; } }
	if (!Pawn) { AddError(TEXT("Player pawn missing")); return false; }
	TestTrue(TEXT("Walking enabled after spawn"), Pawn->GetCharacterMovement()->IsMovingOnGround());
	float HeightM = 30.f;
	if (UCultUlhuGameInstance* GI = Cast<UCultUlhuGameInstance>(World->GetGameInstance()))
		for (const FCultRosterEntry& Entry : UCultUlhuGameInstance::Roster())
			if (Entry.Id == GI->SelectedCharacterId) HeightM = Entry.HeightM;
	TestTrue(TEXT("Roster scale preserved"), FMath::IsNearlyEqual(Pawn->GetActorScale3D().X, HeightM / 1.8f));
	const float WalkingSpeed = Pawn->GetCharacterMovement()->MaxWalkSpeed;
	Pawn->SetSprinting(true);
	TestTrue(TEXT("Sprint speed exceeds walking by at least 70 percent"), Pawn->GetCharacterMovement()->MaxWalkSpeed > WalkingSpeed * 1.7f);
	Pawn->SetSprinting(false);
	if (auto* Controller = Cast<APlayerController>(Pawn->GetController()))
	{
		const float Yaw = Controller->GetControlRotation().Yaw;
		Pawn->LookYaw(20.f); Controller->UpdateRotation(.016f);
		TestTrue(TEXT("Look input rotates the camera controller"), FMath::Abs(FMath::FindDeltaAngleDegrees(Yaw, Controller->GetControlRotation().Yaw)) > 1.f);
	}
	const bool ThirdPerson = Pawn->IsThirdPerson();
	Pawn->ToggleCamera(); TestTrue(TEXT("Camera mode toggles"), Pawn->IsThirdPerson() != ThirdPerson); Pawn->ToggleCamera();
	Pawn->ReceiveDamage(100.f);
 TestEqual(TEXT("Original character max health applies"), Pawn->GetHealth(), Pawn->GetMaxHealth()-100.f);
	const float HalfHeight = Pawn->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
	const FVector Feet = Pawn->GetActorLocation() - FVector(0, 0, HalfHeight);
	FActorSpawnParameters Spawn;
	Spawn.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
	if (UCultUlhuGameInstance* GI = Cast<UCultUlhuGameInstance>(World->GetGameInstance()))
	{
		const FString OriginalId = GI->SelectedCharacterId;
		GI->SelectedCharacterId = TEXT("mi_go");
		ACultUlhuPlayerPawn* SmallerPawn = World->SpawnActor<ACultUlhuPlayerPawn>(Pawn->GetActorLocation(), FRotator::ZeroRotator, Spawn);
		if (TestNotNull(TEXT("20 meter roster entity spawned"), SmallerPawn))
		{
			TestTrue(TEXT("20 meter entity retains roster scale"), FMath::IsNearlyEqual(SmallerPawn->GetActorScale3D().X, 20.f / 1.8f));
			TestTrue(TEXT("20 meter entity finds walkable ground"), SmallerPawn->GetCharacterMovement()->IsMovingOnGround());
			SmallerPawn->Destroy();
		}
		GI->SelectedCharacterId = OriginalId;
	}
	ACultDestructible* Target = World->SpawnActor<ACultDestructible>(Feet + FVector(100, 0, 0), FRotator::ZeroRotator, Spawn);
	if (TestNotNull(TEXT("Combat target created"), Target))
	{
		const float Health = Target->GetHealth();
		Pawn->Smash();
		TestEqual(TEXT("An aimed swing does not instantly damage a nearby collisionless target"),Target->GetHealth(),Health);
  TestTrue(TEXT("The swing enters its timed animation"),Pawn->IsSwinging());
		Target->Destroy();
	}
	auto* Civilian = World->SpawnActor<ACultPopulation>(Feet + FVector(1000, 0, 100), FRotator::ZeroRotator, Spawn);
	if (Civilian)
	{
		Pawn->Interact();
 TestFalse(TEXT("E does not replace the original contextual conversion commands"), Civilian->IsCultist());
 Civilian->Recruit();
 TestTrue(TEXT("Recruitment joins the cult"), Civilian->IsCultist());
		Civilian->Destroy();
	}
	ADD_LATENT_AUTOMATION_COMMAND(FCultWalkCheck(this, Pawn));
	ADD_LATENT_AUTOMATION_COMMAND(FCultTraversalCheck(this, Pawn));
	return true;
}
#endif
