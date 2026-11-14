#include "CultCharacterMovement.h"

void UCultCharacterMovement::ControlledCharacterMove(const FVector& InputVector,float DeltaTime) {
 TGuardValue<FVector> Input(JumpInput,InputVector);
 Super::ControlledCharacterMove(InputVector,DeltaTime);
}

void UCultCharacterMovement::MoveAutonomous(float ClientTimeStamp,float DeltaTime,uint8 CompressedFlags,const FVector& NewAccel) {
 TGuardValue<FVector> Input(JumpInput,NewAccel);
 Super::MoveAutonomous(ClientTimeStamp,DeltaTime,CompressedFlags,NewAccel);
}

bool UCultCharacterMovement::DoJump(bool bReplayingMoves,float DeltaTime) {
 const bool Grounded=IsMovingOnGround();
 if(!Super::DoJump(bReplayingMoves,DeltaTime)) return false;
 const FVector Direction=JumpInput.GetSafeNormal2D();
 if(Grounded&&!Direction.IsNearlyZero()) {
  // A stair may have stopped ground velocity just before the jump.
  // Use the movement input carried by the saved move on both peers.
  const double LaunchSpeed=FMath::Max(Velocity.Size2D(),GetMaxSpeed()*.85);
  Velocity.X=Direction.X*LaunchSpeed;
  Velocity.Y=Direction.Y*LaunchSpeed;
 }
 return true;
}
