// CULT-ULHU: character Data Asset.
//
// UE-side mirror of the core CharacterDef (src/characters/CharacterDef.h).
// Produced by the character pipeline: assets/characters/<name>/character.def
// -> this asset (see Docs/CharacterPipeline.md). The game reads stats from
// here; ACultUlhuCharacter::ToCoreDef() converts back to the core struct.
//
// Field-for-field mapping is validated by unreal/Tests/validate_binding.py.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"

#include "characters/CharacterDef.h"

#include "CultCharacterData.generated.h"

class USkeletalMesh;
class UAnimInstance;

USTRUCT(BlueprintType)
struct FCultSpellData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu")
	FString SpellId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu")
	FString Name;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu")
	FString Flavor;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu")
	float CooldownSec = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu")
	float StaminaCost = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu")
	float ManaCost = 0.0f;

	// Effect kind understood by the game module, e.g. "aoe_damage",
	// "fear_aura", "summon", "buff". Keep the vocabulary small.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu")
	FString EffectKind;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu")
	float EffectPower = 0.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu")
	float Range = 0.0f;

	cultulhu::SpellDef ToCoreDef() const;
};

UENUM(BlueprintType)
enum class ECultHeavyAttackKind : uint8
{
	MeleeHeavy   UMETA(DisplayName = "MeleeHeavy"),
	MindControl  UMETA(DisplayName = "MindControl"),
	AcidSpit     UMETA(DisplayName = "AcidSpit"),
	EldritchGrasp UMETA(DisplayName = "EldritchGrasp"),
};

USTRUCT(BlueprintType)
struct FCultHeavyAttackData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu")
	ECultHeavyAttackKind Kind = ECultHeavyAttackKind::MeleeHeavy;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu")
	FString Name;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu")
	float DamageMult = 1.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu")
	float Range = 2.0f;

	// CC type name ("Stun","Slow","Root","Fear"); empty = none.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu")
	FString CcType;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu")
	float CcSeconds = 0.0f;

	cultulhu::HeavyAttackDef ToCoreDef() const;
};

UCLASS(BlueprintType)
class CULTULHUCORE_API UCultCharacterData : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	// Registry key, e.g. "cthulhu_avatar". Must match character.def id.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu")
	FString CharacterId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu")
	FString DisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu", meta = (MultiLine = true))
	FString Flavor;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu|Vitals")
	float MaxHp = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu|Vitals")
	float MoveSpeed = 5.0f; // m/s; converted to cm/s on apply

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu|Vitals")
	float MaxStamina = 100.0f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu|Kit")
	FCultSpellData QAbility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu|Kit")
	FCultSpellData FAbility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu|Kit")
	FCultSpellData RAbility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu|Kit")
	FString MeleeComboId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu|Kit")
	FCultHeavyAttackData RightClick;

	// Optional state-machine RMB kit id, e.g. "wave_of_domination".
	// When non-empty, the game builds the ability via the core's
	// createRmbAbility() factory instead of the plain HeavyAttackDef numbers.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu|Kit")
	FString RmbAbilityId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu|Kit")
	FString PassiveId;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu|Kit", meta = (MultiLine = true))
	FString PassiveDesc;

	// Skeletal mesh + AnimBP assigned in editor per character.
	// VERIFY IN EDITOR: mesh skeleton matches the rig map for this package.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu|Presentation")
	USkeletalMesh* SkeletalMesh = nullptr;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "CultUlhu|Presentation")
	TSubclassOf<UAnimInstance> AnimBlueprint;

	cultulhu::CharacterDef ToCoreDef() const;
};
