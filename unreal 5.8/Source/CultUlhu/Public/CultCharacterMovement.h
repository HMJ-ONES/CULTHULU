#pragma once

#include "GameFramework/CharacterMovementComponent.h"
#include "CultCharacterMovement.generated.h"

UCLASS()
class UCultCharacterMovement : public UCharacterMovementComponent {
 GENERATED_BODY()
public:
 bool DoJump(bool bReplayingMoves,float DeltaTime) override;
 void MoveAutonomous(float ClientTimeStamp,float DeltaTime,uint8 CompressedFlags,const FVector& NewAccel) override;
protected:
 void ControlledCharacterMove(const FVector& InputVector,float DeltaTime) override;
private:
 FVector JumpInput=FVector::ZeroVector;
};
