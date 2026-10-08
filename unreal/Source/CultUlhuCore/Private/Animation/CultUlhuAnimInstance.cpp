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
	case AnimationState::Levitated: return ECultAnimState::Levitated;
	case AnimationState::FearRun:  return ECultAnimState::FearRun;
	case AnimationState::Brawl:    return ECultAnimState::Brawl;
	case AnimationState::SacrificePerformer:
		return ECultAnimState::SacrificePerformer;
	case AnimationState::SacrificeVictim:
		return ECultAnimState::SacrificeVictim;
	case AnimationState::Maul:     return ECultAnimState::Maul;
	case AnimationState::WarBattle: return ECultAnimState::WarBattle;
	case AnimationState::Build:    return ECultAnimState::Build;
	case AnimationState::Repair:   return ECultAnimState::Repair;
	default:
		// VERIFY IN EDITOR: add new core states here when the enum grows.
		// WarBattle/Build/Repair added wave 18: the AnimBP needs matching
		// looping states for each (see SetCoreState VERIFY note).
		return ECultAnimState::Idle;
	}
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
