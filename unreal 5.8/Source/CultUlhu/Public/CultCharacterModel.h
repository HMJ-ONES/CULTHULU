#pragma once
#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "CultCharacterModel.generated.h"

class UMeshComponent;
class USkeletalMeshComponent;
class UAnimSequence;
class ACultUlhuPlayerPawn;

// Render-only approved source art. The owning pawn retains all movement and combat collision.
UCLASS()
class CULTULHU_API ACultCharacterModel : public AActor
{
 GENERATED_BODY()
public:
 ACultCharacterModel();
 bool Initialize(const FString& Id, bool Preview=false);
 static ACultCharacterModel* AttachToPawn(ACultUlhuPlayerPawn* Pawn,const FString& Id);
 static FString PortraitPath(const FString& Id);
 static FString SourceCredit(const FString& Id);
 static bool HasSourceAnimation(const FString& Id);
 static bool UsesMeleeStep(ACultUlhuPlayerPawn* Pawn);
 virtual void Tick(float DeltaSeconds) override;
 static void Cue(ACultUlhuPlayerPawn* Pawn,const FString& Role,float Seconds=.55f,float Age=0);
 FString AnimationRole() const { return CurrentRole_; }
 void PreviewMotion(const FString& MotionRole) { if(bPreview_) PlayRole(MotionRole); }
 const TArray<TObjectPtr<UMeshComponent>>& Parts() const { return Parts_; }
private:
 UPROPERTY() TArray<TObjectPtr<UMeshComponent>> Parts_;
 UPROPERTY() TMap<FString,TObjectPtr<UAnimSequence>> Animations_;
 FString CurrentRole_,ActionRole_;
 float ActionUntil_=0,LandingUntil_=0,WalkSpeed_=0; bool WasFalling_=false;
 float ActionSeconds_=0;
 float MeleeContactPhase_=.5f;
 UPROPERTY() ACultUlhuPlayerPawn* Pawn_=nullptr;
 bool bPreview_=false;
 bool bFloating_=false;
 bool bMeleeStep_=false;
 bool bOriginalOrb_=false;
 float RoleBegan_=0;
 float Elapsed_=0,Hover_=0;
 FString CharacterId_;
 float GuardWeight_=0, GaitRate_=1;
 bool bRunning_=false;
 TArray<FTransform> StaticRest_;
 void UpdateCombatGesture(float DeltaSeconds);
 void PlayRole(const FString& Role);
 void UpdateOriginalOrb();
 float AuthoredMeleePhase(float Progress) const;
};
