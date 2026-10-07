// CULT-ULHU anim instance implementation.
#include "Animation/CultUlhuAnimInstance.h"

ECultAnimState UCultUlhuAnimInstance::FromCore(cultulhu::AnimationState S)
{
	using cultulhu::AnimationState;
	switch (S)
	{
	case AnimationState::Idle:     return ECultAnimState::Idle;
	case AnimationState::Walk:     return ECultAnimState::Walk;
	case AnimationState::Run:      return ECultAnimState::Run;
	case AnimationState::Attack:   return ECultAnimState::Attack;
	case AnimationState::Cast:     return ECultAnimState::Cast;
	case AnimationState::Stunned:  return ECultAnimState::Stunned;
	case AnimationState::Death:    return ECultAnimState::Death;
	case AnimationState::Channel:  return ECultAnimState::Channel;
	case AnimationState::CastWave: return ECultAnimState::CastWave;
	case AnimationState::Levitate: return ECultAnimState::Levitate;
	case AnimationState::Launch:   return ECultAnimState::Launch;
	}
	// VERIFY IN EDITOR: add new core states here when the enum grows.
	return ECultAnimState::Idle;
}

void UCultUlhuAnimInstance::SetCoreState(int32 CoreState)
{
	AnimState = FromCore(static_cast<cultulhu::AnimationState>(CoreState));
}

void UCultUlhuAnimInstance::PlayAttackMontage()
{
	if (AttackMontage) Montage_Play(AttackMontage);
}

void UCultUlhuAnimInstance::PlayCastWaveMontage()
{
	if (CastWaveMontage) Montage_Play(CastWaveMontage);
}

void UCultUlhuAnimInstance::PlayLaunchMontage()
{
	if (LaunchMontage) Montage_Play(LaunchMontage);
}

void UCultUlhuAnimInstance::SetVictimLevitated(bool bLevitated)
{
	// VERIFY IN EDITOR: implement via an AnimBP bool or a pose snapshot
	// asset shared by victim characters.
	(void)bLevitated;
}
