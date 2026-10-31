// CULT-ULHU player pawn: walkable kaiju with FP/TP camera switching.
//
// ACharacter with a spring-arm camera rig. No input assets required:
// WASD/arrows are polled in Tick, Space jumps, V toggles first/third
// person, Esc returns to the menu. Mouse-look reads raw mouse delta from
// the controller (no axis mappings needed).
//
// The pawn scales to the selected roster entity's canonical height
// (1 unit = 1 m modeling rule); the mesh itself arrives with the owner's
// FBX drops (Docs/CharacterPipeline.md) â€” until then the capsule carries it.
#pragma once

#include "CoreMinimal.h"
#include "CultControlProjection.h"
namespace cultulhu { struct SpellDef; }
#include "GameFramework/Character.h"

#include "CultUlhuPlayerPawn.generated.h"

class USpringArmComponent;
class UCameraComponent;

struct FCultImpactText { FVector Point; float Amount=0,Age=0; bool Killed=false; uint32 TargetId=0; };
enum class ECultPlayerInput : uint8 { Jump, Primary, Heavy, Q, F, R, Interact, Stats, Command, Count };
struct FCultCombatHUDData {
 FString Name,AbilityNames[3],RmbHint; float Health=0,MaxHealth=1,Stamina=0,MaxStamina=100,Cooldowns[3]={},DashCooldown=0; bool Swinging=false;
};
UCLASS(Blueprintable)
class CULTULHU_API ACultUlhuPlayerPawn : public ACharacter
{
	GENERATED_BODY()

public:
	ACultUlhuPlayerPawn();
 // Optional Enhanced Input and zero-asset keys share one physical press/release.
 void RoutePlayerInput(ECultPlayerInput Action,bool Down);
 void SetEnhancedMove(FVector2D Value) { EnhancedMove_=Value; }
 void SetEnhancedSprint(bool Down) { EnhancedSprint_=Down; }

	virtual void Tick(float DeltaSeconds) override;
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;
	FVector FeetLocation() const;
	FRotator AimRotation() const;
	FHitResult CameraAim(float RangeCm = 25000.f, bool bCursor = false) const;
	int32 GetFaction() const { return Faction_; }
	void ConfigureBot(const FString& Id, int32 Faction);
 void SetControlState(bool Stop,bool Fear,float Speed,FVector Origin,bool Stun=false,int32 Source=0) { Control_.Set(Source,Stop,Fear,Speed,Origin,Stun); }
	void SetBotAim(AActor* Target) { BotAim_ = Target; }
 AActor* GetBotAim() const { return BotAim_.Get(); }
	bool IsBot() const { return bBotControlled_; }
	bool IsRmbHeld() const;
	void Notify(const FString& Text);
	FString GetNotice() const { return NoticeTime_>0?Notice_:FString(); }
	float GetMaxHealth() const;
	FVector GetLastEffectLocation() const;
	void ReleaseHeavy();
	void AltPressed();
 void CommandReleased();
	void AltReleased();
	void ToggleStats();
 void TryJump();
 bool TryDash(FVector Direction);
 bool CanDash() const;
 float DashTravelDistance() const;
 void AbilityDash(FVector Direction,float Distance);
 void ResolveSkillHit(const cultulhu::SpellDef& Spell,AActor* Target,FVector Point,int Slot);
 bool IsEvading() const { return EvadeTime_>0; }
 bool IsDashing() const { return DashTime_>0; }
 float BotRangedReach() const;
 bool IsSwinging() const { return SwingTime_>0; }
 FCultCombatHUDData CombatHUD() const;
 void StartMelee(float Damage,float Range=0,float Windup=.12f,float Duration=.20f);
 void HitConfirm() { HitConfirmTime_=.15f; }
 void RecordCombatHit(AActor* Target,FVector Point,float Damage,bool Killed);
 void ConfirmEvade();
 const TArray<FCultImpactText>& ImpactTexts() const { return ImpactTexts_; }
 float KillConfirmTime() const { return KillConfirmTime_; }
 float HurtFeedbackTime() const { return HurtFeedbackTime_; }
 float EvadeConfirmTime() const { return EvadeConfirmTime_; }
 FVector IncomingOrigin() const { return IncomingOrigin_; }
 bool HasIncomingDirection() const { return bIncomingDirection_; }
 AActor* CombatFocus() const { return CombatFocusTime_>0?CombatFocus_.Get():nullptr; }
 bool HasBufferedMelee() const { return MeleeBuffer_>0; }
 float MeleeRecovery() const { return SmashCooldown_; }
 void TickCombatFeel(float Dt);
 void SetMatchDead();
 void RespawnMatch(FVector Feet);
 void RelocateMatch(FVector Feet);
 void MoveMatchToward(FVector Point,float Dt);
 void HealMatch(float Amount);
 bool IsIncapacitated() const { return Control_.Stun||Control_.Fear||bMatchDead_; }
 void MarkCombat();
 void SummonLandedDamage(float Actual);
 void UpdateRecovery(float Dt);
 int HealingAbilitySlot() const;
 float HealingRange() const;
 bool CanHealActor(AActor* Target) const;
 void RecallMatch();
 void RematchConfirm();
 bool MatchDead() const { return bMatchDead_; }

 float HitConfirmTime() const { return HitConfirmTime_; }
	bool StatsVisible() const { return bStats_; }
 void ToggleFaith();
 void FaithScrollUp();
 void FaithScrollDown();

	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Camera")
	void ToggleCamera();

	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Camera")
	bool IsThirdPerson() const { return bThirdPerson_; }

	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Menu")
	void ReturnToMenu();

	// Kaiju ground smash (left mouse): damages destructible buildings in a
	// radius, kicks the camera, plays the shockwave. The test tool for the
	// destruction pass until kit abilities are bridged into UE.
	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Destruction")
	void Smash();
	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Combat")
	void HeavyAttack();
	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Combat")
	void AbilityQ();
	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Combat")
	void AbilityF();
	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Combat")
	void AbilityR();
	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Interaction")
	void Interact();
	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Movement")
	void SetSprinting(bool bSprint);
	void LookYaw(float Value);
	void LookPitch(float Value);
	void ReceiveDamage(float Amount,AActor* Source=nullptr);
	float GetHealth() const;
	FString GetStatusText() const;

	// Small camera nudge (0..1 strength) â€” used by beast swipes.
	UFUNCTION(BlueprintCallable, Category = "CultUlhu|Destruction")
	void AddCameraKick(float Strength);

protected:
 bool InputHeld_[static_cast<int32>(ECultPlayerInput::Count)]={};
 FVector2D EnhancedMove_=FVector2D::ZeroVector;
 bool EnhancedSprint_=false;
	virtual void SetupPlayerInputComponent(UInputComponent* InputComponent) override;

private:
 friend struct FCultCombatFeelTestAccess;
 friend struct FCultMatchTestAccess;
 friend struct FCultTacticsTestAccess;
 friend struct FCultMobaMechanicsTestAccess;
	bool PlaceOnWalkableGround();
	void GuardTerrainBoundary();
	void AreaAttack(FVector Center, float Radius, float Damage);
	void PollMoveKeys(float DeltaSeconds);

	UPROPERTY(VisibleAnywhere, Category = "CultUlhu|Camera")
	USpringArmComponent* SpringArm_ = nullptr;

	UPROPERTY(VisibleAnywhere, Category = "CultUlhu|Camera")
	UCameraComponent* Camera_ = nullptr;
	UPROPERTY() class UStaticMeshComponent* VesselBody_ = nullptr;

	float DashSpeed_=0;
 TArray<FCultImpactText> ImpactTexts_;
 TWeakObjectPtr<AActor> CombatFocus_;
 FVector IncomingOrigin_=FVector::ZeroVector;
 bool bIncomingDirection_=false;
 float MeleeBuffer_=0,KillConfirmTime_=0,HurtFeedbackTime_=0,EvadeConfirmTime_=0,CombatFocusTime_=0,NextImpactSound_=0;
 bool bNextMeleeLeft_=false;
 float DashCooldown_=0,DashTime_=0,EvadeTime_=0,SwingTime_=0,HitConfirmTime_=0;
 FVector DashDirection_=FVector::ZeroVector;
 bool PreviousShift_=false; float ShiftHeldSeconds_=0; FVector ShiftDirection_=FVector::ZeroVector;
 bool ProcessShift(bool Down,FVector Direction,float Dt,bool Allowed=true);
 bool bThirdPerson_ = true;
	float CharScale_ = 1.0f; // actor scale for the selected entity's height
	FBox TerrainBounds_ = FBox(ForceInit);
	FVector LastSafeLocation_ = FVector::ZeroVector;
	FVector HomeLocation_ = FVector::ZeroVector;
	float WalkSpeed_ = 600.f;
 float LastCombatAt_=-100.f;
	float Health_ = 1000.f;
	class FCultKitRuntime* Kit_ = nullptr;
	int32 Faction_ = 0;
 bool bBotControlled_=false,bMatchDead_=false;
	FString CharacterId_;
	TWeakObjectPtr<AActor> BotAim_;
	FString Notice_; float NoticeTime_=0;
	bool bStats_ = false;
 FCultControlProjection Control_;
	bool bRmbHeld_ = false;
	float HeavyCooldown_ = 0.f;
	float QCooldown_ = 0.f;
	float FCooldown_ = 0.f;
	float RCooldown_ = 0.f;
	float GroundCheckTime_ = 0.f;

	// Destruction pass: smash cooldown + manual camera kick.
	float SmashCooldown_ = 0.0f;
	float ShakeTime_ = 0.0f;
	const float ShakeDur_ = 0.35f;
};
