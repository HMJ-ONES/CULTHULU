#include "CultUlhuPlayerPawn.h"
#include "CultCharacterModel.h"

void ACultUlhuPlayerPawn::MulticastMotionCue_Implementation(uint8 Kind,uint8 Combo,bool Retreat,float Seconds) {
 if(!FMath::IsFinite(Seconds)||Seconds<=0||Seconds>5) return;
 FString MotionRole;
 if(Kind==0) MotionRole=FString::Printf(TEXT("%s%d"),Retreat?TEXT("melee_back_"):TEXT("melee_"),FMath::Clamp(int32(Combo),1,3));
 else if(Kind==1) MotionRole=TEXT("dash");
 else if(Kind==2) MotionRole=TEXT("taunt");
 else if(Kind==3) MotionRole=TEXT("primary_fire");
 else if(Kind==4) MotionRole=TEXT("rock_lift");
 else if(Kind==5) MotionRole=TEXT("rock_throw");
 else if(Kind==6) MotionRole=Combo==3?TEXT("cast_heavy"):TEXT("cast_light");
 else return;
 VesselCueAt_=GetWorld()->GetTimeSeconds();
 VesselCueDuration_=Seconds;
 VesselCue_=Kind;
 ACultCharacterModel::Cue(this,MotionRole,Seconds);
}
