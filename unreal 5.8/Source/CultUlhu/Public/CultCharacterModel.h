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
 virtual void Tick(float DeltaSeconds) override;
 static void Cue(ACultUlhuPlayerPawn* Pawn,const FString& Role,float Seconds=.55f);
 FString AnimationRole() const { return CurrentRole_; }
 void PreviewMotion(const FString& MotionRole) { if(bPreview_) PlayRole(MotionRole); }
 const TArray<TObjectPtr<UMeshComponent>>& Parts() const { return Parts_; }
private:
 UPROPERTY() TArray<TObjectPtr<UMeshComponent>> Parts_;
 UPROPERTY() TMap<FString,TObjectPtr<UAnimSequence>> Animations_;
 FString CurrentRole_,ActionRole_;
 float ActionUntil_=0,LandingUntil_=0,WalkSpeed_=0; bool WasFalling_=false;
 UPROPERTY() ACultUlhuPlayerPawn* Pawn_=nullptr;
 bool bPreview_=false;
 bool bFloating_=false;
 float Elapsed_=0,Hover_=0;
 void PlayRole(const FString& Role);
};
