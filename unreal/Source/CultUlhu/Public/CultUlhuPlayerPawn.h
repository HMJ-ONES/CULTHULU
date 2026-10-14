// CULT-ULHU player pawn: walkable kaiju with FP/TP camera switching.
//
// ACharacter with a spring-arm camera rig. No input assets required:
// WASD/arrows are polled in Tick, Space jumps, V toggles first/third
// person, Esc returns to the menu. Mouse-look reads raw mouse delta from
// the controller (no axis mappings needed).
//
// The pawn scales to the selected roster entity's canonical height
// (1 unit = 1 m modeling rule); the mesh itself arrives with the owner's
// FBX drops (Docs/CharacterPipeline.md) — until then the capsule carries it.
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"

#include "CultUlhuPlayerPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;

UCLASS(Blueprintable)
class CULTULHU_API ACultUlhuPlayerPawn : public ACharacter
{
	GENERATED_BODY()

public:
	ACultUlhuPlayerPawn();

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Camera")
	void ToggleCamera();

	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Camera")
	bool IsThirdPerson() const { return bThirdPerson_; }

	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Menu")
	void ReturnToMenu();

protected:
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

private:
	void PollMoveKeys(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere, Category = "CultUlhu|Camera")
	USpringArmComponent* SpringArm_ = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "CultUlhu|Camera")
	UCameraComponent* Camera_ = nullptr;

	bool bThirdPerson_ = true;
	float CharScale_ = 1.0f; // actor scale for the selected entity's height
};
