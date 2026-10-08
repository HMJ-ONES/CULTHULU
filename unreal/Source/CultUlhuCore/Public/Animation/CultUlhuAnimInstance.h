// CULT-ULHU animation instance skeleton.
//
// Bridges the core AnimationStateMachine (src/animation) to UE's
// UAnimInstance. The game logic requests core states; this class mirrors
// them into an AnimGraph state machine (authored in the AnimBP) and
// exposes one-shot montage hooks for attacks/casts.
//
// Core mapping: src/animation/AnimationStateMachine.h
// Retargeting: Docs/CharacterPipeline.md (Mixamo -> IK Retargeter)
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"

#include "animation/AnimationStateMachine.h"

#include "CultUlhuAnimInstance.generated.h"

// Mirrors cultulhu::AnimationState for the AnimGraph.
// VERIFY IN EDITOR: keep in sync with the core enum (incl. CastWave,
// Levitate, Launch, Levitated for the Wave of Domination).
UENUM(BlueprintType)
enum class ECultAnimState : uint8
{
	Idle     UMETA(DisplayName = "Idle"),
	Walk     UMETA(DisplayName = "Walk"),
	Run      UMETA(DisplayName = "Run"),
	Attack   UMETA(DisplayName = "Attack"),
	Cast     UMETA(DisplayName = "Cast"),
	Stunned  UMETA(DisplayName = "Stunned"),
	Death    UMETA(DisplayName = "Death"),
	Channel  UMETA(DisplayName = "Channel"),
	CastWave UMETA(DisplayName = "CastWave"),
	Levitate UMETA(DisplayName = "Levitate"),
	Launch   UMETA(DisplayName = "Launch"),
	Levitated UMETA(DisplayName = "Levitated"), // victim: floating, held
};

UCLASS(Blueprintable)
class CULTULHUCORE_API UCultUlhuAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

public:
	// Current mirrored state; the AnimBP state machine reads this.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CultUlhu|Anim")
	ECultAnimState AnimState = ECultAnimState::Idle;

	// Push a core animation state into the instance.
	// VERIFY IN EDITOR: the AnimBP has states for every ECultAnimState
	// value, with transitions driven by AnimState.
	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Anim")
	void SetCoreState(int32 CoreState);

	// One-shot hooks. Bind these to AnimMontages in the AnimBP.
	// VERIFY IN EDITOR: montages exist per character; slots named below.
	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Anim")
	void PlayAttackMontage();

	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Anim")
	void PlayCastWaveMontage();

	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Anim")
	void PlayLaunchMontage();

	// Victim-side hook: the levitated-victim animation.
	// VERIFY IN EDITOR: victim skeletal meshes use a compatible AnimBP or
	// a shared "Levitated" pose snapshot.
	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Anim")
	void SetVictimLevitated(bool bLevitated);

protected:
	UPROPERTY(EditDefaultsOnly, Category = "CultUlhu|Anim")
	UAnimMontage* AttackMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "CultUlhu|Anim")
	UAnimMontage* CastWaveMontage = nullptr;

	UPROPERTY(EditDefaultsOnly, Category = "CultUlhu|Anim")
	UAnimMontage* LaunchMontage = nullptr;

private:
	static ECultAnimState FromCore(cultulhu::AnimationState S);
};
