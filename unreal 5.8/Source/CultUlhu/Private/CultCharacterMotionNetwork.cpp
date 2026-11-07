#include "CultUlhuPlayerPawn.h"
#include "CultCharacterModel.h"
#include "CultAbilityFX.h"
#include "GameFramework/GameStateBase.h"

void ACultUlhuPlayerPawn::MulticastMotionCue_Implementation(uint8 Kind,float Seconds) {
 if(!FMath::IsFinite(Seconds)||Seconds<=0||Seconds>5) return;
 if(Kind!=1&&Kind!=2) return;
 AttackPlayback_={};
 ACultCharacterModel::Cue(this,Kind==1?TEXT("dash"):TEXT("taunt"),Seconds);
}

void ACultUlhuPlayerPawn::PlayAttackMotion(CultAttackMotion::EMove Move,float Contact,float Seconds,uint8 Variant) {
 if(!HasAuthority()||IsActorBeingDestroyed()) return;
 const auto* State=GetWorld()->GetGameState();
 const float Now=State?State->GetServerWorldTimeSeconds():GetWorld()->GetTimeSeconds();
 MulticastAttackMotion(uint8(Move),Variant,Contact,Seconds,Now);
}

void ACultUlhuPlayerPawn::MulticastAttackMotion_Implementation(uint8 Move,uint8 Variant,float Contact,float Seconds,float ServerStart) {
 using namespace CultAttackMotion;
 if(Move>=uint8(EMove::Count)||!FMath::IsFinite(Contact)||!FMath::IsFinite(Seconds)||!FMath::IsFinite(ServerStart)) return;
 if(Contact<0||Seconds<.12f||Seconds>5||Contact>Seconds-.06f) return;
 const float Now=GetWorld()->GetTimeSeconds();
 const auto* State=GetWorld()->GetGameState();
 const float Age=State?FMath::Clamp(float(State->GetServerWorldTimeSeconds()-ServerStart),0.f,.35f):0.f;
 const auto AttackMove=EMove(Move);
 AttackPlayback_={AttackMove,Variant,Now-Age,Contact,Seconds,AttackFrame()};
 if(IsHold(AttackMove)) HeldAttack_=AttackPlayback_;
 else if(AttackMove==EMove::None||AttackMove==EMove::Lower||AttackMove==EMove::RockThrow||
         AttackMove==EMove::MinionThrow||AttackMove==EMove::WaveThrow) HeldAttack_={};
 FString MotionRole=TEXT("attack_overlay");
 if(AttackMove==EMove::Melee) {
  MotionRole=FString::Printf(TEXT("%s%d"),(Variant&4)?TEXT("melee_back_"):TEXT("melee_"),FMath::Clamp(int32(Variant&3),1,3));
 }
 ACultCharacterModel::Cue(this,MotionRole,Seconds,Age);
 CultAbilityFX::CastCue(this,CharacterId_,Move,Contact,Age);
}

void ACultUlhuPlayerPawn::EmitAbilityBurst(uint8 Look,uint8 Event,FVector Location,FVector Heading,float Size) {
 if(HasAuthority()&&Look!=0) MulticastAbilityBurst(Look,Event,Location,Heading.GetSafeNormal(),Size);
}

void ACultUlhuPlayerPawn::MulticastAbilityBurst_Implementation(uint8 Look,uint8 Event,FVector_NetQuantize Location,
                                                             FVector_NetQuantizeNormal Heading,float Size) {
 using namespace CultAbilityFX;
 ACultAbilityFX::Spawn(GetWorld(),ELook(Look),EEvent(Event),Location,Heading,Size);
}

CultAttackMotion::FFrame ACultUlhuPlayerPawn::AttackFrame() const {
 using namespace CultAttackMotion;
 if(IsIncapacitated()||Control_.Stop) return {};
 if(IsDashing()&&!IsSurge(CharacterId_,AttackPlayback_.Move)) return {};
 const float Now=GetWorld()->GetTimeSeconds();
 const auto Active=Sample(CharacterId_,AttackPlayback_,Now);
 if(HeldAttack_.Move==EMove::None||IsHold(AttackPlayback_.Move)) return Active;
 const float UntilEnd=AttackPlayback_.Began+AttackPlayback_.Duration-Now;
 const float ReturnWeight=1.f-FMath::SmoothStep(0.f,.16f,UntilEnd);
 return Blend(Active,Sample(CharacterId_,HeldAttack_,Now),ReturnWeight);
}
