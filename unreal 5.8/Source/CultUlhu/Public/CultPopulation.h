#pragma once
#include "CoreMinimal.h"
#include "CultControlProjection.h"
#include "GameFramework/Character.h"
#include "CultPopulation.generated.h"

// Playable prototype population, to be dressed with the owner's character art.
UCLASS()
class CULTULHU_API ACultPopulation : public ACharacter
{
	GENERATED_BODY()
public:
	ACultPopulation();
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
	void Recruit();
 void DieWithCorpse();
 void SummonTo(int32 Faction) { Faction_=Faction; bSummoned_=true; Health_=120; }
 bool IsSummoned() const { return bSummoned_; }
 void Desert() { bCultist_=false; Faction_=-1; Activity_=TEXT("Wander"); OrderTime_=0; bPlayerBattle_=false; }
	void RecruitTo(int32 Faction);
 void TransferTo(int32 Faction) { if(bCultist_&&Faction_==Faction) return; bCultist_=false; RecruitTo(Faction); }
 void ApplyFaithState(float Hp,float Loyalty,bool Lunatic) { Health_=Hp; Loyalty_=Loyalty; bLunatic_=Lunatic; if(Hp<=0) DieWithCorpse(); }
 void ConfigureBred(int32 Faction,float Hp) { SummonTo(Faction); Health_=Hp; CreatureMaxHp_=Hp; }
 bool IsSorcerer() const { return bSorcerer_; }
 void SetSorcerer(bool Value) { bSorcerer_=Value; }
	int32 GetFaction() const { return Faction_; }
	float GetHealth() const { return Health_; }
	float GetLoyalty() const { return Loyalty_; }
	void SetLoyalty(float L, bool Lunatic) { Loyalty_=L; bLunatic_=Lunatic; }
	bool IsLunatic() const { return bLunatic_; }
 bool IsStunned() const { return Control_.Stun; }
	bool IsCaptive() const { return bCaptive_; }
	void Capture(AActor* Escort=nullptr,int32 Faction=-1) { bCaptive_=true; Captor_=Escort; CaptiveFaction_=Faction; }
 int32 GetCaptiveFaction() const { return CaptiveFaction_; }
 void ReleaseCaptive() { bCaptive_=false; Captor_.Reset(); CaptiveFaction_=-1; }
	void Heal(float Amount) { Health_=FMath::Min(bSummoned_?CreatureMaxHp_:bCultist_?100.f:1.f,Health_+Amount); }
	void Order(const FString& Action, FVector Point, AActor* Target = nullptr, float Duration = 60.f, bool bDirective = false);
 bool HasDirective() const { return bPlayerBattle_ || (bDirective_ && OrderTime_>0); }
 FVector GetCommandPoint() const { return OrderPoint_; }
 bool HasPlayerBattle() const { return bPlayerBattle_; }
 void PlayerOrder(const FString& Action,FVector Point,AActor* Target=nullptr,bool Battle=false);
	void SetControlState(bool Stop,bool Fear,float Speed,FVector Origin,bool Stun=false,int32 Source=0) { Control_.Set(Source,Stop,Fear,Speed,Origin,Stun); }
	FString GetActivity() const { return Activity_; }
	void ReceiveAttack(float Damage,int32 Attacker=-1,bool Accounted=false,bool Feral=false);
	bool IsCultist() const { return bCultist_; }
	bool IsAttacking() const { return bAttack_; }
	void SetAttackOrder(bool bAttack) { bAttack_ = bAttack; }
public:
 void ShowContribution(const FString& Text) { Contribution_=Text; ContributionTime_=6; }
 void SetWorkProgress(float P,bool Working) { WorkProgress_=FMath::Clamp(P,0.f,1.f); bWorking_=Working; }
 float WorkProgress() const { return WorkProgress_; }
 int32 HumanPartCount() const { return HumanPoseParts_.IsEmpty()?HumanParts_.Num():HumanPoseParts_.Num(); }
 bool HasAuthoredHumanModel() const { return HumanPose_!=nullptr; }
 float GetMaxHealth() const { return bSummoned_?CreatureMaxHp_:bCultist_?100.f:1.f; }
 FString InspectionActivity() const;
 FString InspectionRole() const;
 FString InspectionContribution() const { return ContributionTime_>0?Contribution_:FString(); }
private:
 friend struct FCultArmyTestAccess;
 UPROPERTY() class UPoseableMeshComponent* HumanPose_=nullptr;
 UPROPERTY() TArray<class UPoseableMeshComponent*> HumanPoseParts_;
 UPROPERTY() TArray<class UStaticMeshComponent*> HumanAccessories_;
 TArray<FTransform> HumanRestPose_; FString HumanSkin_; float HumanPoseTime_=0;
 void RefreshAuthoredHuman();
 void AnimateAuthoredHuman(float Dt);
 UPROPERTY() class USceneComponent* Appearance_=nullptr;
 UPROPERTY() TArray<class UStaticMeshComponent*> HumanParts_;
 UPROPERTY() class UStaticMeshComponent* LeftArm_=nullptr;
 UPROPERTY() class UStaticMeshComponent* RightArm_=nullptr;
 UPROPERTY() class UStaticMeshComponent* LeftLeg_=nullptr;
 UPROPERTY() class UStaticMeshComponent* RightLeg_=nullptr;
 int32 AppearanceFaction_=-99; bool AppearanceCultist_=false;
 FString Contribution_; float ContributionTime_=0,WorkProgress_=0; bool bWorking_=false;
 void CreateHumanAppearance();
 void UpdateHumanAppearance(float Dt);
	UPROPERTY() class UStaticMeshComponent* Body_ = nullptr;
	bool bCultist_ = false;
 bool bSummoned_=false;
	bool bAttack_ = false;
	float Health_ = 1.f;
 float CreatureMaxHp_=120;
 bool bSorcerer_=false;
	float AttackTime_ = 0.f;
	FVector Home_ = FVector::ZeroVector;
	int32 Faction_ = -1;
	float Loyalty_ = 60.f;
	bool bLunatic_ = false;
	bool bCaptive_ = false;
 int32 CaptiveFaction_=-1;
 TWeakObjectPtr<AActor> Captor_;
	FCultControlProjection Control_;
	FString Activity_ = TEXT("Wander");
	FVector OrderPoint_;
	TWeakObjectPtr<AActor> OrderTarget_;
	float OrderTime_ = 0.f;
 bool bDirective_=false;
 bool bPlayerBattle_=false;
	float AmbientTime_ = 0.f;
};
