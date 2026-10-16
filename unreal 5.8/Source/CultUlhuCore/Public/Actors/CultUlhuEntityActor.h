// CULT-ULHU: generic entity actor.
//
// AActor wrapper around a core cultulhu::Entity. The core object owns all
// simulation state (hp, position, faction); the actor mirrors position to
// the UE transform each tick and forwards damage/death.
//
// For the player avatar and humanoid characters use ACultUlhuCharacter
// instead (ACharacter + CharacterMovementComponent).
//
// Core mapping: src/entities/Entity.h
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"

#include "entities/Entity.h"

#include "CultUlhuEntityActor.generated.h"

UCLASS(Blueprintable)
class CULTULHUCORE_API ACultUlhuEntityActor : public AActor
{
	GENERATED_BODY()

public:
	ACultUlhuEntityActor();

	// Takes ownership of a core entity (std::move it in).
	void PossessCoreEntity(std::unique_ptr<cultulhu::Entity> CoreEntity);

	cultulhu::Entity* CoreEntity() { return CoreEntity_.get(); }
	const cultulhu::Entity* CoreEntity() const { return CoreEntity_.get(); }

	// ---- Replicated presentation state ----
	// The core sim runs on the listen-server host; HP is replicated so
	// clients can drive health bars / death VFX.
	UPROPERTY(ReplicatedUsing = OnRep_CoreHp)
	float ReplicatedHp = 0.0f;

	UFUNCTION()
	void OnRep_CoreHp();

	virtual void Tick(float DeltaSeconds) override;
	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

protected:
	virtual void BeginPlay() override;

private:
	// VERIFY IN EDITOR: std::unique_ptr member in an AActor — fine in
	// practice, but confirm no UHT complaints about the non-UPROPERTY type.
	std::unique_ptr<cultulhu::Entity> CoreEntity_;

	static FVector ToUnreal(const cultulhu::Vec3& V) { return FVector(V.x, V.z, V.y); }
	static cultulhu::Vec3 ToCore(const FVector& V) { return cultulhu::Vec3{ (float)V.X, (float)V.Z, (float)V.Y }; }
};
