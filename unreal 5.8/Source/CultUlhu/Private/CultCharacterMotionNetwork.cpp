#include "CultUlhuPlayerPawn.h"
#include "CultCharacterModel.h"

void ACultUlhuPlayerPawn::MulticastMotionCue_Implementation(uint8 Kind,uint8 Combo,bool Retreat,float Seconds) {
 if(!FMath::IsFinite(Seconds)||Seconds<=0||Seconds>5) return;
 FString MotionRole;
 if(Kind==0) MotionRole=FString::Printf(TEXT("%s%d"),Retreat?TEXT("melee_back_"):TEXT("melee_"),FMath::Clamp(int32(Combo),1,3));
 else if(Kind==1) MotionRole=TEXT("dash");
 else if(Kind==2) MotionRole=TEXT("taunt");
 else return;
 ACultCharacterModel::Cue(this,MotionRole,Seconds);
}
